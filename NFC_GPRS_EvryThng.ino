/*
  Copyright 2012 Javier Montaner
  
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
  
  http://www.apache.org/licenses/LICENSE-2.0
  
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/*
  This software has been tested with Arduino Uno, SeeedStudio NFC shield and 
  SeeedStudio GPRS shield with one SIM card from Yoigo (Spanish Mobile Network 
  Operator - MNO).
  The PIN of the SIM card is disabled so no PIN validation is performed.
  Update the mobile settings (APN, UserName, Password) with the data of your MNO.
  
  No library is provided by SeeedStudio for managing the GPRS shield so all the AT commands 
  are sent as byte strings over the serial interface. This is a bit tricky specially when 
  the handling of the responses must work in parallel to the reading of NFC tags.
  Note that the http response data received from evrythng server is not fully 
  read/parsed. Further work would be required to process the whole repsonse (currently only the 
  first 63 bytes of the response are read - I think that this is due to an internal buffer
  in the SIM900 GPRS module.
*/
 
/*
  //WARNING: This function must be added to the PN532 library downloaded from SeeedStudio
  void PN532::RFConfiguration(uint8_t mxRtyPassiveActivation) {
  pn532_packetbuffer[0] = PN532_RFCONFIGURATION;
  pn532_packetbuffer[1] = PN532_MAX_RETRIES;
  pn532_packetbuffer[2] = 0xFF; // default MxRtyATR
  pn532_packetbuffer[3] = 0x01; // default MxRtyPSL
  pn532_packetbuffer[4] = mxRtyPassiveActivation;
  
  sendCommandCheckAck(pn532_packetbuffer, 5);
  // ignore response!
  }
*/

#include <SoftwareSerial.h>
#include <PN532.h>


#define SCK 13
#define MOSI 11
#define SS 10
#define MISO 12

PN532 nfc(SCK, MISO, MOSI, SS);
unsigned long time;
unsigned long gprsTime;
uint32_t gprsState =0;

int ledPin =9;  
int redLedPin=6;

uint32_t tagId=0;
uint32_t xmitId=0;
uint32_t index=0;

uint32_t flowState =0;
uint32_t prevFowState =0;
uint32_t nextFlowState =0;

uint8_t repeatCounter;
uint8_t repeatThreshold;

uint32_t sendATState =0;

const unsigned short MAX_LENGTH_AT_COMMAND = 100;
const unsigned short MAX_LENGTH_AT_RESPONSE = 100;


char atCommandBuffer [MAX_LENGTH_AT_COMMAND];
char atResponseBuffer [MAX_LENGTH_AT_RESPONSE];
char atCompareResponseBuffer [MAX_LENGTH_AT_RESPONSE];

unsigned short nbrCharsATResponse = 0;
unsigned short nbrCharsATExpectedResponse = 0;
unsigned short nbrCharsATCommand = 0;
int num_of_bytes=0;

#define STATE_IDDLE 0
#define STATE_SENDING_AT_COMMAND 10
#define STATE_CIPSTART 15
#define STATE_CIPSEND 20
#define STATE_CIPSEND_CONTENT 25
#define STATE_CIPSHUT 30
#define STATE_SUCCESSFUL_AT_COMMAND 35


#define STATE_CIPSHUT_1 15
#define STATE_CIPMUX 20
#define STATE_CIPMODE 25
#define STATE_CGDCONT 32
#define STATE_CSTT 35
#define STATE_CIICR 40
#define STATE_CIFSR 45
#define STATE_CIPSTART_1 50
#define STATE_CIPSHUT_2 55
#define STATE_SUCCESSFUL_INIT 100

#define AT_STATE_WS 1
#define AT_STATE_WFC 2
#define AT_STATE_WLC 3
#define AT_STATE_CHECK_RESPONSE 4

long time_processingAT=0;
long time_ws=0;
long time_wfc=0;
long timeout_wfc=0;
long time_wlc=0;
long timeout_wlc=0;


char  tagIdString [11]= "1234567890";

SoftwareSerial GPRS_Serial(7, 8);



