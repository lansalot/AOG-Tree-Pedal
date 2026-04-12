#ifndef zEthernet_h
#define zEthernet_h
void EthernetStart()
{
  // start the Ethernet connection:
  Serial.println("Initializing ethernet with static IP address");

  // try to congifure using IP:
  Ethernet.begin(mac, 0); // Start Ethernet with IP 0.0.0.0

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println("Ethernet shield was not found. GPS via USB only.");

    return;
  }

  if (Ethernet.linkStatus() == LinkOFF)
  {
    Serial.println("Ethernet cable is not connected - Who cares we will start ethernet anyway.");
  }

  // grab the ip from EEPROM
  Eth_myip[0] = networkAddress.ipOne;
  Eth_myip[1] = networkAddress.ipTwo;
  Eth_myip[2] = networkAddress.ipThree;
  Eth_myip[3] = 123; // 123 is machine module

  Ethernet.setLocalIP(Eth_myip); // Change IP address to IP set by user
  Serial.println("\r\nEthernet status OK");
  Serial.print("IP set Manually: ");
  Serial.println(Ethernet.localIP());

  Ethernet_running = true;

  Eth_BroadCastDestination[0] = Eth_myip[0];
  Eth_BroadCastDestination[1] = Eth_myip[1];
  Eth_BroadCastDestination[2] = Eth_myip[2];
  Eth_BroadCastDestination[3] = 255;

  Serial.print("\r\nEthernet IP of module: ");
  Serial.println(Ethernet.localIP());
  Serial.print("Ethernet sending to IP: ");
  Serial.println(Eth_BroadCastDestination);
  Serial.print("All data sending to port: ");
  Serial.println(portDestination);

  // init UPD Port getting AutoSteer data from AOG
  if (Eth_udpMachineBoard.begin(AOGPort))
  {
    Serial.print("Ethernet AutoSteer UDP listening to & send from port: ");
    Serial.println(AOGPort);
  }
}
#endif