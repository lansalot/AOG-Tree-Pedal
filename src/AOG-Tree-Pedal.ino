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
bool engageBrake = false;
bool movingRam = false;
uint16_t ramMaxTime = 1000; // milliseconds to push/pull ram
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
#define goButton 34

void driveRAM(_RAMState state)
{
  if (state == _RAMState::Push)
  {
    bitSet(PORTD, 4); // set the correct direction
    analogWrite(Cytron_PWM, 255);
  }
  else if (state == _RAMState::Retract)
  {
    bitClear(PORTD, 4);
    analogWrite(Cytron_PWM, 255);
  }
  else
  {
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
  delay(500);               // Small delay so serial can monitor start up
  set_arm_clock(150000000); // Set CPU speed to 150mhz

  pinMode(GGAReceivedLED, OUTPUT);
  pinMode(Power_on_LED, OUTPUT);
  pinMode(Ethernet_Active_LED, OUTPUT);
  pinMode(GPSRED_LED, OUTPUT);
  pinMode(GPSGREEN_LED, OUTPUT);
  pinMode(AUTOSTEER_STANDBY_LED, OUTPUT);
  pinMode(AUTOSTEER_ACTIVE_LED, OUTPUT);
  pinMode(goButton, INPUT_PULLUP);

  delay(10);
  Serial.begin(115200);
  delay(10);
  Serial.println("Start setup");

  EEPROM.get(0, EEread); // read identifier

  if (EEread != EEP_Ident) // check on first start and write EEPROM
  {
    EEPROM.put(0, EEP_Ident);
    EEPROM.put(10, networkAddress);
    Serial.println("Writing default EEPROM settings");
  }
  else
  {
    EEPROM.get(10, networkAddress); // read the Settings
    Serial.println("EEPROM settings loaded");
  }

  Serial.println("\r\nStarting Ethernet...");
  EthernetStart();

  Serial.println("\r\nEnd setup, waiting for GPS...\r\n");
}

void loop()
{
  if (millis() - ramStartTime >= ramMaxTime) {
    driveRAM(_RAMState::Stop);
  }
  // Loop triggers every 200 msec and sends back gyro heading, and roll, steer angle etc
  if (digitalRead(goButton) == LOW && engageBrake)
  {
    Serial.println("Button pressed, disabling breaking and starting ram");
    engageBrake = false;
    ramStartTime = millis();
    driveRAM(_RAMState::Retract);
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