void setup()
{
  pinMode(ledPin, OUTPUT);     
  pinMode(redLedPin, OUTPUT);   
  time = millis();
  GPRS_Serial.begin(19200);  //GPRS Shield baud rate
  Serial.begin(19200);
  digitalWrite(redLedPin, HIGH);      
  nfc.begin();
  nfc.RFConfiguration(0x14); // default is 0xFF (try forever; ultimately it does time out but after a long while
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
  }else {
    // Got ok data, print it out!
    Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
    Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
    Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
    Serial.print("Supports "); Serial.println(versiondata & 0xFF, HEX);
 
   digitalWrite(redLedPin, LOW);      
    // configure board to read RFID tags and cards
    nfc.SAMConfig();
  }
  digitalWrite(redLedPin, LOW);

  digitalWrite(ledPin, HIGH);      
  flowState= STATE_IDDLE;
  while (flowState!=STATE_SUCCESSFUL_INIT) {

       switch (flowState){
         case (STATE_SENDING_AT_COMMAND): 
            processATState();
            break;
         case (STATE_IDDLE):
           GPRS_Serial.flush();
           delay(10000);
           SendAtCommand ( STATE_IDDLE, STATE_CIPSHUT_1, 2, 500, 200, "ATE0", "\r\nOK\r\n"); 
           break;
         case (STATE_CIPSHUT_1):
           SendAtCommand ( STATE_CIPSHUT_1, STATE_CIPMUX, 1, 3000, 100, "AT+CIPSHUT", "\r\nSHUT OK\r\n");
           break;
         case (STATE_CIPSHUT):
           flowState=STATE_IDDLE; // due to timeout exit state from processAtState
           break;         
         case (STATE_CIPMUX):
           SendAtCommand ( STATE_CIPMUX, STATE_CIPMODE, 1, 3000, 100, "AT+CIPMUX=0", "\r\nOK\r\n");
           break;
         case (STATE_CIPMODE):
           SendAtCommand ( STATE_CIPMODE, STATE_CGDCONT, 1, 3000, 100, "AT+CIPMODE=0", "\r\nOK\r\n"); 
           break;
         case (STATE_CGDCONT):
           // Update values with the settings of your Mobile Netwrok Operator
           SendAtCommand ( STATE_CGDCONT, STATE_CSTT, 1, 3000, 100, "AT+CGDCONT=1,\"IP\",\"internet\",\"\",0,0", "\r\nOK\r\n");
           break;
         case (STATE_CSTT):
           // Update values with the settings of your Mobile Netwrok Operator
           SendAtCommand ( STATE_CSTT, STATE_CIICR, 1, 3000, 100, "AT+CSTT=\"internet\",\"\",\"\"", "");
           break;
         case (STATE_CIICR):
           SendAtCommand ( STATE_CIICR, STATE_CIFSR, 2, 20000, 300, "AT+CIICR", "\r\nOK\r\n");
           break;
         case (STATE_CIFSR):
           SendAtCommand ( STATE_CIFSR, STATE_CIPSTART_1, 2, 5000, 2000, "AT+CIFSR", "");
           break;
         case (STATE_CIPSTART_1):
           SendAtCommand ( STATE_CIPSTART_1, STATE_CIPSHUT_2, 2, 5000, 2000, "AT+CIPSTART=\"TCP\",\"www.evrythng.net\",\"80\"", "\r\nOK\r\n\r\nCONNECT OK\r\n");
           break;
         case (STATE_CIPSHUT_2):
           SendAtCommand ( STATE_CIPSHUT_2, STATE_SUCCESSFUL_INIT, 1, 3000, 100, "AT+CIPSHUT", "\r\nSHUT OK\r\n");
           break;
       }
  }

    digitalWrite(ledPin, LOW);
    delay(200);   
    digitalWrite(ledPin, HIGH);    
    delay(200);
    digitalWrite(ledPin, LOW);        
    flowState=STATE_IDDLE;
    delay(2000);


}



