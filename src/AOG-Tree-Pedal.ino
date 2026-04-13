#include <Arduino.h>
// Status LED's
#define GGAReceivedLED 13        // Teensy onboard LED
#define Power_on_LED 5           // Red
#define Ethernet_Active_LED 6    // Green
#define GPSRED_LED 9             // Red (Flashing = NO IMU or Dual, ON = GPS fix with IMU)
#define GPSGREEN_LED 10          // Green (Flashing = Dual bad, ON = Dual good)
#define AUTOSTEER_STANDBY_LED 11 // Red
#define AUTOSTEER_ACTIVE_LED 12  // Green

#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <EEPROM.h>

const uint8_t LOOP_TIME = 200; // 5hz
uint32_t lastTime = LOOP_TIME;
uint32_t currentTime = LOOP_TIME;
uint8_t watchdogTimer = 20; // make sure we are talking to AOG

// Ram section   ***********************************************************************************************************
uint8_t currentState = 1, reading, previous = 0;
uint8_t lastSteerSwitch = 0;
int8_t sensorReading = 0;
bool engageBrake = false;
bool movingRam = false;
uint32_t ramStartTime = millis();
byte Sections = 0;
enum _RAMState
{
  Push,
  Retract,
  Stop
};
_RAMState ramState = Stop;
#define Cytron_PWM 2
#define Cytron_DIR 4
#define goButton 34
#define CURRENT_SENSOR_PIN A17

void driveRAM(_RAMState state)
{
  static _RAMState lastState = _RAMState::Stop;

  if (state != lastState)
  {
    // Serial.print("RAM state -> ");
    // if (state == _RAMState::Push)
    //   Serial.print("Push");
    // else if (state == _RAMState::Retract)
    //   Serial.print("Retract");
    // else
    //   Serial.print("Stop");

    // Serial.print(" at ms=");
    // Serial.println(millis());
    lastState = state;
  }

  if (state == _RAMState::Push)
  {
    // digitalWrite(Power_on_LED, LOW);
    digitalWriteFast(Cytron_DIR, HIGH);
    analogWrite(Cytron_PWM, 255);
  }
  else if (state == _RAMState::Retract)
  {
    // digitalWrite(Power_on_LED, LOW);
    digitalWriteFast(Cytron_DIR, LOW);
    analogWrite(Cytron_PWM, 255);
  }
  else
  {
    digitalWrite(Ethernet_Active_LED, LOW);
    analogWrite(Cytron_PWM, 0);
  }
}
// end of RAM section ***********************************************************************************************************

struct ConfigIP
{
  uint8_t ipOne = 192;
  uint8_t ipTwo = 168;
  uint8_t ipThree = 5;
};
ConfigIP networkAddress; // 3 bytes

struct Config
{
  uint8_t ramMaxTime = 100; // multiplied by 10 to get milliseconds, so max time is 2550 msec
  uint8_t currentCutOff = 12; // multiplied by 10 to get value
  uint8_t sectionMask = 15;
};
Config aogConfig; // 4 bytes

// IP & MAC address of this module of this module
byte Eth_myip[4] = {0, 0, 0, 0}; // This is now set via AgIO
byte mac[] = {0x00, 0x00, 0x56, 0x00, 0x00, 0x79};

unsigned int portMy = 5123;          // port of this module
unsigned int portDestination = 9999; // Port of AOG that listens
unsigned int AOGPort = 8888;         // port machine data from AOG comes in

EthernetUDP Eth_udpMachineBoard; // In & Out Port 8888
IPAddress Eth_BroadCastDestination;

// Used to set CPU speed
extern "C" uint32_t set_arm_clock(uint32_t frequency); // required prototype

uint8_t PGN_237[] = {0x80, 0x81, 0x7f, 237, 8, 1, 2, 3, 4, 0, 0, 0, 0, 0xCC};
int8_t PGN_237_Size = sizeof(PGN_237) - 1;

