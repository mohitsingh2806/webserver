#include "gps.h"
#include "misc.h"

boolean GPSSerialTransferComplete = false;
boolean g_GPSDataReady = false; // use it to signal packetiseSensorData() function that data is ready to send
// float g_magnetic_declination = 0;
// char g_magnetic_declination_str[5] = {0};
TinyGPSPlus g_gps;

#define GPS_DATA_DELAY 5000
 
void receiveDataFromGPS(void)
{
  char rc = 0;

  char LAT[4] = {'\0'}, LONG[4] = {'\0'};
  // static uint32_t GPSTimer = millis();


  while ((Serial1.available() > 0))
  {
    rc = Serial1.read();
    g_gps.encode(rc); // give the data to TinyGPS library object
  }

  if (g_gps.location.isValid() ) // send data every 5 seconds
  {


    // GPSTimer = millis();

    g_latSign = g_gps.location.lat() < 0 ? '-' : '+';  // for openamip
    g_longSign = g_gps.location.lng() < 0 ? '-' : '+'; // for openamip
    dtostrf(abs(g_gps.location.lat()), 7, 5, g_LAT);   // for openamip
    dtostrf(abs(g_gps.location.lng()), 7, 5, g_LONG);  // for openamip

    g_header[2] = '1'; // openAMIP location valid flag
    float f_lat = abs(g_gps.location.lat());
    LAT[0] = (int)f_lat;
    LAT[1] = (int)((f_lat - LAT[0]) * 100);
    int temp = (int)(f_lat * 100);
    LAT[2] = (int)(((f_lat * 100) - temp) * 100);

    float f_long = abs(g_gps.location.lng());
    LONG[0] = (int)f_long;
    LONG[1] = (int)((f_long - LONG[0]) * 100);
    temp = (int)(f_long * 100);
    LONG[2] = (int)(((f_long * 100) - temp) * 100);

    

    

  }
  else 
  {
    LAT[0] = 0;
    LAT[1] = 0;
    LAT[2] = 0;

    LONG[0] = 0;
    LONG[1] = 0;
    LONG[2] = 0;

    
    // g_header[2] = '0';  for testing
  }
}
