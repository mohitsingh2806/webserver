#include "misc.h"

#define GPS_BAUD 115200

byte g_mac[] = MAC_ADDR;
IPAddress g_ip(ADAPTER_IP_ADDRESS); // IP address of arduino
IPAddress g_gateway(GATEWAY);
IPAddress g_subnet(SUBNET);

int g_openamip_port = OPENAMIP_PORT; // set in herluces local port

EthernetServer g_server(OPENAMIP_PORT);

uint32_t g_baud_rate = DEFAULT_BAUD_RATE;

// Configuration server
// EthernetServer configurationServer(CONF_PORT_TELNET);
EthernetServer configurationServer(CONF_WEB_SERVER_PORT);
// Configuration related flags and data
boolean g_configurationPktTransferComplete = false;
char g_configurationMessage[50] = {'\0'};
boolean g_configurationDataReady = false;

EthernetClient g_confclient; // Ethernet client for config server

void init_all_peripherals(void)
{

  /************************************* controller initialisation start****************************************/
  init();      // arduino related initialisations
  delay(1000); // system delay of 1 sec so that compass is powered on properly before we send the start data command

  pinMode(LAN_RST_PIN, OUTPUT);
  digitalWrite(LAN_RST_PIN, HIGH);
  delay(20);
  Serial1.begin(GPS_BAUD); // gps //115200

  /************************************* controller initialisation end****************************************/

  wdt_enable(WDTO_4S); // 4 sec watchdog timer so that we don't get stuck in processes like compass cal indefinitely

  /********************************************* Initialise LAN Settings *********************************************/
  byte o1, o2, o3, o4, o5, o6;
  if (EEPROM.read(EEPROM_LOC_IP_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG) // if eeprom has valid data, then initialise from eeprom, otherwise initialise from decalared value in code.
  {
    o1 = EEPROM.read(EEPROM_LOC_IP_OCT1);
    o2 = EEPROM.read(EEPROM_LOC_IP_OCT2);
    o3 = EEPROM.read(EEPROM_LOC_IP_OCT3);
    o4 = EEPROM.read(EEPROM_LOC_IP_OCT4);
    g_ip = IPAddress(o1, o2, o3, o4);
  }

  if (EEPROM.read(EEPROM_LOC_GATEWAY_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
  {
    o1 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT1);
    o2 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT2);
    o3 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT3);
    o4 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT4);
    g_gateway = IPAddress(o1, o2, o3, o4);
  }

  if (EEPROM.read(EEPROM_LOC_SUBNET_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
  {
    o1 = EEPROM.read(EEPROM_LOC_SUBNET_OCT1);
    o2 = EEPROM.read(EEPROM_LOC_SUBNET_OCT2);
    o3 = EEPROM.read(EEPROM_LOC_SUBNET_OCT3);
    o4 = EEPROM.read(EEPROM_LOC_SUBNET_OCT4);
    g_subnet = IPAddress(o1, o2, o3, o4);
  }

  if (EEPROM.read(EEPROM_LOC_MAC_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
  {
    o1 = EEPROM.read(EEPROM_LOC_MAC_OCT1);
    o2 = EEPROM.read(EEPROM_LOC_MAC_OCT2);
    o3 = EEPROM.read(EEPROM_LOC_MAC_OCT3);
    o3 = EEPROM.read(EEPROM_LOC_MAC_OCT4);
    o4 = EEPROM.read(EEPROM_LOC_MAC_OCT5);
    o4 = EEPROM.read(EEPROM_LOC_MAC_OCT6);

    g_mac[0] = o1;
    g_mac[1] = o2;
    g_mac[2] = o3;
    g_mac[3] = o4;
    g_mac[4] = o5;
    g_mac[5] = o6;
  }

  Ethernet.begin(g_mac, g_ip, g_gateway, g_gateway, g_subnet); // start ethernet

  uint32_t tempOpenAMIPPort = OPENAMIP_PORT;
  if (EEPROM.read(EEPROM_LOC_OPENAMIP_PORT_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
  {
    EEPROM.get(EEPROM_LOC_OPENAMIP_PORT, tempOpenAMIPPort);
    g_server = EthernetServer(tempOpenAMIPPort); // init server again with port from flash
  }
}

int periodic_lan_reset(void)
{
  static uint32_t timeBeforeLastReset = millis();
  // bool isAnyClientConnected = g_client[0].connected() || g_client[1].connected() || g_client[2].connected() || g_client[3].connected() || g_client[4].connected() || g_client[5].connected() || g_client[6].connected() || g_client[7].connected();
  bool isAnyClientConnected = 0;
  for (int i = 0; i < 8; i++)
  {
    isAnyClientConnected = isAnyClientConnected || g_client[i].connected();
  }

  if (!g_confclient.connected() && !isAnyClientConnected && millis() - timeBeforeLastReset > 20000)
  {
    resetLAN();
    timeBeforeLastReset = millis();
    // wdt_enable(WDTO_15MS);  // for periodic board reset
    // delay(1000); // for periodic board reset
  }
}

int resetLAN(void)
{
  g_confclient.stop();
  for (int i = 0; i < 8; i++)
  {
    g_client[i].stop();
    g_alreadyConnected[i] = false;
  }
  digitalWrite(LAN_RST_PIN, LOW);
  delay(10);
  digitalWrite(LAN_RST_PIN, HIGH);
  delay(50);

  Ethernet.begin(g_mac, g_ip, g_gateway, g_gateway, g_subnet); // restart ethernet
  return 0;
}

void configServerTasks()
{

  char *ret = NULL;
  static boolean recvInProgress = false;
  static byte ndx = 255;
  char endMarker = END_MARKER;
  char rc = 0;
  char ipAddressStr[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  boolean currentLineIsBlank = true;
  int reqIndex = 0;
  bool isPost = false;
  int contentLength = 0;
  bool headersEnded = false;
#define REQ_BUF_SZ 999
#define POST_BUF_SZ 999

  char reqBuffer[REQ_BUF_SZ];
  char postData[POST_BUF_SZ];

  if (!g_confclient.connected()) // this ensures only one connection on this server
  {
    g_confclient.stop();                         // release the unused socket.
    g_confclient = configurationServer.accept(); // accept incoming connections
  }

  while (g_confclient.connected())
  {
    if (g_confclient.available())
    {
      char c = g_confclient.read();

      if (reqIndex < REQ_BUF_SZ - 1)
      {
        reqBuffer[reqIndex++] = c;
        reqBuffer[reqIndex] = 0;
      }

      if (strncmp(reqBuffer, "POST", 4) == 0)
      {
        isPost = true;
      }

      if (strstr(reqBuffer, "\r\n\r\n") != NULL)
      {
        headersEnded = true;
        break;
      }
    }
  }

  // Parse content length
  char *clPtr = strstr(reqBuffer, "Content-Length:");
  if (clPtr)
  {
    clPtr += 15;
    while (*clPtr == ' ')
      clPtr++;
    contentLength = atoi(clPtr);
  }

  // Read POST data
  int postIndex = 0;
  if (isPost && contentLength > 0)
  {
    while (g_confclient.available() < contentLength)
      ;
    while (g_confclient.available() && postIndex < POST_BUF_SZ - 1)
    {
      char c = g_confclient.read();
      postData[postIndex++] = c;
    }
    postData[postIndex] = '\0';
  }

  // Parse POST fields
  char name[32] = "";
  char temp[16] = "";

  if (isPost && contentLength > 0)
  {
    char *token = strtok(postData, "&");
    while (token != NULL)
    {
      char *equalSign = strchr(token, '=');
      if (equalSign != NULL)
      {
        *equalSign = '\0';
        char *key = token;
        char *value = equalSign + 1;

        if (strcmp(key, "deviceId") == 0)
        {
          strncpy(name, value, sizeof(name) - 1);
        // g_confclient.print(name);
        }
        else if (strcmp(key, "temp") == 0)
        {
          strncpy(temp, value, sizeof(temp) - 1);
        }
      }
      token = strtok(NULL, "&");
    }
  }
  sendWebserverData();
}
//   if (g_configurationPktTransferComplete == true)
//   {

//     byte o1, o2, o3, o4;
//     ret = strstr(g_configurationMessage, "CONFIGURATION");
//     if (ret != NULL)
//     {

//       // retrieve settings from flash

//       /********************************************************************/
//       byte o1, o2, o3, o4, o5, o6;
//       IPAddress tempIP(ADAPTER_IP_ADDRESS);
//       if (EEPROM.read(EEPROM_LOC_IP_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG) // if eeprom has valid data, then initialise from eeprom, otherwise initialise from decalared value in code.
//       {
//         o1 = EEPROM.read(EEPROM_LOC_IP_OCT1);
//         o2 = EEPROM.read(EEPROM_LOC_IP_OCT2);
//         o3 = EEPROM.read(EEPROM_LOC_IP_OCT3);
//         o4 = EEPROM.read(EEPROM_LOC_IP_OCT4);
//         tempIP = IPAddress(o1, o2, o3, o4); // take IP Address from EEPROM
//       }
//       IPAddress tempGW(GATEWAY);
//       if (EEPROM.read(EEPROM_LOC_GATEWAY_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
//       {
//         o1 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT1);
//         o2 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT2);
//         o3 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT3);
//         o4 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT4);
//         tempGW = IPAddress(o1, o2, o3, o4);
//       }
//       IPAddress tempSubnet(SUBNET);
//       if (EEPROM.read(EEPROM_LOC_SUBNET_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
//       {
//         o1 = EEPROM.read(EEPROM_LOC_SUBNET_OCT1);
//         o2 = EEPROM.read(EEPROM_LOC_SUBNET_OCT2);
//         o3 = EEPROM.read(EEPROM_LOC_SUBNET_OCT3);
//         o4 = EEPROM.read(EEPROM_LOC_SUBNET_OCT4);
//         tempSubnet = IPAddress(o1, o2, o3, o4);
//       }
//       byte tempMAC[] = MAC_ADDR;
//       if (EEPROM.read(EEPROM_LOC_MAC_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
//       {
//         o1 = EEPROM.read(EEPROM_LOC_MAC_OCT1);
//         o2 = EEPROM.read(EEPROM_LOC_MAC_OCT2);
//         o3 = EEPROM.read(EEPROM_LOC_MAC_OCT3);
//         o4 = EEPROM.read(EEPROM_LOC_MAC_OCT4);
//         o5 = EEPROM.read(EEPROM_LOC_MAC_OCT5);
//         o6 = EEPROM.read(EEPROM_LOC_MAC_OCT6);

//         tempMAC[0] = o1;
//         tempMAC[1] = o2;
//         tempMAC[2] = o3;
//         tempMAC[3] = o4;
//         tempMAC[4] = o5;
//         tempMAC[5] = o6;
//       }

//       uint32_t tempOpenAMIPPort = OPENAMIP_PORT;

//       if (EEPROM.read(EEPROM_LOC_OPENAMIP_PORT_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
//       {
//         EEPROM.get(EEPROM_LOC_OPENAMIP_PORT, tempOpenAMIPPort);
//       }

//       uint32_t tempBaudrate = DEFAULT_BAUD_RATE;

//       if (EEPROM.read(EEPROM_LOC_BAUD_RATE_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
//       {
//         EEPROM.get(EEPROM_LOC_BAUD_RATE, tempBaudrate);
//       }
//       /****************************************************************************/
//       g_confclient.println("NMEA-openAMIP Converter"); // to identify sparr replacement code from STL code for MPT

//       g_confclient.print("Software Version :");
//       g_confclient.println(SWVERSION);

//       g_confclient.print("Release Date:");
//       g_confclient.println(DATE);
//       g_confclient.print("\r\n");

//       g_confclient.print("IP ADDRESS: ");
//       ipAddressToString(tempIP, ipAddressStr, sizeof(ipAddressStr));
//       g_confclient.print(ipAddressStr);
//       g_confclient.print("\r\n");

//       g_confclient.print("GATEWAY: ");
//       ipAddressToString(tempGW, ipAddressStr, sizeof(ipAddressStr));
//       g_confclient.print(ipAddressStr);
//       g_confclient.print("\r\n");

//       g_confclient.print("SUBNET MASK: ");
//       ipAddressToString(tempSubnet, ipAddressStr, sizeof(ipAddressStr));
//       g_confclient.print(ipAddressStr);
//       g_confclient.print("\r\n");

//       g_confclient.print("MAC ADDRESS: ");
//       g_confclient.print(tempMAC[0], HEX);
//       g_confclient.print(":");
//       g_confclient.print(tempMAC[1], HEX);
//       g_confclient.print(":");
//       g_confclient.print(tempMAC[2], HEX);
//       g_confclient.print(":");
//       g_confclient.print(tempMAC[3], HEX);
//       g_confclient.print(":");
//       g_confclient.print(tempMAC[4], HEX);
//       g_confclient.print(":");
//       g_confclient.print(tempMAC[5], HEX);
//       g_confclient.print("\r\n");

//       g_confclient.print("OPENAMIP PORT: ");
//       g_confclient.print(tempOpenAMIPPort);
//       g_confclient.print("\r\n");

//       g_confclient.print("BAUD RATE: ");
//       g_confclient.print(tempBaudrate);
//       g_confclient.print("\r\n");
//     }

//     ret = strstr(g_configurationMessage, "?");
//     if (ret != NULL)
//     {

//       g_confclient.write("    \r\n"
//                          "    FORMAT FOR SENDING COMMANDS:\r\n\r\n"
//                          "    IP=x.x.x.x<CR>\r\n"
//                          "    GATEWAY=x.x.x.x<CR>\r\n"
//                          "    SUBNET=x.x.x.x<CR>\r\n"
//                          "    MAC_ADDRESS=xx:xx:xx:xx:xx:xx<CR>\r\n"
//                          "    OPENAMIP_PORT=xxxx<CR>\r\n"
//                          "    BAUD=xxxxxxx<CR>\r\n");
//     }

//     ret = strstr(g_configurationMessage, "HELP");
//     if (ret != NULL)
//     {

//       g_confclient.write("    \r\n"
//                          "    FORMAT FOR SENDING COMMANDS:\r\n\r\n"
//                          "    IP=x.x.x.x<CR>\r\n"
//                          "    GATEWAY=x.x.x.x<CR>\r\n"
//                          "    SUBNET=x.x.x.x<CR>\r\n"
//                          "    MAC_ADDRESS=xx:xx:xx:xx:xx:xx<CR>\r\n"
//                          "    OPENAMIP_PORT=xxxx<CR>\r\n"
//                          "    BAUD=xxxxxxx<CR>\r\n");
//     }
//     ret = strstr(g_configurationMessage, "IP=");
//     if (ret != NULL)
//     {
//       ret = strtok(ret, "=");
//       ret = strtok(NULL, ".");
//       o1 = atoi(ret);
//       ret = strtok(NULL, ".");
//       o2 = atoi(ret);
//       ret = strtok(NULL, ".");
//       o3 = atoi(ret);
//       ret = strtok(NULL, "\0");
//       o4 = atoi(ret);

//       EEPROM.update(EEPROM_LOC_IP_VALID_FLAG, EEPROM_VALID_VALUE_FLAG);
//       EEPROM.update(EEPROM_LOC_IP_OCT1, o1);
//       EEPROM.update(EEPROM_LOC_IP_OCT2, o2);
//       EEPROM.update(EEPROM_LOC_IP_OCT3, o3);
//       EEPROM.update(EEPROM_LOC_IP_OCT4, o4);

//       g_ip = IPAddress(o1, o2, o3, o4);

//       ipAddressToString(g_ip, ipAddressStr, sizeof(ipAddressStr));

//       g_confclient.write("\r\nNew IP Address is : ");
//       g_confclient.write(ipAddressStr);

//       // Ethernet.begin(g_mac, g_ip, g_gateway, g_gateway, g_subnet);
//       g_confclient.print("\r\nREBOOT REQUIRED");
//       return;
//     }

//     ret = strstr(g_configurationMessage, "SUBNET=");
//     if (ret != NULL)
//     {
//       ret = strtok(ret, "=");
//       ret = strtok(NULL, ".");
//       o1 = atoi(ret);
//       ret = strtok(NULL, ".");
//       o2 = atoi(ret);
//       ret = strtok(NULL, ".");
//       o3 = atoi(ret);
//       ret = strtok(NULL, "\0");
//       o4 = atoi(ret);
//       EEPROM.update(EEPROM_LOC_SUBNET_VALID_FLAG, EEPROM_VALID_VALUE_FLAG);
//       EEPROM.update(EEPROM_LOC_SUBNET_OCT1, o1);
//       EEPROM.update(EEPROM_LOC_SUBNET_OCT2, o2);
//       EEPROM.update(EEPROM_LOC_SUBNET_OCT3, o3);
//       EEPROM.update(EEPROM_LOC_SUBNET_OCT4, o4);
//       g_subnet = IPAddress(o1, o2, o3, o4);
//       ipAddressToString(g_subnet, ipAddressStr, sizeof(ipAddressStr));

//       g_confclient.write("\r\nNew Subnet Mask is : ");
//       g_confclient.write(ipAddressStr);

//       // Ethernet.begin(g_mac, g_ip, g_gateway, g_gateway, g_subnet);
//       g_confclient.print("\r\nREBOOT REQUIRED");
//       return;
//     }

//     ret = strstr(g_configurationMessage, "GATEWAY=");
//     if (ret != NULL)
//     {
//       ret = strtok(ret, "=");
//       ret = strtok(NULL, ".");
//       o1 = atoi(ret);
//       ret = strtok(NULL, ".");
//       o2 = atoi(ret);
//       ret = strtok(NULL, ".");
//       o3 = atoi(ret);
//       ret = strtok(NULL, "\0");
//       o4 = atoi(ret);

//       EEPROM.update(EEPROM_LOC_GATEWAY_VALID_FLAG, EEPROM_VALID_VALUE_FLAG);
//       EEPROM.update(EEPROM_LOC_GATEWAY_OCT1, o1);
//       EEPROM.update(EEPROM_LOC_GATEWAY_OCT2, o2);
//       EEPROM.update(EEPROM_LOC_GATEWAY_OCT3, o3);
//       EEPROM.update(EEPROM_LOC_GATEWAY_OCT4, o4);
//       g_gateway = IPAddress(o1, o2, o3, o4);
//       ipAddressToString(g_gateway, ipAddressStr, sizeof(ipAddressStr));

//       g_confclient.write("\r\nNew Default Gateway is : ");
//       g_confclient.write(ipAddressStr);

//       // Ethernet.begin(g_mac, g_ip, g_gateway, g_gateway, g_subnet);
//       g_confclient.print("\r\nREBOOT REQUIRED");
//       return;
//     }

//     ret = strstr(g_configurationMessage, "MAC_ADDRESS=");
//     if (ret != NULL)
//     {

//       ret = strtok(ret, "=");
//       ret = strtok(NULL, ":");
//       g_mac[0] = hex2byte(ret);
//       ret = strtok(NULL, ":");
//       g_mac[1] = hex2byte(ret);
//       ret = strtok(NULL, ":");
//       g_mac[2] = hex2byte(ret);
//       ret = strtok(NULL, ":");
//       g_mac[3] = hex2byte(ret);
//       ret = strtok(NULL, ":");
//       g_mac[4] = hex2byte(ret);
//       ret = strtok(NULL, "\0");
//       g_mac[5] = hex2byte(ret);

//       // for(int i =0;i<6;i++){
//       //            g_confclient.println(g_mac[i]);

//       // }

//       EEPROM.update(EEPROM_LOC_MAC_VALID_FLAG, EEPROM_VALID_VALUE_FLAG);
//       EEPROM.update(EEPROM_LOC_MAC_OCT1, g_mac[0]);
//       EEPROM.update(EEPROM_LOC_MAC_OCT2, g_mac[1]);
//       EEPROM.update(EEPROM_LOC_MAC_OCT3, g_mac[2]);
//       EEPROM.update(EEPROM_LOC_MAC_OCT4, g_mac[3]);
//       EEPROM.update(EEPROM_LOC_MAC_OCT5, g_mac[4]);
//       EEPROM.update(EEPROM_LOC_MAC_OCT6, g_mac[5]);

//       g_confclient.write("\r\nMAC Address Received");
//       // Ethernet.begin(g_mac, g_ip, g_gateway, g_gateway, g_subnet);
//       g_confclient.print("\r\nREBOOT REQUIRED");
//       return;
//     }
//     ret = strstr(g_configurationMessage, "OPENAMIP_PORT=");
//     if (ret != NULL)
//     {
//       ret = strtok(ret, "=");
//       ret = strtok(NULL, "");

//       g_openamip_port = (uint32_t)atoi(ret);
//       EEPROM.update(EEPROM_LOC_OPENAMIP_PORT_VALID_FLAG, EEPROM_VALID_VALUE_FLAG);
//       // EEPROM.update(EEPROM_LOC_LOCAL_PORT, g_port_for_local_cnx);
//       writeLong(EEPROM_LOC_OPENAMIP_PORT, g_openamip_port);
//       g_confclient.write("\r\nTCP Port for openAMIP connection Updated to: ");
//       g_confclient.print(g_openamip_port);
//       g_confclient.print("\r\nREBOOT REQUIRED");
//       return;
//       // delay(10000);
//     }

//     ret = strstr(g_configurationMessage, "BAUD=");
//     if (ret != NULL)
//     {
//       ret = strtok(ret, "=");
//       ret = strtok(NULL, "");
//       g_baud_rate = strtol(ret, 0, 10);

//       Serial1.end();
//       Serial1.begin(g_baud_rate);
//       EEPROM.update(EEPROM_LOC_BAUD_RATE_VALID_FLAG, EEPROM_VALID_VALUE_FLAG);
//       // EEPROM.update(EEPROM_LOC_BAUD_RATE, g_baud_rate);
//       writeLong(EEPROM_LOC_BAUD_RATE, g_baud_rate);
//       g_confclient.write("\r\nBaud Rate for Serial Connection updated to: ");
//       g_confclient.print(g_baud_rate);
//     }

//     ret = strstr(g_configurationMessage, "ERASE@5433");
//     if (ret != NULL)
//     {

//       g_confclient.write("\r\nDevice Erase started.. \r\n");
//       for (int i = EEPROM_LOC_START; i <= EEPROM_LOC_END; i++)
//       {
//         EEPROM.update(i, 0xFF);
//       }
//       g_confclient.write("Device Erased. Reboot the system\r\n");
//       delay(10000);
//     }

//     g_configurationDataReady = true;
//     g_configurationPktTransferComplete = false;
//   }
// }

int searchChar(char *buff, char c, size_t start = 0, size_t end = MAXPACKETSIZE_GPS)
{
  for (size_t i = start; i < end; i++)
  {
    if (buff[i] == c)
    {
      return i;
    }
  }
  return -1;
}

float fround(float var)
{

  float value = (int)(var * 100 + .5);
  return (float)value / 100;
}
void ipAddressToString(const IPAddress &ip, char *buffer, size_t bufferSize)
{
  snprintf(buffer, bufferSize, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

void writeLong(int address, uint32_t value)
{
  for (int i = 0; i < 4; i++)
  {
    EEPROM.update(address + i, (value >> (8 * i)) & 0xFF);
  }
}

uint32_t readLong(int address)
{
  uint32_t value = 0;
  for (int i = 0; i < 4; i++)
  {
    value |= (long(EEPROM.read(address + i)) << (8 * i));
  }
  return value;
}
unsigned char hex2byte(const char *hex)
{
  if (!hex || hex[0] == '\0') // Check for null or empty input
  {
    return 0;
  }

  unsigned char val = 0;

  // Check if it's a single-digit input (second character is null)
  if (hex[1] == '\0')
  {
    unsigned char byte = hex[0];
    // Process single digit as the lower nibble (e.g., "f" -> 0x0f)
    if (byte >= '0' && byte <= '9')
    {
      val = (byte - '0');
    }
    else if (byte >= 'A' && byte <= 'F')
    {
      val = (byte - 'A' + 10);
    }
    else if (byte >= 'a' && byte <= 'f')
    {
      val = (byte - 'a' + 10);
    }
    else
    {
      return 0; // Invalid hex character
    }
    return val;
  }

  // Handle two-digit input
  for (int i = 0; i < 2; i++)
  {
    unsigned char byte = hex[i];
    val <<= 4;
    if (byte >= '0' && byte <= '9')
    {
      val |= (byte - '0');
    }
    else if (byte >= 'A' && byte <= 'F')
    {
      val |= (byte - 'A' + 10);
    }
    else if (byte >= 'a' && byte <= 'f')
    {
      val |= (byte - 'a' + 10);
    }
    else
    {
      return 0; // Invalid hex character
    }
  }
  return val;
}

void sendWebserverData(void)
{

  char ipAddressStr[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  g_confclient.println("HTTP/1.0 200 OK");
  g_confclient.println("Content-Type: text/html");
  g_confclient.println();
  // g_confclient.println("<HTML><BODY>TEST OK!</BODY></HTML>");
  byte o1, o2, o3, o4, o5, o6;
  IPAddress tempIP(ADAPTER_IP_ADDRESS);
  if (EEPROM.read(EEPROM_LOC_IP_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG) // if eeprom has valid data, then initialise from eeprom, otherwise initialise from decalared value in code.
  {
    o1 = EEPROM.read(EEPROM_LOC_IP_OCT1);
    o2 = EEPROM.read(EEPROM_LOC_IP_OCT2);
    o3 = EEPROM.read(EEPROM_LOC_IP_OCT3);
    o4 = EEPROM.read(EEPROM_LOC_IP_OCT4);
    tempIP = IPAddress(o1, o2, o3, o4); // take IP Address from EEPROM
  }
  IPAddress tempGW(GATEWAY);
  if (EEPROM.read(EEPROM_LOC_GATEWAY_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
  {
    o1 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT1);
    o2 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT2);
    o3 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT3);
    o4 = EEPROM.read(EEPROM_LOC_GATEWAY_OCT4);
    tempGW = IPAddress(o1, o2, o3, o4);
  }
  IPAddress tempSubnet(SUBNET);
  if (EEPROM.read(EEPROM_LOC_SUBNET_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
  {
    o1 = EEPROM.read(EEPROM_LOC_SUBNET_OCT1);
    o2 = EEPROM.read(EEPROM_LOC_SUBNET_OCT2);
    o3 = EEPROM.read(EEPROM_LOC_SUBNET_OCT3);
    o4 = EEPROM.read(EEPROM_LOC_SUBNET_OCT4);
    tempSubnet = IPAddress(o1, o2, o3, o4);
  }
  byte tempMAC[] = MAC_ADDR;
  if (EEPROM.read(EEPROM_LOC_MAC_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
  {
    o1 = EEPROM.read(EEPROM_LOC_MAC_OCT1);
    o2 = EEPROM.read(EEPROM_LOC_MAC_OCT2);
    o3 = EEPROM.read(EEPROM_LOC_MAC_OCT3);
    o4 = EEPROM.read(EEPROM_LOC_MAC_OCT4);
    o5 = EEPROM.read(EEPROM_LOC_MAC_OCT5);
    o6 = EEPROM.read(EEPROM_LOC_MAC_OCT6);

    tempMAC[0] = o1;
    tempMAC[1] = o2;
    tempMAC[2] = o3;
    tempMAC[3] = o4;
    tempMAC[4] = o5;
    tempMAC[5] = o6;
  }

  uint32_t tempOpenAMIPPort = OPENAMIP_PORT;

  if (EEPROM.read(EEPROM_LOC_OPENAMIP_PORT_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
  {
    EEPROM.get(EEPROM_LOC_OPENAMIP_PORT, tempOpenAMIPPort);
  }

  uint32_t tempBaudrate = DEFAULT_BAUD_RATE;

  if (EEPROM.read(EEPROM_LOC_BAUD_RATE_VALID_FLAG) == EEPROM_VALID_VALUE_FLAG)
  {
    EEPROM.get(EEPROM_LOC_BAUD_RATE, tempBaudrate);
  }

  g_confclient.print(F("<!DOCTYPE html>\n"));
  g_confclient.print(F("<html lang=\"en\">\n"));
  g_confclient.print(F("<head>\n"));
  g_confclient.print(F("<meta charset=\"UTF-8\">\n"));
  g_confclient.print(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"));
  g_confclient.print(F("<title>Network Configuration</title>\n"));
  g_confclient.print(F("<style>\n"));
  g_confclient.print(F("* {\n"));
  g_confclient.print(F("margin: 0;\n"));
  g_confclient.print(F("padding: 0;\n"));
  g_confclient.print(F("box-sizing: border-box;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("body {\n"));
  g_confclient.print(F("font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n"));
  g_confclient.print(F("background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n"));
  g_confclient.print(F("min-height: 100vh;\n"));
  g_confclient.print(F("padding: 20px;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".container {\n"));
  g_confclient.print(F("max-width: 1200px;\n"));
  g_confclient.print(F("margin: 0 auto;\n"));
  g_confclient.print(F("background: white;\n"));
  g_confclient.print(F("border-radius: 15px;\n"));
  g_confclient.print(F("box-shadow: 0 15px 35px rgba(0, 0, 0, 0.1);\n"));
  g_confclient.print(F("overflow: hidden;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".header {\n"));
  g_confclient.print(F("background: linear-gradient(90deg, #4a90e2, #7b68ee);\n"));
  g_confclient.print(F("color: white;\n"));
  g_confclient.print(F("padding: 25px;\n"));
  g_confclient.print(F("text-align: center;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".header h1 {\n"));
  g_confclient.print(F("font-size: 2rem;\n"));
  g_confclient.print(F("font-weight: 300;\n"));
  g_confclient.print(F("letter-spacing: 1px;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".content {\n"));
  g_confclient.print(F("display: grid;\n"));
  g_confclient.print(F("grid-template-columns: 1fr 1fr;\n"));
  g_confclient.print(F("gap: 30px;\n"));
  g_confclient.print(F("padding: 30px;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".config-section, .current-config {\n"));
  g_confclient.print(F("background: #f8f9fa;\n"));
  g_confclient.print(F("padding: 25px;\n"));
  g_confclient.print(F("border-radius: 10px;\n"));
  g_confclient.print(F("border: 1px solid #e9ecef;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".section-title {\n"));
  g_confclient.print(F("font-size: 1.5rem;\n"));
  g_confclient.print(F("font-weight: 600;\n"));
  g_confclient.print(F("color: #2c3e50;\n"));
  g_confclient.print(F("margin-bottom: 25px;\n"));
  g_confclient.print(F("text-align: center;\n"));
  g_confclient.print(F("padding-bottom: 10px;\n"));
  g_confclient.print(F("border-bottom: 2px solid #4a90e2;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".form-group {\n"));
  g_confclient.print(F("margin-bottom: 20px;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".form-row {\n"));
  g_confclient.print(F("display: grid;\n"));
  g_confclient.print(F("grid-template-columns: 120px 1fr;\n"));
  g_confclient.print(F("align-items: center;\n"));
  g_confclient.print(F("gap: 15px;\n"));
  g_confclient.print(F("margin-bottom: 15px;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("label {\n"));
  g_confclient.print(F("font-weight: 500;\n"));
  g_confclient.print(F("color: #34495e;\n"));
  g_confclient.print(F("font-size: 0.95rem;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("input[type=\"text\"], input[type=\"email\"] {\n"));
  g_confclient.print(F("padding: 12px 15px;\n"));
  g_confclient.print(F("border: 2px solid #ddd;\n"));
  g_confclient.print(F("border-radius: 6px;\n"));
  g_confclient.print(F("font-size: 0.95rem;\n"));
  g_confclient.print(F("transition: all 0.3s ease;\n"));
  g_confclient.print(F("background: white;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("input[type=\"text\"]:focus, input[type=\"email\"]:focus {\n"));
  g_confclient.print(F("outline: none;\n"));
  g_confclient.print(F("border-color: #4a90e2;\n"));
  g_confclient.print(F("box-shadow: 0 0 0 3px rgba(74, 144, 226, 0.1);\n"));
  g_confclient.print(F("transform: translateY(-1px);\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".submit-btn {\n"));
  g_confclient.print(F("width: 100%;\n"));
  g_confclient.print(F("padding: 15px;\n"));
  g_confclient.print(F("background: linear-gradient(45deg, #4a90e2, #7b68ee);\n"));
  g_confclient.print(F("color: white;\n"));
  g_confclient.print(F("border: none;\n"));
  g_confclient.print(F("border-radius: 8px;\n"));
  g_confclient.print(F("font-size: 1.1rem;\n"));
  g_confclient.print(F("font-weight: 600;\n"));
  g_confclient.print(F("cursor: pointer;\n"));
  g_confclient.print(F("transition: all 0.3s ease;\n"));
  g_confclient.print(F("margin-top: 20px;\n"));
  g_confclient.print(F("text-transform: uppercase;\n"));
  g_confclient.print(F("letter-spacing: 1px;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".submit-btn:hover {\n"));
  g_confclient.print(F("transform: translateY(-2px);\n"));
  g_confclient.print(F("box-shadow: 0 8px 25px rgba(74, 144, 226, 0.3);\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".submit-btn:active {\n"));
  g_confclient.print(F("transform: translateY(0);\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".config-display {\n"));
  g_confclient.print(F("background: white;\n"));
  g_confclient.print(F("padding: 20px;\n"));
  g_confclient.print(F("border-radius: 8px;\n"));
  g_confclient.print(F("border-left: 4px solid #4a90e2;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".config-item {\n"));
  g_confclient.print(F("display: grid;\n"));
  g_confclient.print(F("grid-template-columns: 130px 1fr;\n"));
  g_confclient.print(F("gap: 10px;\n"));
  g_confclient.print(F("margin-bottom: 12px;\n"));
  g_confclient.print(F("padding: 8px 0;\n"));
  g_confclient.print(F("border-bottom: 1px solid #eee;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".config-label {\n"));
  g_confclient.print(F("font-weight: 600;\n"));
  g_confclient.print(F("color: #2c3e50;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".config-value {\n"));
  g_confclient.print(F("color: #666;\n"));
  g_confclient.print(F("font-family: 'Courier New', monospace;\n"));
  g_confclient.print(F("background: #f8f9fa;\n"));
  g_confclient.print(F("padding: 4px 8px;\n"));
  g_confclient.print(F("border-radius: 4px;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".version-info {\n"));
  g_confclient.print(F(" white-space: pre;text-align: left;\n"));
  g_confclient.print(F("margin-top: 20px;\n"));
  g_confclient.print(F("padding: 15px;\n"));
  g_confclient.print(F("background: #e8f4fd;\n"));
  g_confclient.print(F("border-radius: 6px;\n"));
  g_confclient.print(F("color: #2c3e50;\n"));
  g_confclient.print(F("font-weight: 500;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("@media (max-width: 768px) {\n"));
  g_confclient.print(F(".content {\n"));
  g_confclient.print(F("grid-template-columns: 1fr;\n"));
  g_confclient.print(F("gap: 20px;\n"));
  g_confclient.print(F("padding: 20px;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".form-row {\n"));
  g_confclient.print(F("grid-template-columns: 1fr;\n"));
  g_confclient.print(F("gap: 8px;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".config-item {\n"));
  g_confclient.print(F("grid-template-columns: 1fr;\n"));
  g_confclient.print(F("gap: 5px;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".loading {\n"));
  g_confclient.print(F("display: none;\n"));
  g_confclient.print(F("text-align: center;\n"));
  g_confclient.print(F("color: #666;\n"));
  g_confclient.print(F("font-style: italic;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F(".success-message {\n"));
  g_confclient.print(F("display: none;\n"));
  g_confclient.print(F("background: #d4edda;\n"));
  g_confclient.print(F("color: #155724;\n"));
  g_confclient.print(F("padding: 12px;\n"));
  g_confclient.print(F("border-radius: 6px;\n"));
  g_confclient.print(F("margin-top: 15px;\n"));
  g_confclient.print(F("border: 1px solid #c3e6cb;\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("</style>\n"));
  g_confclient.print(F("</head>\n"));
  g_confclient.print(F("<body>\n"));
  g_confclient.print(F("<div class=\"container\">\n"));
  g_confclient.print(F("<div class=\"header\">\n"));
  g_confclient.print(F("<h1>Network Configuration Panel</h1>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"content\">\n"));
  g_confclient.print(F("<!-- Network Configuration Form -->\n"));
  g_confclient.print(F("<div class=\"config-section\">\n"));
  g_confclient.print(F("<h2 class=\"section-title\">Configuration Window</h2>\n"));
  g_confclient.print(F("<form id=\"networkForm\" method=\"POST\" action=\"\">\n"));
  g_confclient.print(F("<div class=\"form-group\">\n"));
  g_confclient.print(F("<div class=\"form-row\">\n"));
  g_confclient.print(F("<label for=\"deviceId\">ID:</label>\n"));
  g_confclient.print(F("<input type=\"text\" id=\"deviceId\" name=\"deviceId\" >\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"form-row\">\n"));
  g_confclient.print(F("<label for=\"ipAddress\">IP Address:</label>\n"));
  g_confclient.print(F("<input type=\"text\" id=\"ipAddress\" name=\"ipAddress\" pattern=\"^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$\">\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"form-row\">\n"));
  g_confclient.print(F("<label for=\"defaultGw\">Default GW:</label>\n"));
  g_confclient.print(F("<input type=\"text\" id=\"defaultGw\" name=\"defaultGw\" pattern=\"^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$\">\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"form-row\">\n"));
  g_confclient.print(F("<label for=\"subnetMask\">Subnet Mask:</label>\n"));
  g_confclient.print(F("<input type=\"text\" id=\"subnetMask\" name=\"subnetMask\"  pattern=\"^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$\">\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"form-row\">\n"));
  g_confclient.print(F("<label for=\"macAddress\">MAC Address:</label>\n"));
  g_confclient.print(F("<input type=\"text\" id=\"macAddress\" name=\"macAddress\"  pattern=\"^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$\">\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"form-row\">\n"));
  g_confclient.print(F("<label for=\"openAmpPort\">OpenAMIP Port:</label>\n"));
  g_confclient.print(F("<input type=\"text\" id=\"openAmpPort\" name=\"openAmpPort\" >\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"form-row\">\n"));
  g_confclient.print(F("<label for=\"baudRate\">Baud Rate:</label>\n"));
  g_confclient.print(F("<input type=\"text\" id=\"baudRate\" name=\"baudRate\" >\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  // g_confclient.print(F("<div class=\"form-row\">\n"));
  // g_confclient.print(F("<label for=\"serialNumber\">S.N.:</label>\n"));
  // g_confclient.print(F("<input type=\"text\" id=\"serialNumber\" name=\"serialNumber\" >\n"));
  // g_confclient.print(F("</div>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<button type=\"submit\" class=\"submit-btn\">Submit Configuration</button>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"loading\" id=\"loadingMessage\">\n"));
  g_confclient.print(F("Submitting configuration...\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"success-message\" id=\"successMessage\">\n"));
  g_confclient.print(F("Configuration submitted successfully!\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("</form>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<!-- Current Configuration Display -->\n"));
  g_confclient.print(F("<div class=\"current-config\">\n"));
  g_confclient.print(F("<h2 class=\"section-title\">Current Configuration</h2>\n"));
  g_confclient.print(F("<div class=\"config-display\">\n"));

  g_confclient.print(F("<div class=\"config-item\">\n"));
  g_confclient.print(F("<span class=\"config-label\">Device ID:</span>\n"));
  g_confclient.print(F("<span class=\"config-value\" id=\"deviceID\">"));
  g_confclient.print("DUMMY_ID");

  g_confclient.print(F("</span>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));

  g_confclient.print(F("<div class=\"config-item\">\n"));
  g_confclient.print(F("<span class=\"config-label\">IP Address:</span>\n"));
  g_confclient.print(F("<span class=\"config-value\" id=\"currentIp\">"));
  ipAddressToString(tempIP, ipAddressStr, sizeof(ipAddressStr));
  g_confclient.print(ipAddressStr);

  g_confclient.print(F("</span>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"config-item\">\n"));
  g_confclient.print(F("<span class=\"config-label\">Default GW:</span>\n"));
  g_confclient.print(F("<span class=\"config-value\" id=\"currentGw\">"));

  ipAddressToString(tempGW, ipAddressStr, sizeof(ipAddressStr));
  g_confclient.print(ipAddressStr);
  g_confclient.print(F("</span>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"config-item\">\n"));
  g_confclient.print(F("<span class=\"config-label\">Subnet Mask:</span>\n"));
  g_confclient.print(F("<span class=\"config-value\" id=\"currentMask\">"));
  ipAddressToString(tempSubnet, ipAddressStr, sizeof(ipAddressStr));
  g_confclient.print(ipAddressStr);
  g_confclient.print(F("</span>\n"));

  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"config-item\">\n"));
  g_confclient.print(F("<span class=\"config-label\">MAC Address:</span>\n"));
  g_confclient.print(F("<span class=\"config-value\" id=\"currentMac\">"));
  g_confclient.print(tempMAC[0], HEX);
  g_confclient.print(":");
  g_confclient.print(tempMAC[1], HEX);
  g_confclient.print(":");
  g_confclient.print(tempMAC[2], HEX);
  g_confclient.print(":");
  g_confclient.print(tempMAC[3], HEX);
  g_confclient.print(":");
  g_confclient.print(tempMAC[4], HEX);
  g_confclient.print(":");
  g_confclient.print(tempMAC[5], HEX);
  g_confclient.print("\r\n");
  g_confclient.print(F("</span>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"config-item\">\n"));
  g_confclient.print(F("<span class=\"config-label\">OpenAMIP Port:</span>\n"));
  g_confclient.print(F("<span class=\"config-value\" id=\"currentPort\">"));
  g_confclient.print(tempOpenAMIPPort);

  g_confclient.print(F("</span>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"config-item\">\n"));
  g_confclient.print(F("<span class=\"config-label\">Baud Rate:</span>\n"));
  g_confclient.print(F("<span class=\"config-value\" id=\"currentBaud\">"));

  g_confclient.print(tempBaudrate);
  g_confclient.print(F("</span>\n"));

  g_confclient.print(F("</div>\n"));
  // g_confclient.print(F("\n"));
  g_confclient.print(F("<div class=\"version-info\">\n"));
  g_confclient.print(F("<strong>Version:</strong>                                "));
  g_confclient.println(SWVERSION);
  // g_confclient.print(F("  "));
  g_confclient.print(F("<p><strong>Release Date:</strong>                       "));
  g_confclient.print(DATE);
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("</div>\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("<script>\n"));
  g_confclient.print(F("document.getElementById('networkForm').addEventListener('submit', function(e) {\n"));
  g_confclient.print(F("e.preventDefault();\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("// Show loading message\n"));
  g_confclient.print(F("document.getElementById('loadingMessage').style.display = 'block';\n"));
  g_confclient.print(F("document.getElementById('successMessage').style.display = 'none';\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("// Get form data\n"));
  g_confclient.print(F("const formData = new FormData(this);\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("// Simulate form submission (replace with actual POST request)\n"));
  g_confclient.print(F("fetch(this.action || window.location.href, {\n"));
  g_confclient.print(F("    method: this.method || 'POST',\n"));
  g_confclient.print(F("    body: formData,\n"));
  g_confclient.print(F("})\n"));
  g_confclient.print(F(".then(response => {\n"));
  g_confclient.print(F("    if (!response.ok) {\n"));
  g_confclient.print(F("        throw new Error('Network error');\n"));
  g_confclient.print(F("    }\n"));
  g_confclient.print(F("    return response.json(); // or .text() depending on your backend\n"));
  g_confclient.print(F("})\n"));

  g_confclient.print(F("document.getElementById('loadingMessage').style.display = 'none';\n"));
  g_confclient.print(F("document.getElementById('successMessage').style.display = 'block';\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("// Hide success message after 3 seconds\n"));
  g_confclient.print(F("setTimeout(() => {\n"));
  g_confclient.print(F("document.getElementById('successMessage').style.display = 'none';\n"));
  g_confclient.print(F("}, 3000);\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("// Reset form\n"));
  g_confclient.print(F("this.reset();\n"));
  g_confclient.print(F("}, 1500);\n"));
  g_confclient.print(F("\n"));

  g_confclient.print(F("});\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("// Input validation\n"));
  g_confclient.print(F("document.getElementById('ipAddress').addEventListener('input', function() {\n"));
  g_confclient.print(F("validateIP(this);\n"));
  g_confclient.print(F("});\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("document.getElementById('defaultGw').addEventListener('input', function() {\n"));
  g_confclient.print(F("validateIP(this);\n"));
  g_confclient.print(F("});\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("document.getElementById('subnetMask').addEventListener('input', function() {\n"));
  g_confclient.print(F("validateIP(this);\n"));
  g_confclient.print(F("});\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("function validateIP(input) {\n"));
  g_confclient.print(F("const ipPattern = /^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$/;\n"));
  g_confclient.print(F("if (input.value && !ipPattern.test(input.value)) {\n"));
  g_confclient.print(F("input.style.borderColor = '#e74c3c';\n"));
  g_confclient.print(F("} else {\n"));
  g_confclient.print(F("input.style.borderColor = '#ddd';\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("}\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("// MAC address formatting\n"));
  g_confclient.print(F("document.getElementById('macAddress').addEventListener('input', function() {\n"));
  g_confclient.print(F("let value = this.value.replace(/[^0-9A-Fa-f]/g, '');\n"));
  g_confclient.print(F("if (value.length > 12) value = value.substr(0, 12);\n"));
  g_confclient.print(F("\n"));
  g_confclient.print(F("// Add colons every 2 characters\n"));
  g_confclient.print(F("value = value.replace(/(.{2})/g, '$1:').slice(0, -1);\n"));
  g_confclient.print(F("this.value = value;\n"));
  g_confclient.print(F("});\n"));
  g_confclient.print(F("</script>\n"));
  g_confclient.print(F("</body>\n"));
  g_confclient.print(F("</html>\n"));

  g_confclient.stop();
}