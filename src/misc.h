#ifndef MISC_H
#define MISC_H

#include <Arduino.h>
#include <Ethernet.h>
#include <SPI.h>
#include <Wire.h>
#include "gps.h"
#include <TinyGPS++.h>
#include "openAMIP.h"
#include <EEPROM.h>
#include <avr/wdt.h>


#define SWVERSION "0.0.1a"
#define DATE "16.10.2025"  // 9 chars
// #define ADAPTER_IP_ADDRESS 10,10,30,31
#define ADAPTER_IP_ADDRESS 192,168,1,1

// #define HUGHES_MODEM_IP_ADDRESS 192,168,0,1
#define GATEWAY 192,168,1,1
#define SUBNET 255,255,255,0
#define MAC_ADDR {0x02, 0xAB, 0x56, 0x78, 0x9A, 0xBC} //02:AB:CD:EF:12:34
#define OPENAMIP_PORT 5000
#define OPENAMIP_PORT2 6000
#define CONF_PORT_TELNET 23
#define CONF_WEB_SERVER_PORT 80
#define END_MARKER '\r'  // CARRIAGE RETURN
#define MAXPACKETSIZE_CONFIG 200

  
// Port configuration
// #define PORT_FOR_LOCAL_CNX 6262  //disabled for testing
// #define PORT_FOR_LOCAL_CNX 1234  //enabled for testing

// #define PORT_FOR_REMOTE_CNX 7727

extern EthernetUDP myUdp;

#define EEPROM_VALID_VALUE_FLAG 1

#define EEPROM_LOC_START 80
#define EEPROM_LOC_END 150

#define EEPROM_LOC_IP_VALID_FLAG 80
#define EEPROM_LOC_IP_OCT1 81
#define EEPROM_LOC_IP_OCT2 82
#define EEPROM_LOC_IP_OCT3 83
#define EEPROM_LOC_IP_OCT4 84

#define EEPROM_LOC_GATEWAY_VALID_FLAG 85
#define EEPROM_LOC_GATEWAY_OCT1 86
#define EEPROM_LOC_GATEWAY_OCT2 87
#define EEPROM_LOC_GATEWAY_OCT3 88
#define EEPROM_LOC_GATEWAY_OCT4 89

#define EEPROM_LOC_SUBNET_VALID_FLAG 90
#define EEPROM_LOC_SUBNET_OCT1 91
#define EEPROM_LOC_SUBNET_OCT2 92
#define EEPROM_LOC_SUBNET_OCT3 93
#define EEPROM_LOC_SUBNET_OCT4 94

#define EEPROM_LOC_MAC_VALID_FLAG 95
#define EEPROM_LOC_MAC_OCT1 96
#define EEPROM_LOC_MAC_OCT2 97
#define EEPROM_LOC_MAC_OCT3 98
#define EEPROM_LOC_MAC_OCT4 99
#define EEPROM_LOC_MAC_OCT5 100
#define EEPROM_LOC_MAC_OCT6 101



#define EEPROM_LOC_OPENAMIP_PORT_VALID_FLAG 112
#define EEPROM_LOC_OPENAMIP_PORT 113  // 4 bytes

#define EEPROM_LOC_BAUD_RATE_VALID_FLAG 117
#define EEPROM_LOC_BAUD_RATE 118  // 4 bytes


#define DEFAULT_BAUD_RATE 115200
#define LAN_RST_PIN 5 //PE3




extern void configServerTasks();


#define FALSE 0
#define TRUE 1

extern char g_eth_rx_buff[100];

extern void init_all_peripherals(void);

extern boolean g_GPSDataReady;
extern TinyGPSPlus g_gps;
extern EthernetServer g_server;
extern EthernetServer g_server2;
extern EthernetClient g_client[8];
extern EthernetClient g_tempC;

extern boolean g_gotValidDataFromGPS;

void init_all_peripherals(void);
int searchChar(char *buff, char c, size_t start, size_t end);


extern uint32_t prevMillis;
extern uint32_t timeTicks_mptUpTime;

extern void openAMIPserver();
extern void openAMIPserver2();
extern boolean g_alreadyConnected[8];
extern boolean g_alreadyConnected2;
extern int resetLAN(void);
extern int periodic_lan_reset(void);

float fround(float var);
extern EthernetClient g_AdapterAsClient;
extern char g_header[];
extern unsigned char hex2byte(const char *hex);
extern void ipAddressToString(const IPAddress &ip, char *buffer, size_t bufferSize);
extern void writeLong(int address, uint32_t value);
extern const char html_head[];
extern void sendWebserverData();
uint32_t readLong(int address);
#endif