byte ackPacket[72] = {0xB5, 0x62, 0x01, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t helloFromMachine[] = {128, 129, 123, 123, 5, 0, 0, 0, 0, 0, 71};
uint8_t machineUdpData[UDP_TX_PACKET_MAX_SIZE]; // Buffer For Receiving UDP Data

bool Ethernet_running = false; // Auto set on in ethernet setup

#include "UDP.ino"
#include "zEthernet.ino"

int16_t EEread = 0;
#define EEP_Ident 1309

// Setup procedure ------------------------
void setup()
{
  delay(5000);               // Small delay so serial can monitor start up
  set_arm_clock(150000000); // Set CPU speed to 150mhz

  pinMode(GGAReceivedLED, OUTPUT);
  pinMode(Power_on_LED, OUTPUT);
  pinMode(Ethernet_Active_LED, OUTPUT);
  pinMode(GPSRED_LED, OUTPUT);
  pinMode(GPSGREEN_LED, OUTPUT);
  pinMode(AUTOSTEER_STANDBY_LED, OUTPUT);
  pinMode(AUTOSTEER_ACTIVE_LED, OUTPUT);
  pinMode(Cytron_PWM, OUTPUT);
  pinMode(Cytron_DIR, OUTPUT);
  pinMode(goButton, INPUT_PULLUP);
  pinMode(CURRENT_SENSOR_PIN, INPUT_DISABLE);

  digitalWrite(GPSRED_LED, LOW);
  digitalWrite(GPSGREEN_LED, HIGH);
  Serial.begin(115200);
  delay(10);
  Serial.println("Start setup");

  EEPROM.get(0, EEread); // read identifier

  if (EEread != EEP_Ident) // check on first start and write EEPROM
  {
    EEPROM.put(0, EEP_Ident);
    EEPROM.put(10, networkAddress);
    EEPROM.put(20, aogConfig);
    Serial.println("Writing default EEPROM settings");
  }
  else
  {
    EEPROM.get(10, networkAddress); // read the Settings
    EEPROM.get(20, aogConfig);     // read the Settings
  }
  
  Serial.println("\r\nStarting Ethernet...");
  EthernetStart();
  
  Serial.println("ramMaxTime: " + String(aogConfig.ramMaxTime * 10) + " currentCutOff: " + String(aogConfig.currentCutOff * 10) + " sectionMask: " + String(aogConfig.sectionMask));
  Serial.println("\r\nEnd setup, waiting for GPS...\r\n");
}

void loop()
{
  float sensorSample = (float)analogRead(CURRENT_SENSOR_PIN);
  sensorSample = (abs(775 - sensorSample)) * 0.5;
  sensorReading = sensorReading * 0.7 + sensorSample * 0.3;
  sensorReading = min(sensorReading, 255);
  if (sensorReading > aogConfig.currentCutOff * 10) // current cutoff, make this variable
  {
    Serial.println("Current sensor reading: " + String(sensorReading) + " so cutting off!");
    movingRam = false;
    digitalWrite(Power_on_LED, HIGH);
    ramState = _RAMState::Stop;
    driveRAM(ramState);
  }
  if (movingRam && (millis() - ramStartTime >= (aogConfig.ramMaxTime * 10)))
  {
    movingRam = false;
    ramState = _RAMState::Stop;
    driveRAM(ramState);
  }
  // Loop triggers every 200 msec and sends back gyro heading, and roll, steer angle etc
  if (digitalRead(goButton) == LOW && engageBrake)
  {
    engageBrake = false;
    ramState = _RAMState::Retract;
    ramStartTime = millis();
    movingRam = true;
    driveRAM(ramState);
  }
  currentTime = millis();

  if (currentTime - lastTime >= LOOP_TIME)
  {
    lastTime = currentTime;

    // If connection lost to AgOpenGPS, the watchdog will count up
    if (watchdogTimer++ > 250)
      watchdogTimer = 20;

    // checksum
    int16_t CK_A = 0;
    for (uint8_t i = 2; i < PGN_237_Size; i++)
    {
      CK_A = (CK_A + PGN_237[i]);
    }
    PGN_237[PGN_237_Size] = CK_A;

    // off to AOG
    SendUdp(helloFromMachine, sizeof(helloFromMachine), Eth_BroadCastDestination, portDestination);

  } // end of timed loop

  ReceiveUdp();
}