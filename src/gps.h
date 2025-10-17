#ifndef GPS_H
#define GPS_H
#include <Arduino.h>

#define START_MARKER_GPS 0x24 //$
#define END_MARKER_GPS 0x0D   // <cr>

#define MAXPACKETSIZE_GPS 100

extern void receiveDataFromGPS(void);
  
#endif