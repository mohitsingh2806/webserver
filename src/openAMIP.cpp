
#include "openAMIP.h"
#include "misc.h"

EthernetClient g_client[8];
boolean g_alreadyConnected[8] = {false, false, false, false, false, false, false, false};
boolean g_TCPPktTransferComplete[8] = {false, false, false, false, false, false, false, false};
char g_receivedCharsfromMODEM[MAXPACKETSIZE_OPENAMIP][8] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
boolean g_modemDataReady[8] = {false, false, false, false, false, false, false, false};
uint32_t g_openamip_alive_interval[8] = {1, 1, 1, 1, 1, 1, 1, 1};
boolean g_alive_flag_updated[8] = {false, false, false, false, false, false, false, false};
uint8_t g_openamip_where_interval[8] = {255, 255, 255, 255, 255, 255, 255, 255};
char g_header[] = "w 1 "; // for testing
// char g_header[] = "w 0 ";
char g_latSign = '+', g_longSign = '+';
char space = ' ';
char footer[] = " 0 0 0 0 0 0 0";
char CR = '\r';
char NL = '\n';
char g_LAT[9] = "28.6692", g_LONG[9] = "77.4538";
char g_STATUS_OPENAMIP[] = "s 1 1 0 0";

void openAMIPserver()
{
  static uint32_t lastTime[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  char *ret[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
  EthernetClient tempC;
  static boolean recvInProgress[8] = {
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
  };
  static byte ndx[8] = {255, 255, 255, 255, 255, 255, 255, 255};
  char endMarker = END_MARKER_OPENAMIP;
  char rc = 0;
  for (int i = 0; i < 8; i++)
  {
    if (!g_client[i].connected())
    {
      // g_client[i].print("Disconnecting Client #");
      // g_client[i].println(i);
      g_client[i].stop();             // Close the connection to free socket
      g_client[i] = EthernetClient(); // Reset client to default state
      g_alreadyConnected[i] = false;
      g_TCPPktTransferComplete[i] = false;
      g_modemDataReady[i] = false;
      recvInProgress[i] = false;
      ndx[i] = 255;
      g_alive_flag_updated[i] = false;
      g_openamip_alive_interval[i] = 2;          // Reset to default
      g_openamip_where_interval[i] = 255;        // Reset to default
      memset(g_receivedCharsfromMODEM[i], 0, 8); // Clear buffer
    }
  }
  tempC = g_server.accept();

  if (tempC)
  {

    for (byte i = 0; i < 8; i++)
    {
      if (!g_client[i])
      {
        // Once we "accept", the client is no longer tracked by EthernetServer
        // so we must store it into our list of clients
        g_client[i] = tempC;
        break;
      }
    }
  }

  for (int i = 0; i < 8; i++)
  {

    if (g_client[i])
    {

      if (g_alreadyConnected[i] == false)
      {

        g_client[i].write(g_header);
        g_client[i].write(g_latSign);
        g_client[i].write(g_LAT);
        g_client[i].write(space);
        g_client[i].write(g_longSign);
        g_client[i].write(g_LONG);
        g_client[i].write(footer);
        g_client[i].write(CR);
        g_client[i].write(NL);
        g_alreadyConnected[i] = true;
      }

      while (g_client[i].available() > 0 && g_TCPPktTransferComplete[i] == false)
      {

        rc = g_client[i].read();

        if (recvInProgress[i] == true)
        {

          if ((rc == endMarker) or (ndx[i] >= MAXPACKETSIZE_OPENAMIP - 1)) // either end of packet reached or buffer overflow
          {
            g_receivedCharsfromMODEM[i][ndx[i]] = '\0'; // terminate the string
            recvInProgress[i] = false;
            ndx[i] = 255;
            g_TCPPktTransferComplete[i] = true;
          }
          else
          {
            g_receivedCharsfromMODEM[i][ndx[i]] = rc;

            ndx[i]++;
          }
        }
        else if (ndx[i] == 255) // condition where data is starting to come
        {

          recvInProgress[i] = true;
          g_modemDataReady[i] = false;
          ndx[i] = 0;
          g_receivedCharsfromMODEM[i][ndx[i]] = rc;
          ndx[i]++;
        }
      }

      if ((g_alive_flag_updated[i]) && (millis() - lastTime[i] > ((g_openamip_alive_interval[i] - 1) * 1000)))
      { // keep alive

        lastTime[i] = millis();
        g_client[i].write(g_header);
        g_client[i].write(g_latSign);
        g_client[i].write(g_LAT);
        g_client[i].write(space);
        g_client[i].write(g_longSign);
        g_client[i].write(g_LONG);
        g_client[i].write(footer);
        g_client[i].write(CR);
        g_client[i].write(NL);
      }

      if (g_TCPPktTransferComplete[i] == true)
      {

        ret[i] = strstr(g_receivedCharsfromMODEM[i], ALIVE_INTERVAL_MSG);
        if (ret[i] != NULL)
        {
          ret[i] = strtok(ret[i], " ");
          ret[i] = strtok(NULL, "\r");
          g_openamip_alive_interval[i] = atoi(ret[i]);
          if ((g_openamip_alive_interval[i] - 1) <= 0)
          {
            g_openamip_alive_interval[i] = 2;
          }
          g_alive_flag_updated[i] = true;
        }

        ret[i] = strstr(g_receivedCharsfromMODEM[i], WHERE_MSG);
        if (ret[i] != NULL)
        {
          ret[i] = strtok(ret[i], " ");
          ret[i] = strtok(NULL, "\r");
          g_openamip_where_interval[i] = atoi(ret[i]);

          g_client[i].write(g_header);
          g_client[i].write(g_latSign);
          g_client[i].write(g_LAT);
          g_client[i].write(space);
          g_client[i].write(g_longSign);
          g_client[i].write(g_LONG);
          g_client[i].write(footer);
          g_client[i].write(CR);
          g_client[i].write(NL);
        }

        ret[i] = strstr(g_receivedCharsfromMODEM[i], FIND_SAT_MSG);
        if (ret[i] != NULL)
        {

          g_client[i].write(g_STATUS_OPENAMIP); // need to get the status from control card before this based to ack lock status
          g_client[i].write(CR);
          g_client[i].write(NL);
        }

        g_modemDataReady[i] = true;
        g_TCPPktTransferComplete[i] = false;
      }
    }
  }
}
