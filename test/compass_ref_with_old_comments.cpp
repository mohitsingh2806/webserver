#include "compass.h"
#include "misc.h"
#include <math.h>
//                                                     $HCHDM,abc.d,M*<checksumMS><checksumLS><cr><lf>
//                                                      HCHDM,abc.d,M   => used for checksum
boolean compassDataReady = false;   //use it to signal packetiseSensorData() function that data is ready to send
byte head[3]={0,0,0};


char receivedCharsfromCompass[MAXPACKETSIZE_COMPASS];
boolean compassSerialTransferComplete = false;


unsigned int heading = 0;

void receiveDataFromCompass(void) {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = START_MARKER_COMPASS;
  char endMarker = END_MARKER_COMPASS;
  char rc;
  

  while (Serial3.available() > 0 && compassSerialTransferComplete == false) {
    rc = Serial3.read();
    if (recvInProgress == true) {
      if ((rc == endMarker) or (ndx >= MAXPACKETSIZE_COMPASS - 1)) //either end of packet reached or buffer overflow
      {
        receivedCharsfromCompass[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        compassSerialTransferComplete = true;
      }

      else  {
        receivedCharsfromCompass[ndx] = rc;
        ndx++;


      }
    }
    else if (rc == startMarker) {
      recvInProgress = true;


    }
  }

}


//                                                     $HCHDM,abc.d,M*<checksumMS><checksumLS><cr><lf>
//                                                      HCHDM,abc.d,M   => used for checksum
void processDataFromCompass(void) {
  byte i = 0,j=0;
  char received_checksum_array[2] = "";
  char heading_array[4]="";

  if (compassSerialTransferComplete == true) {

//PRINT ENTIRE RECEIVED MESSAGE
// int index =0;
// while(receivedCharsfromCompass[index] != '\0'){
//   Serial.print(index);
//   Serial.print(":        ");
//   Serial.println(receivedCharsfromCompass[index]);
//   index++;
// }

    /****************NEW*****************/
    byte calc_checksum = 0;
    unsigned char asterisk_location = searchChar(receivedCharsfromCompass, MAXPACKETSIZE_COMPASS , '*', 0, MAXPACKETSIZE_COMPASS - 1);
    unsigned char end_location = searchChar(receivedCharsfromCompass, MAXPACKETSIZE_COMPASS , '\0', 0, MAXPACKETSIZE_COMPASS - 1);



    for ( byte i = asterisk_location + 1; i < end_location; i++)
    {
      received_checksum_array[i - (asterisk_location + 1)] = receivedCharsfromCompass[i];
    }

    byte received_checksum = (((received_checksum_array[0] >= 'A') ? received_checksum_array[0] - 'A' + 10 : received_checksum_array[0] - '0') * 16) + ((received_checksum_array[1] >= 'A') ? received_checksum_array[1] - 'A' + 10 : received_checksum_array[1] - '0');

 


    while (receivedCharsfromCompass[i] != '*') {
      calc_checksum = calc_checksum ^ receivedCharsfromCompass[i];
      i++;
    }
 
    /****************NEW*****************/



    /****************OLD*****************/
    // byte calc_checksum = receivedCharsfromCompass[0] ^ receivedCharsfromCompass[1] ^ receivedCharsfromCompass[2] ^ receivedCharsfromCompass[3] ^ receivedCharsfromCompass[4] ^ receivedCharsfromCompass[5] ^ receivedCharsfromCompass[6] ^ receivedCharsfromCompass[7] ^ receivedCharsfromCompass[8] ^ receivedCharsfromCompass[9] ^ receivedCharsfromCompass[10] ^ receivedCharsfromCompass[11] ^ receivedCharsfromCompass[12];
    // byte received_checksum = ((receivedCharsfromCompass[14] - '0') * 16) + (receivedCharsfromCompass[15] - '0');  // need to change this if used, see new code
    /****************OLD*****************/


//                                                     $HCHDM,abc.d,M*<checksumMS><checksumLS><cr><lf>
//                                                      HCHDM,abc.d,M   => used for checksum

    if (calc_checksum == received_checksum) {
      heading =0;
      /******************NEW*****************/

      unsigned char firstCommaLocation = searchChar(receivedCharsfromCompass, MAXPACKETSIZE_COMPASS , ',', 0, MAXPACKETSIZE_COMPASS - 1);
      unsigned char secondCommaLocation = searchChar(receivedCharsfromCompass, MAXPACKETSIZE_COMPASS , ',', firstCommaLocation+1, MAXPACKETSIZE_COMPASS - 1);

      for(int i= firstCommaLocation+1; i< secondCommaLocation ; i++){
          if(receivedCharsfromCompass[i] == '.'){
            continue;
          }
          heading_array[j] = receivedCharsfromCompass[i];

          j++;
      }
      for (int i = 0; i< 4 ; i++)
      {  //4 digit heading
      heading =  (unsigned int)(heading_array[i] - '0')    +        (10* heading) ;
            // Serial.print(heading_array[i]);
            // Serial.print(" * ");
            // Serial.print(pow(10,(3-i) )); 
            // Serial.print(" = ");
            // Serial.println((unsigned int)(heading_array[i] - '0') * pow(10,(3-i) ) );

      }
      /******************NEW*****************/



      /******************OLD*****************/
      // heading = (long)(receivedCharsfromCompass[6] - '0') * 1000 + (long)(receivedCharsfromCompass[7] - '0') * 100 + (receivedCharsfromCompass[8] - '0') * 10 + (receivedCharsfromCompass[10] - '0');
      /******************OLD*****************/
      Serial.println(heading);


      //       for (int i = 0; i < 4 ; i++)
//       {
//         //4 digit heading
//         heading =  (unsigned int)(heading_array[i] - '0')    +        (10 * heading) ;

// //      0 <ha0>
// //   <ha1> <ha2>
// //   <ha3> 0

//       }
//directly convert heading array to head

          // Serial.print("heading :          ");
          //  Serial.println(heading);
 
          head[0] = (heading_array[0] - '0');
          head[1] = ((heading_array[1] - '0') * 10 )+ (heading_array[2] - '0');
          head[2] = (heading_array[0] - '0')<<4;

          // head[0] = heading / 1000;
          // head[1] = (heading - ((long)head[0] * 1000)) / 10;
          // head[2] = (heading - ((long)head[0] * 1000) - (long)head[1] * 10);
      Serial.print(" HEAD array:           ");
      Serial.print(head[0] , HEX);
      Serial.print(head[1],HEX );
      Serial.println(head[2] ,HEX);
     compassDataReady =true;
    }
    compassSerialTransferComplete = false;
  }

}