void loop()
{ 

  if (millis()-time > 1000){
    // look for MiFare type cards
    digitalWrite(ledPin, LOW);   // sets the LED on    
    time=millis();
    tagId = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A);
    if (tagId != 0) 
    {
      
      digitalWrite(ledPin, HIGH);   // sets the LED on
      Serial.print("Read card #"); Serial.println(tagId);
      xmitId=0;
      uint32_t divisor= 1000000000;
      for (int i=0;i<10; i++){
         tagIdString [i]= char(48 + tagId/divisor);
         tagId=tagId%divisor;
         divisor /=10;       
      }
      Serial.print("Converted String: "); 
      Serial.println(tagIdString);
      time=millis();
      Serial.print("FlowState: "); Serial.println(flowState);
      if (flowState==STATE_IDDLE) flowState=STATE_CIPSTART;
      return;
     }
  }   

  if ((flowState== STATE_SENDING_AT_COMMAND)&& (millis() - time_processingAT >100)) {
      processATState();
      time_processingAT=millis();
  }
 
  if (flowState== STATE_CIPSTART) {
      SendAtCommand ( STATE_CIPSTART, STATE_CIPSEND, 2, 5000, 2000, "AT+CIPSTART=\"TCP\",\"www.evrythng.net\",\"80\"", "\r\nOK\r\n");
      GPRS_Serial.flush();  
  }


  
  if (flowState== STATE_CIPSEND) {
      delay(1000);
      SendAtCommand ( STATE_CIPSEND, STATE_CIPSEND_CONTENT, 2, 2000, 100, "AT+CIPSEND", "\r\n> ");	
      GPRS_Serial.flush();
  }
 
 
  if ((flowState==STATE_CIPSEND_CONTENT)) {
          // Update value of yourThingId
          GPRS_Serial.println("PUT http://evrythng.net/thngs/...yourThingIdHere.../properties/ReadTag HTTP/1.1");
          GPRS_Serial.println("Content-Type: application/json");
          GPRS_Serial.println("Accept: application/vnd.evrythng-v2+json");
          // Update value of yourAPIToken
          GPRS_Serial.println("X-Evrythng-Token: ...yourAPITokenHere..."); 
          GPRS_Serial.println("Host: evrythng.net");
          GPRS_Serial.println("Content-Length: 45"); 
          GPRS_Serial.println(""); 
          GPRS_Serial.print("{\"key\": \"ReadTag\",\"value\":  \""); GPRS_Serial.print(tagIdString); GPRS_Serial.println("\"}"); 
          GPRS_Serial.println(""); 
          GPRS_Serial.println(""); 
          

          GPRS_Serial.print(0x1A, 0); //Arduino 1.0
          delay(300);
          flowState= STATE_CIPSHUT;
          //GPRS_Serial.flush();

          digitalWrite(ledPin, HIGH);          
          delay(100);
          digitalWrite(ledPin, LOW);
          delay(100);
          digitalWrite(ledPin, HIGH);
          delay(100);
          digitalWrite(ledPin, LOW);

    }
  

  
  if (flowState==STATE_CIPSHUT)  {
      delay(300);
      GPRS_Serial.flush();	
      SendAtCommand(STATE_CIPSHUT, STATE_SUCCESSFUL_AT_COMMAND, 1, 3000, 100, "AT+CIPSHUT", "");
      GPRS_Serial.flush();	
  }
  
  if (flowState==STATE_SUCCESSFUL_AT_COMMAND) {
          GPRS_Serial.flush();	
          digitalWrite(redLedPin, HIGH);          
          delay(100);
          digitalWrite(redLedPin, LOW);
          delay(100);
          digitalWrite(redLedPin, HIGH);
          delay(100);
          digitalWrite(redLedPin, LOW);
          flowState= STATE_IDDLE;
  }
  
  
}
 
 
 
 
 
 
 
 
 
char GPRS_Serial_wait_for_bytes(char no_of_bytes, int timeout)
{
  while(GPRS_Serial.available() < no_of_bytes)
  {
    delay(200);
    timeout-=1;
    if(timeout == 0)
    {
      return 0;
    }
  }
  return 1;
}


void SendAtCommand (uint32_t previousState, uint32_t nextState, uint8_t repeatThr, long to_wfc, long to_wlc, char *atCommand, char *expectedResponse)
{
  if (flowState==STATE_SENDING_AT_COMMAND){
     Serial.println("Already processing another AT command");
  } else {
    prevFowState=previousState;
    nextFlowState=nextState;
    sendATState=AT_STATE_WS;
    repeatCounter=1;
    repeatThreshold=repeatThr;
    timeout_wfc=to_wfc;
    timeout_wlc=to_wlc;
    num_of_bytes=0;
    while ( (atCommand[num_of_bytes]!=0) && (num_of_bytes<(MAX_LENGTH_AT_COMMAND - 1))){
      atCommandBuffer[num_of_bytes] = atCommand[num_of_bytes];
      num_of_bytes++;
    }
    atCommandBuffer[num_of_bytes]=0;
    nbrCharsATCommand=num_of_bytes;

    num_of_bytes=0;
    while ( (expectedResponse[num_of_bytes]!=0) && (num_of_bytes<(MAX_LENGTH_AT_RESPONSE - 1))){
      atCompareResponseBuffer[num_of_bytes] = expectedResponse[num_of_bytes];
      num_of_bytes++;
    }
    atCompareResponseBuffer[num_of_bytes] = 0;
    nbrCharsATExpectedResponse=num_of_bytes;
    time_processingAT=millis();
    flowState=STATE_SENDING_AT_COMMAND;
    
  }
}

