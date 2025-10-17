

#include "misc.h"
#include <Arduino.h>
int main(void)
{
  init_all_peripherals(); // initialisation related tasks

  while (true)
  {
    wdt_reset(); // reset the watchdog timer, need to reset before 4 seconds

    receiveDataFromGPS();  // change valid flag before prod code fusing

    openAMIPserver(); 
    configServerTasks(); // configuration change task
    periodic_lan_reset(); // may not be required
  }
  return 0; // never reached 
}

