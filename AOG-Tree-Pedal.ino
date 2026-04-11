
   /*
    * This program only turns the relays for section control
    * On and Off. Connect to the Relay Port in AgOpenGPS
    * 
    * Hydraulic Raise D4
    * Hydraulic Lower D3
    * 
    * Tram Right Side D5
    * Tram left Side D6
    * 
    * Section 0 to 5 -- D7 to D12
    * 
    */
    
  //loop time variables in microseconds

  #include <EEPROM.h> 
  #define EEP_Ident 0x5400  

    //Program counter reset
    void(* resetFunc) (void) = 0;

  //Variables for config - 0 is false  
    struct Config {
        uint8_t raiseTime = 2;
        uint8_t lowerTime = 4;
        uint8_t enableToolLift = 0;
        uint8_t isRelayActiveHigh = 0; //if zero, active low (default)

        uint8_t user1 = 0; //user defined values set in machine tab
        uint8_t user2 = 0;
        uint8_t user3 = 0;
        uint8_t user4 = 0;

    };  Config aogConfig;   //4 bytes

    /*
    * Functions as below assigned to pins
    0: -
    1 thru 16: Section 1,Section 2,Section 3,Section 4,Section 5,Section 6,Section 7,Section 8, 
                Section 9, Section 10, Section 11, Section 12, Section 13, Section 14, Section 15, Section 16, 
    17,18    Hyd Up, Hyd Down, 
    19 Tramline, 
    20: Geo Stop
    21,22,23 - unused so far
    */

    bool engageBrake = false;

    //24 possible pins assigned to these functions
    uint8_t pin[] = { 1,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

    //read value from Machine data and set 1 or zero according to list
    uint8_t relayState[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

  const uint8_t LOOP_TIME = 200; //5hz
  uint32_t lastTime = LOOP_TIME;
  uint32_t currentTime = LOOP_TIME;
  uint32_t fifthTime = 0;
  uint16_t count = 0;

  //Comm checks
  uint8_t watchdogTimer = 0; //make sure we are talking to AOG
  uint8_t serialResetTimer = 0; //if serial buffer is getting full, empty it

  bool isRaise = false;
  bool isLower = false;
  
   //Communication with AgOpenGPS
  int16_t temp, EEread = 0;

   //Parsing PGN
  bool isPGNFound = false, isHeaderFound = false;
  uint8_t pgn = 0, dataLength = 0, idx = 0;
  int16_t tempHeader = 0;
  
  uint8_t AOG[] = {0x80,0x81, 0x7f, 0xED, 8, 0, 0, 0, 0, 0,0,0,0, 0xCC };

  //The variables used for storage
  uint8_t relayHi=0, relayLo = 0, tramline = 0, uTurn = 0, hydLift = 0, geoStop = 0;
  float gpsSpeed;
  
  uint8_t raiseTimer = 0, lowerTimer = 0, lastTrigger = 0;
 
  void setup()
  {
      //set the baud rate
      Serial.begin(38400);
      //while (!Serial) { ; } // wait for serial port to connect. Needed for native USB

      //set the pins to be outputs (pin numbers)
      pinMode(LED_BUILTIN, OUTPUT);
  }

  void loop()
  {
      //Loop triggers every 200 msec and sends back gyro heading, and roll, steer angle etc

      currentTime = millis();

      if (currentTime - lastTime >= LOOP_TIME)
      {
          lastTime = currentTime;

          //If connection lost to AgOpenGPS, the watchdog will count up 
          if (watchdogTimer++ > 250) watchdogTimer = 12;

          //clean out serial buffer to prevent buffer overflow
          if (serialResetTimer++ > 20)
          {
              while (Serial.available() > 0) Serial.read();
              serialResetTimer = 0;
          }


          //hydraulic lift


      } //end of timed loop

      // Serial Receive
      //Do we have a match with 0x8081?    
      if (Serial.available() > 4 && !isHeaderFound && !isPGNFound)
      {
          uint8_t temp = Serial.read();
          if (tempHeader == 0x80 && temp == 0x81)
          {
              isHeaderFound = true;
              tempHeader = 0;
          }
          else
          {
              tempHeader = temp;     //save for next time
              return;
          }
      }

      //Find Source, PGN, and Length
      if (Serial.available() > 2 && isHeaderFound && !isPGNFound)
      {
          Serial.read(); //The 7F or less
          pgn = Serial.read();
          dataLength = Serial.read();
          isPGNFound = true;
          idx = 0;
      }

      //The data package
      if (Serial.available() > dataLength && isHeaderFound && isPGNFound)
      {
          if (pgn == 239) // EF Machine Data
          {
              uTurn = Serial.read();
              gpsSpeed = (float)Serial.read();//actual speed times 4, single uint8_t

              hydLift = Serial.read();
              tramline = Serial.read();  //bit 0 is right bit 1 is left

              //just get the rest of bytes
              Serial.read();   //high,low bytes   
              Serial.read();

              relayLo = Serial.read();          // read relay control from AgOpenGPS
              relayHi = Serial.read();

              if (aogConfig.isRelayActiveHigh)
              {
                  tramline = 255 - tramline;
                  relayLo = 255 - relayLo;
              }

              //Bit 13 CRC
              Serial.read();

              //reset watchdog
              watchdogTimer = 0;

              //Reset serial Watchdog  
              serialResetTimer = 0;

              //reset for next pgn sentence
              isHeaderFound = isPGNFound = false;
              pgn = dataLength = 0;
          }




      }
  }