void processATState ()
{
 switch (sendATState) {
    case AT_STATE_WS:
      if (((millis()-time_ws>=1000)&&(repeatCounter>1))||(repeatCounter==1)){
        GPRS_Serial.println(atCommandBuffer);
        Serial.println("Sending AT command...");
        Serial.println(atCommandBuffer);
        time_wfc=millis();
        sendATState= AT_STATE_WFC;
      }
      break;
 
    case AT_STATE_WFC:
      if (uint32_t nbrChar= GPRS_Serial.available()) {
        time_wlc=millis();
        Serial.print("Received some AT response ");
        Serial.println(nbrChar);
        nbrCharsATResponse=0;
        sendATState= AT_STATE_WLC;
      } else {
        if ((millis()-time_wfc)>=timeout_wfc){
          repeatCounter++;
          if (repeatCounter<=repeatThreshold) {
            Serial.println("Timeout before receiving any AT command response data. Retry AT command");            
            time_ws=millis();
            sendATState= AT_STATE_WS;            
          } else {
            Serial.println("Timeout before receiving any AT response data. No more repetitions. Back to iddle state");
            flowState=STATE_CIPSHUT;
          }
        }
      }
      break;

    case AT_STATE_WLC:
      num_of_bytes = GPRS_Serial.available(); 
     // Serial.print ("Received bytes "); 
     // Serial.println(num_of_bytes);
      if (num_of_bytes<=0) {
        if ((millis()-time_wlc)>=timeout_wlc){ // end of reading
            Serial.println(atResponseBuffer);
            sendATState= AT_STATE_CHECK_RESPONSE;  
        }            
      } else {
          for (uint16_t ind= 1; ind<=num_of_bytes; ind++) {
     //         Serial.print("reading buffer ");
     //         Serial.println (ind);            
              if (nbrCharsATResponse < (MAX_LENGTH_AT_RESPONSE - 1)) { 
                  atResponseBuffer[nbrCharsATResponse] = GPRS_Serial.read();  
    //              Serial.print(atResponseBuffer[nbrCharsATResponse]);
                  atResponseBuffer[++nbrCharsATResponse] = 0x00; 
              } 
              else { 
                  // comm buffer is full, other incoming characters 
                  // will be discarded 
                   Serial.print("x");
                   Serial.print(GPRS_Serial.read());                  
              }
           } 
        time_wlc = millis(); 
      } 

      break;

    case AT_STATE_CHECK_RESPONSE:
      num_of_bytes=0;
      while (num_of_bytes<=nbrCharsATResponse) {
        Serial.print(atResponseBuffer[num_of_bytes++],HEX);        
        Serial.print("-");
      }
      Serial.println();
      Serial.println(atResponseBuffer);
      num_of_bytes=0;
      while ((atCompareResponseBuffer[num_of_bytes]!=0)&&(num_of_bytes < (MAX_LENGTH_AT_RESPONSE - 1))) {
        if (atCompareResponseBuffer[num_of_bytes]!=atResponseBuffer[num_of_bytes]) {
            repeatCounter++;
            if (repeatCounter<=repeatThreshold) {
              Serial.println("Received AT response different from expected response. Retry AT command");            
              time_ws=millis();
              sendATState= AT_STATE_WS;     
              return;       
            } else {
              Serial.println("Received AT response different from expected response. Back to iddle state");
              flowState=STATE_CIPSHUT;
              return;
            }
        }
        num_of_bytes++;
      }
      Serial.println("Successful AT command");
      flowState= nextFlowState;
      break;

      
    default: 
        Serial.println("Error in state while proecessing AT command. Reset to iddle state");
        flowState=STATE_IDDLE;
  }

}


