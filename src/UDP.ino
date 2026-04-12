#include <Arduino.h>
#ifndef UDP_h
#define UDP_h
void ReceiveUdp()
{
  // When ethernet is not running, return directly. parsePacket() will block when we don't
  if (!Ethernet_running)
  {
    return;
  }

  uint16_t len = Eth_udpMachineBoard.parsePacket();

  // Check for len > 4, because we check byte 0, 1, 3 and 3
  if (len > 4)
  {
    Eth_udpMachineBoard.read(machineUdpData, UDP_TX_PACKET_MAX_SIZE);

    if (machineUdpData[0] == 0x80 && machineUdpData[1] == 0x81 && machineUdpData[2] == 0x7F) // Data
    {
      if (machineUdpData[3] == 0xEF)
      { // 239
        Sections = machineUdpData[11];
        //if (Sections != 0 && Sections != 15 && !engageBrake)
        if (Sections == 15 && !engageBrake)
        {
          // brake-state has changed!
          engageBrake = true;
          ramStartTime = millis();
          driveRAM(_RAMState::Push);
          Serial.println("Braking/pushing ram");
        }

        // reset watchdog
        watchdogTimer = 0;
      }

      else if (machineUdpData[3] == 200) // Hello from AgIO
      {
        helloFromMachine[5] = 1;
        helloFromMachine[6] = 2;
        SendUdp(helloFromMachine, sizeof(helloFromMachine), Eth_BroadCastDestination, portDestination);
      }

      else if (machineUdpData[3] == 201)
      {
        Serial.println("Updating network settings from AgIO");
        // make really sure this is the subnet pgn
        if (machineUdpData[4] == 5 && machineUdpData[5] == 201 && machineUdpData[6] == 201)
        {
          networkAddress.ipOne = machineUdpData[7];
          networkAddress.ipTwo = machineUdpData[8];
          networkAddress.ipThree = machineUdpData[9];
          // save in EEPROM and restart
          EEPROM.put(10, networkAddress);
          Serial.println("New IP set by AgIO: " + String(networkAddress.ipOne) + "." + String(networkAddress.ipTwo) + "." + String(networkAddress.ipThree) + ".123");
          delay(1000);
          SCB_AIRCR = 0x05FA0004; // Teensy Reset
        }
      } // end 201

      // whoami
      else if (machineUdpData[3] == 202)
      {
        // make really sure this is the reply pgn
        if (machineUdpData[4] == 3 && machineUdpData[5] == 202 && machineUdpData[6] == 202)
        {
          IPAddress rem_ip = Eth_udpMachineBoard.remoteIP();

          // hello from AgIO
          uint8_t scanReply[] = {128, 129, Eth_myip[3], 203, 7,
                                 Eth_myip[0], Eth_myip[1], Eth_myip[2], Eth_myip[3],
                                 rem_ip[0], rem_ip[1], rem_ip[2], 23};

          // checksum
          int16_t CK_A = 0;
          for (uint8_t i = 2; i < sizeof(scanReply) - 1; i++)
          {
            CK_A = (CK_A + scanReply[i]);
          }
          scanReply[sizeof(scanReply) - 1] = CK_A;

          static uint8_t ipDest[] = {255, 255, 255, 255};
          uint16_t portDest = 9999; // AOG port that listens

          // off to AOG
          SendUdp(scanReply, sizeof(scanReply), ipDest, portDest);
        }
      }
    } // end if 80 81 7F
  }
}

void SendUdp(uint8_t *data, uint8_t datalen, IPAddress dip, uint16_t dport)
{
  Eth_udpMachineBoard.beginPacket(dip, dport);
  Eth_udpMachineBoard.write(data, datalen);
  Eth_udpMachineBoard.endPacket();
}

#endif