#ifndef OPENAMIP_H
#define OPENAMIP_H
#include <Arduino.h>
void openAMIP_Server(void);
#define END_MARKER_OPENAMIP '\r'
#define LF '\n'
#define MAXPACKETSIZE_OPENAMIP 100

#define ALIVE_INTERVAL_MSG "A "
#define BEAT_FREQ_MSG "B "
#define CNR_MSG "C "
#define EXPECTED_POWER_MSG "E "
#define FIND_SAT_MSG "F"
#define HUNT_FREQ_MSG "H " 
#define MODEM_ID_MSG "I "
#define SKEW_MSG "K "
#define LOCK_STATUS_MSG "L "       // its interval is set by a command from ACU
#define POLARIZATION_MODE_MSG "P " // modem commands antenna to use these polarisations
#define SAT_LONG_MSG "S "
#define TRANSMIT_FREQ_MSG "T "
#define WHERE_MSG "W "
#define EXTRA_HUNT_PARAMS_MSG "X "
  
// extern uint32_t g_openamip_alive_interval;
// extern uint8_t g_openamip_where_interval;
extern char g_STATUS_OPENAMIP[];
extern boolean g_TCPPktTransferComplete[8];
extern char g_LAT[9];
extern char g_LONG[9];
extern char g_latSign;
extern char g_longSign;
extern char g_openAMIP_w_string[];
extern boolean g_gotLatLongFromUser;
#endif