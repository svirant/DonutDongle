/*
* RT4K Donut Dongle v0.5
* Copyright (C) 2025 @Donutswdad
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define PORT->Group[g_APinDescription[SDA].ulPort].PINCFG[g_APinDescription[SDA].ulPin].bit.PMUXEN = 0; // set SDA pin back to GPIO
#define PORT->Group[g_APinDescription[SCL].ulPort].PINCFG[g_APinDescription[SCL].ulPin].bit.PMUXEN = 0; // set SCL pin back to GPIO
#define IR_SEND_PIN 11  // Optional IR LED Emitter for RT5X compatibility. Sends IR data out Arduino pin D11
#define IR_RECEIVE_PIN 2 // Optional IR Receiver on pin D2

#include "TinyIRReceiver.hpp"
#include <IRremote.h>       // found in the built-in Library Manager
#include <SoftwareSerial.h>
#include <AltSoftSerial.h>  // https://github.com/PaulStoffregen/AltSoftSerial in order to have a 3rd Serial port for 2nd Extron Switch / alt sw2
                            // Step 1 - Goto the github link above. Click the GREEN "<> Code" box and "Download ZIP"
                            // Step 2 - In Arudino IDE; goto "Sketch" -> "Include Library" -> "Add .ZIP Library"
/*
////////////////////
//    OPTIONS    //
//////////////////
*/
//debug
uint8_t debugGscart1 = 0; // line ~779
uint8_t debugGscart2 = 0; // line ~916
uint8_t debugECAPbytes = 1; // line ~287

uint8_t SVS = 0; //     "Remote" profiles are profiles that are assigned to buttons 1-12 on the RT4K remote. "SVS" profiles reside under the "/profile/SVS/" directory 
             //     on the SD card.  This option allows you to choose which ones to call when a console is powered on.  Remote profiles allow you to easily change 
             //     the profile being used for a console's switch input if your setup is in flux. SVS require you to rename the file itself on the SD card which is 
             //     a little more work.  Regardless, SVS profiles will need to be used for console switch inputs over 12.
             //
             // **** Make sure "Auto Load SVS" is "On" under the RT4K Profiles menu. A requirement for most options ****
             //
             // 0 - use "Remote" profiles 1-12 for up to 12 inputs on 1st Extron Switch and SVS 13 - 99 for everything over 12. Only SVS profiles are used on 2nd Extron Switch if connected.
             //
             //     - remote profiles 1-12 for 1st Extron or TESmart Switch (If DP0 below is set to true - remote profile 12 is used when all ports are in-active)
             //     - SVS  12 - 99  for 1st Extron or TESmart (DP0 is true)
             //     - SVS  13 - 99  for 1st Extron or TESmart (DP0 is false)
             //     - SVS 101 - 199 for 2nd Extron or TESmart
             //     - SVS 201 - 208 for 1st gScart
             //     - SVS 209 - 216 for 2nd gScart
             //
             // 1 - use only "SVS" profiles.
             //     Make sure "Auto Load SVS" is "On" under the RT4K Profiles menu
             //     RT4K checks the /profile/SVS subfolder for profiles and need to be named: "S<input number>_<user defined>.rt4"
             //     For example, SVS input 2 would look for a profile that is named S2_SNES…rt4
             //     If there’s more than one profile that fits the pattern, the first match is used
             //
             //     - SVS   1 -  99 for 1st Extron or TESmart
             //     - SVS 101 - 199 for 2nd Extron or TESmart
             //     - SVS 201 - 208 for 1st gScart
             //     - SVS 209 - 216 for 2nd gScart
             //     - SVS   0       for DP0 option mentioned below
             //
             //  ** If DP0 below is set to true, create "S0_<user defined>.rt4" for when all ports are in-active. Ex: S0_DefaultHDMI.rt4
             //
             // 2 - use "Remote" profiles 1-12 for gscart/gcomp switches. Remote profile 1-8 for 1st gscart switch, 9-12 for inputs 1-4 on 2nd gscart switch.
             //     inputs 5-8 on the 2nd gscart switch will use SVS profiles 213 - 216
             //
             //     - remote profiles 1-8 for 1st gScart, 9 - 12 for first 4 inputs on 2nd gScart (If DP0 below is set to true - remote profile 12 is used when all ports are in-active)
             //     - SVS 213 -  216 for remaining inputs 5 - 8 on 2nd gScart 
             //     - SVS   1 -  99 for 1st Extron or TESmart
             //     - SVS 101 - 199 for 2nd Extron or TESmart
             //
             //
             //



bool DP0  = false;       // (Default Profile 0) 
                         //
                         //
                         // set true to load "Remote" profile 12 (if SVS=0) when all ports are in-active on 1st Extron switch (and 2nd if connected). 
                         // You can assign it to a generic HDMI profile for example.
                         // If your device has a 12th input, SVS will be used instead. "IF" you also have an active 2nd Extron Switch, remote profile 12
                         // will only load if "BOTH" switches have all in-active ports.
                         // 
                         // 
                         // If SVS=1, /profile/SVS/ "S0_<user defined>.rt4" will be used instead of remote profile 12
                         //
                         // If SVS=2, remote profile 12 will be used for gscart/gcomp
                         //
                         //
                         // default is false // also recommended to set false to filter out unstable Extron inputs that can result in spamming the RT4K with profile changes 
                       


uint8_t voutMatrix[65] = {1,  // MATRIX switchers // by default ALL input changes to any/all outputs result in a profile change
                                                   // disable specific outputs from triggering profile changes
                                                   //
                           1,  // output 1 (1 = enabled, 0 = disabled)
                           1,  // output 2
                           1,  // output 3
                           1,  // output 4
                           1,  // output 5
                           1,  // output 6
                           1,  // output 7
                           1,  // output 8
                           1,  // output 9
                           1,  // output 10
                           1,  // output 11
                           1,  // output 12
                           1,  // output 13
                           1,  // output 14
                           1,  // output 15
                           1,  // output 16
                           1,  // output 17
                           1,  // output 18
                           1,  // output 19
                           1,  // output 20
                           1,  // output 21
                           1,  // output 22
                           1,  // output 23
                           1,  // output 24
                           1,  // output 25
                           1,  // output 26
                           1,  // output 27
                           1,  // output 28
                           1,  // output 29
                           1,  // output 30
                           1,  // output 31
                           1,  // output 32 (1 = enabled, 0 = disabled)
                               //
                               // ONLY USE FOR 2ND MATRIX SWITCH
                           1,  // 2ND MATRIX SWITCH output 1 (1 = enabled, 0 = disabled)
                           1,  // 2ND MATRIX SWITCH output 2
                           1,  // 2ND MATRIX SWITCH output 3
                           1,  // 2ND MATRIX SWITCH output 4
                           1,  // 2ND MATRIX SWITCH output 5
                           1,  // 2ND MATRIX SWITCH output 6
                           1,  // 2ND MATRIX SWITCH output 7
                           1,  // 2ND MATRIX SWITCH output 8
                           1,  // 2ND MATRIX SWITCH output 9
                           1,  // 2ND MATRIX SWITCH output 10
                           1,  // 2ND MATRIX SWITCH output 11
                           1,  // 2ND MATRIX SWITCH output 12
                           1,  // 2ND MATRIX SWITCH output 13
                           1,  // 2ND MATRIX SWITCH output 14
                           1,  // 2ND MATRIX SWITCH output 15
                           1,  // 2ND MATRIX SWITCH output 16
                           1,  // 2ND MATRIX SWITCH output 17
                           1,  // 2ND MATRIX SWITCH output 18
                           1,  // 2ND MATRIX SWITCH output 19
                           1,  // 2ND MATRIX SWITCH output 20
                           1,  // 2ND MATRIX SWITCH output 21
                           1,  // 2ND MATRIX SWITCH output 22
                           1,  // 2ND MATRIX SWITCH output 23
                           1,  // 2ND MATRIX SWITCH output 24
                           1,  // 2ND MATRIX SWITCH output 25
                           1,  // 2ND MATRIX SWITCH output 26
                           1,  // 2ND MATRIX SWITCH output 27
                           1,  // 2ND MATRIX SWITCH output 28
                           1,  // 2ND MATRIX SWITCH output 29
                           1,  // 2ND MATRIX SWITCH output 30
                           1,  // 2ND MATRIX SWITCH output 31
                           1,  // 2ND MATRIX SWITCH output 32 (1 = enabled, 0 = disabled)
                           };
                           


uint8_t RT5Xir = 1;      // 0 = disables IR Emitter for RetroTink 5x
                     // 1 = enabled for Extron sw1 switch, TESmart HDMI, or Otaku Games Scart Switch if connected
                     //     sends Profile 1 - 10 commands to RetroTink 5x. Must have IR LED emitter connected.
                     //     (DP0 - if enabled uses Profile 10 on the RT5X)
                     //
                     // 2 = enabled for gscart switch only (remote profiles 1-8 for first gscart, 9-10 for first 2 inputs on second gscart)

uint8_t RT4Kir = 0;      // 0 = disables IR Emitter for RetroTink 4K
                     // 1 = enabled for Extron sw1 switch, TESmart HDMI, or Otaku Games Scart Switch if connected
                     //     sends Profile 1 - 12 commands to RetroTink 4K. Must have IR LED emitter connected.
                     //     (DP0 - if enabled uses Profile 12 on the RT4K)
                     //
                     // 2 = enabled for gscart switch only (remote profiles 1-8 for first gscart, 9-12 for first 4 inputs on second gscart)


uint8_t auxprof[12] =    // Assign SVS profiles to IR remote profile buttons. 
                          // Replace 1, 2, 3, etc below with "ANY" SVS profile number.
                          // Press AUX8 then profile button to load. Must have IR Receiver connected and Serial connection to RT4K.
                          // 
                     {1,  // AUX8 + profile 1 button
                      2,  // AUX8 + profile 2 button
                      3,  // AUX8 + profile 3 button
                      4,  // AUX8 + profile 4 button
                      5,  // AUX8 + profile 5 button
                      6,  // AUX8 + profile 6 button
                      7,  // AUX8 + profile 7 button
                      8,  // AUX8 + profile 8 button
                      9,  // AUX8 + profile 9 button
                      10, // AUX8 + profile 10 button
                      11, // AUX8 + profile 11 button
                      12, // AUX8 + profile 12 button
                      };
                          
////////////////////////////////////////////////////////////////////////  

// gscart / gcomp Global variables
uint8_t fpdcprev = 0; // stores 50% duty cycle detection
uint8_t fpdcprev2 = 0;
uint8_t fpdccount = 0; // number of times a 50% duty cycle period has been detected
uint8_t fpdccount2 = 0;
uint8_t allgscartoff = 2; // 0 = at least 1 port is active, 1 = no ports are active, 2 = disconnected or not used yet
uint8_t allgscartoff2 = 2;
uint8_t samcc = 0; // ADC sample cycle counter
uint8_t samcc2 = 0;
uint8_t highcount[3] = {0,0,0}; // number of high samples recorded for bit0, bit1, bit2
uint8_t highcount2[3] = {0,0,0};
uint8_t bitprev[3] = {0,0,0}; // stores previous bit state
uint8_t bitprev2[3] = {0,0,0};
byte const apin[3] = {A0,A1,A2}; // defines analog pins used to read bit0, bit1, bit2
byte const apin2[3] = {A3,A4,A5};

// gscart / gcomp adjustment Global variables for port detection
float high = 1.2; // for gscart sw1, rise above this voltage for a high sample
float high2 = 1.2; // for gscart sw2,  rise above this voltage for a high sample
uint8_t dch = 15; // at least this many high samples per "samsize" for a high bit (~75% duty cycle)
uint8_t dcl = 5; // at least this many high samples and less than "dch" per "samsize" indicate all inputs are in-active (~50% duty cycle)
uint8_t samsize = 20; // total number of ADC samples required to capture at least 1 period
uint8_t fpdccountmax = 3; // number of periods required when in the 50% duty cycle state before a Default Profile 0 is triggered.

// Extron sw1 / alt sw1 software serial port -> MAX3232 TTL IC
#define rxPin 3 // sets Rx pin to D3 on Arduino
#define txPin 4 // sets Tx pin to D4 ...

// Extron Global variables
byte ecapbytes[13]; // used to store first 13 captured bytes / messages for Extron sw1 / alt sw1                    
String ecap; // used to store Extron status messages for 1st Extron in String format
String einput; // used to store first 4 chars of Extron input
String previnput = "discon"; // used to keep track of previous input
String eoutput; // used to store first 2 chars of Extron output

byte ecapbytes2[13]; // used to store first 13 captured bytes / messages for Extron sw2 / alt sw2
String ecap2; // used to store Extron status messages for 2nd Extron in String format
String einput2; // used to store first 4 chars of Extron input for 2nd Extron
String previnput2 = "discon"; // used to keep track of previous input
String eoutput2; // used to store first 2 chars of Extron output for 2nd Extron

SoftwareSerial extronSerial = SoftwareSerial(rxPin,txPin); // setup a software serial port for listening to Extron sw1 / alt sw1 
AltSoftSerial extronSerial2; // setup yet another serial port for listening to Extron sw2 / alt sw2. hardcoded to pins D8 / D9

// IR Global variables
uint8_t pwrtoggle = 0; // used to toggle remote power button command (on/off) when using the optional IR Receiver
uint8_t repeatcount = 0; // used to help emulate the repeat nature of directional button presses
uint8_t extrabuttonprof = 0; // used to keep track of AUX8 button presses for addtional button profiles
String svsbutton; // used to store 3 digit SVS profile when AUX8 is double pressed
uint8_t nument = 0; // used to keep track of how many digits have been entered for 3 digit SVS profile
IRsend irsend;


void setup(){

    pinMode(txPin,OUTPUT); // set pin mode for RX, no need to set TX b/c all pins are inputs by default
    initPCIInterruptForTinyReceiver(); // for IR Receiver
    Serial.begin(9600); // set the baud rate for the RT4K Serial Connection
    while(!Serial){;}   // allow connection to establish before continuing
    Serial.print(F("\r")); // clear RT4K Serial buffer
    extronSerial.begin(9600); // set the baud rate for the Extron sw1 Connection
    extronSerial.setTimeout(150); // sets the timeout for reading / saving into a string
    extronSerial2.begin(9600); // set the baud rate for Extron sw2 Connection
    extronSerial2.setTimeout(150); // sets the timeout for reading / saving into a string for the Extron sw2 Connection

} // end of setup


void loop(){

// below are a list of functions that loop over and over to read in port changes and other misc tasks. you can disable them by commenting them out

irRec(); // intercepts the remote's button presses and relays them through the Serial interface giving a much more responsive experience

readGscart1();

readGscart2();

readExtron1(); // also reads TESmart HDMI and Otaku Games Scart switch on "alt sw1" port

readExtron2(); // also reads TESmart HDMI and Otaku Games Scart switch on "alt sw2" port

all_extron_inactive_ports_check();

} /////////////////////////////////// end of void loop ////////////////////////////////////

void readExtron1(){
    
    // listens to the Extron sw1 Port for changes
    // SIS Command Responses reference - Page 77 https://media.extron.com/public/download/files/userman/XP300_Matrix_B.pdf
    if(extronSerial.available() > 0){ // if there is data available for reading, read
    extronSerial.readBytes(ecapbytes,13); // read in and store only the first 13 bytes for every status message received from 1st Extron SW port
      if(debugECAPbytes){
        Serial.print(F("ecapbytes: "));
        for(int i=0;i<13;i++){
          Serial.print(ecapbytes[i],HEX);Serial.print(F(" "));
        }Serial.println(F("\r"));
      } // end of debugECAPbytes()
    }
    ecap = String((char *)ecapbytes); // convert bytes to String for Extron switches


    if(ecap.substring(0,3) == "Out"){ // store only the input and output states, some Extron devices report output first instead of input
      einput = ecap.substring(6,10);
      eoutput = ecap.substring(3,5);
    }
    else if(ecap.substring(0,1) == "F"){
      einput = ecap.substring(4,8);
      eoutput = "00";
    }
    else{                             // less complex switches only report input status, no output status
      einput = ecap.substring(0,4);
      eoutput = "00";
    }


    // for Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references voutMaxtrix
    if(einput.substring(0,2) == "In" && voutMatrix[eoutput.toInt()]){
      if(einput == "In1 " || einput == "In01"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x92,2);delay(30);} // RT5X profile 1 
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x0B,2); // RT4K profile 1

        if(SVS==0)Serial.println(F("remote prof1\r"));
        else sendSVS(1);
      }
      else if(einput == "In2 " || einput == "In02"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x93,2);delay(30);} // RT5X profile 2
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x07,2); // RT4K profile 2

        if(SVS==0)Serial.println(F("remote prof2\r"));
        else sendSVS(2);
      }
      else if(einput == "In3 " || einput == "In03"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xCC,2);delay(30);} // RT5X profile 3
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x03,2); // RT4K profile 3

        if(SVS==0)Serial.println(F("remote prof3\r"));
        else sendSVS(3);
      }
      else if(einput == "In4 " || einput == "In04"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8E,2);delay(30);} // RT5X profile 4
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x0A,2); // RT4K profile 4

        if(SVS==0)Serial.println(F("remote prof4\r"));
        else sendSVS(4);
      }
      else if(einput == "In5 " || einput == "In05"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8F,2);delay(30);} // RT5X profile 5
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x06,2); // RT4K profile 5

        if(SVS==0)Serial.println(F("remote prof5\r"));
        else sendSVS(5);
      }
      else if(einput == "In6 " || einput == "In06"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC8,2);delay(30);} // RT5X profile 6
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x02,2); // RT4K profile 6

        if(SVS==0)Serial.println(F("remote prof6\r"));
        else sendSVS(6);
      }
      else if(einput == "In7 " || einput == "In07"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8A,2);delay(30);} // RT5X profile 7
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x09,2); // RT4K profile 7

        if(SVS==0)Serial.println(F("remote prof7\r"));
        else sendSVS(7);
      }
      else if(einput == "In8 " || einput == "In08"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8B,2);delay(30);} // RT5X profile 8
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x05,2); // RT4K profile 8

        if(SVS==0)Serial.println(F("remote prof8\r"));
        else sendSVS(8);
      }
      else if(einput == "In9 " || einput == "In09"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC4,2);delay(30);} // RT5X profile 9
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x01,2); // RT4K profile 9

        if(SVS==0)Serial.println(F("remote prof9\r"));
        else sendSVS(9);
      }
      else if(einput == "In10"){
        if((RT5Xir == 1) && !DP0){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x25,2); // RT4K profile 10

        if(SVS==0)Serial.println(F("remote prof10\r"));
        else sendSVS(10);
      }
      else if(einput == "In11"){
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x26,2); // RT4K profile 11

        if(SVS==0)Serial.println(F("remote prof11\r"));
        else sendSVS(11);
      }
      else if(einput == "In12"){
        if((RT4Kir == 1) && !DP0)irsend.sendNEC(0x49,0x27,2); // RT4K profile 12

        if((SVS==0 && !DP0))Serial.println(F("remote prof12\r")); // okay to use this profile if DP0 is set to false
        else sendSVS(12);
      }
      else if(einput != "In0 " && einput != "In00" && einput2 != "In0 " && einput2 != "In00"){ // for inputs 13-99 (SVS only)
        sendSVS(einput.substring(2,4));
      }

      previnput = einput;

    }

    // for TESmart HDMI switch on alt sw1 Port
    if(ecapbytes[4] == 17){
      if(ecapbytes[6] == 22){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x92,2);delay(30);} // RT5X profile 1 
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x0B,2); // RT4K profile 1

        if(SVS==0)Serial.println(F("remote prof1\r"));
        else sendSVS(1);
      }
      else if(ecapbytes[6] == 23){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x93,2);delay(30);} // RT5X profile 2
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x07,2); // RT4K profile 2

        if(SVS==0)Serial.println(F("remote prof2\r"));
        else sendSVS(2);
      }
      else if(ecapbytes[6] == 24){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xCC,2);delay(30);} // RT5X profile 3
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x03,2); // RT4K profile 3

        if(SVS==0)Serial.println(F("remote prof3\r"));
        else sendSVS(3);
      }
      else if(ecapbytes[6] == 25){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8E,2);delay(30);} // RT5X profile 4
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x0A,2); // RT4K profile 4

        if(SVS==0)Serial.println(F("remote prof4\r"));
        else sendSVS(4);
      }
      else if(ecapbytes[6] == 26){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8F,2);delay(30);} // RT5X profile 5
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x06,2); // RT4K profile 5

        if(SVS==0)Serial.println(F("remote prof5\r"));
        else sendSVS(5);
      }
      else if(ecapbytes[6] == 27){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC8,2);delay(30);} // RT5X profile 6
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x02,2); // RT4K profile 6

        if(SVS==0)Serial.println(F("remote prof6\r"));
        else sendSVS(6);
      }
      else if(ecapbytes[6] == 28){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8A,2);delay(30);} // RT5X profile 7
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x09,2); // RT4K profile 7

        if(SVS==0)Serial.println(F("remote prof7\r"));
        else sendSVS(7);
      }
      else if(ecapbytes[6] == 29){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8B,2);delay(30);} // RT5X profile 8
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x05,2); // RT4K profile 8

        if(SVS==0)Serial.println(F("remote prof8\r"));
        else sendSVS(8);
      }
      else if(ecapbytes[6] == 30){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC4,2);delay(30);} // RT5X profile 9
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x01,2); // RT4K profile 9

        if(SVS==0)Serial.println(F("remote prof9\r"));
        else sendSVS(9);
      }
      else if(ecapbytes[6] == 31){
        if((RT5Xir == 1) && !DP0){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x25,2); // RT4K profile 10

        if(SVS==0)Serial.println(F("remote prof10\r"));
        else sendSVS(10);
      }
      else if(ecapbytes[6] == 32){
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x26,2); // RT4K profile 11

        if(SVS==0)Serial.println(F("remote prof11\r"));
        else sendSVS(11);
      }
      else if(ecapbytes[6] == 33){
        if((RT4Kir == 1) && !DP0)irsend.sendNEC(0x49,0x27,2); // RT4K profile 12

        if(SVS==0 && !DP0)Serial.println(F("remote prof12\r")); // okay to use this profile if DP0 is set to false
        else sendSVS(12);
      }
      else if(ecapbytes[6] > 33 && ecapbytes[6] < 38){
        sendSVS(ecapbytes[6] - 21);
      }
    }

    // set ecapbytes to 0 for next read
    memset(ecapbytes,0,sizeof(ecapbytes));

    // for Otaku Games Scart Switch
    if(ecap.substring(0,6) == "remote"){
      if(ecap == "remote prof10"){
          if((RT5Xir == 1) && !DP0){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x25,2); // RT4K profile 10

          if(SVS==0) Serial.println(F("remote prof10\r"));
          else sendSVS(10);
      }
      else if(ecap == "remote prof12"){
          delay(1); // do nothing
      }
      else if(ecap.substring(0,12) == "remote prof1"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x92,2);delay(30);} // RT5X profile 1 
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x0B,2); // RT4K profile 1

          if(SVS==0) Serial.println(F("remote prof1\r"));
          else sendSVS(1);
      }
      else if(ecap.substring(0,12) == "remote prof2"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x93,2);delay(30);} // RT5X profile 2
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x07,2); // RT4K profile 2

          if(SVS==0) Serial.println(F("remote prof2\r"));
          else sendSVS(2);
      }
      else if(ecap.substring(0,12) == "remote prof3"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0xCC,2);delay(30);} // RT5X profile 3
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x03,2); // RT4K profile 3

          if(SVS==0) Serial.println(F("remote prof3\r"));
          else sendSVS(3);
      }
      else if(ecap.substring(0,12) == "remote prof4"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8E,2);delay(30);} // RT5X profile 4
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x0A,2); // RT4K profile 4

          if(SVS==0) Serial.println(F("remote prof4\r"));
          else sendSVS(4);
      }
      else if(ecap.substring(0,12) == "remote prof5"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8F,2);delay(30);} // RT5X profile 5
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x06,2); // RT4K profile 5

          if(SVS==0) Serial.println(F("remote prof5\r"));
          else sendSVS(5);
      }
      else if(ecap.substring(0,12) == "remote prof6"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC8,2);delay(30);} // RT5X profile 6
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x02,2); // RT4K profile 6

          if(SVS==0) Serial.println(F("remote prof6\r"));
          else sendSVS(6);
      }
      else if(ecap.substring(0,12) == "remote prof7"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8A,2);delay(30);} // RT5X profile 7
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x09,2); // RT4K profile 7

          if(SVS==0) Serial.println(F("remote prof7\r"));
          else sendSVS(7);
      }
      else if(ecap.substring(0,12) == "remote prof8"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8B,2);delay(30);} // RT5X profile 8
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x05,2); // RT4K profile 8

          if(SVS==0) Serial.println(F("remote prof8\r"));
          else sendSVS(8);
      }
      else if(ecap.substring(0,12) == "remote prof9"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC4,2);delay(30);} // RT5X profile 9
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x01,2); // RT4K profile 9

          if(SVS==0) Serial.println(F("remote prof9\r"));
          else sendSVS(9);
      }

    }

} // end of readExtron1()


void readExtron2(){
    
    // listens to the Extron sw2 Port for changes
    if(extronSerial2.available() > 0){ // if there is data available for reading, read
    extronSerial2.readBytes(ecapbytes2,13); // read in and store only the first 13 bytes for every status message received from 2nd Extron port
    }
    ecap2 = String((char *)ecapbytes2);

    if(ecap2.substring(0,3) == "Out"){ // store only the input and output states, some Extron devices report output first instead of input
      einput2 = ecap2.substring(6,10);
      eoutput2 = ecap2.substring(3,5);
    }
    else if(ecap2.substring(0,1) == "F"){
      einput2 = ecap2.substring(4,8);
      eoutput2 = "00";
    }
    else{                              // less complex switches only report input status, no output status
      einput2 = ecap2.substring(0,4);
      eoutput2 = "00";
    }


    // For Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references voutMaxtrix
    if(einput2.substring(0,2) == "In" && voutMatrix[eoutput2.toInt()+32]){
      if(einput2 != "In0 " && einput2 != "In00"){ // much easier method for switch 2 since ALL inputs will respond with SVS commands regardless of SVS option above
        if(einput2.substring(3,4) == " ") 
          sendSVS(einput2.substring(2,3).toInt()+100);
        else 
          sendSVS(einput2.substring(2,4).toInt()+100);
      }

        previnput2 = einput2;
    }

    // for TESmart HDMI switch on Extron sw2 Port
    if(ecapbytes2[4] == 17){
      if(ecapbytes2[6] > 21 && ecapbytes2[6] < 38){
        sendSVS(ecapbytes2[6] + 79);
      }
    }

    // set ecapbytes2 to 0 for next read
    memset(ecapbytes2,0,sizeof(ecapbytes2));

    // for Otaku Games Scart Switch
    if(ecap2.substring(0,6) == "remote"){
      if(ecap2 == "remote prof10") sendSVS(110);
      else if(ecap2 == "remote prof12") delay(1); // do nothing
      else if(ecap2.substring(0,12) == "remote prof1") sendSVS(101);
      else if(ecap2.substring(0,12) == "remote prof2") sendSVS(102);
      else if(ecap2.substring(0,12) == "remote prof3") sendSVS(103);
      else if(ecap2.substring(0,12) == "remote prof4") sendSVS(104);
      else if(ecap2.substring(0,12) == "remote prof5") sendSVS(105);
      else if(ecap2.substring(0,12) == "remote prof6") sendSVS(106);
      else if(ecap2.substring(0,12) == "remote prof7") sendSVS(107);
      else if(ecap2.substring(0,12) == "remote prof8") sendSVS(108);
      else if(ecap2.substring(0,12) == "remote prof9") sendSVS(109);
    }

}// end of readExtron2()

void readGscart1(){ // readGscart1

// https://shmups.system11.org/viewtopic.php?p=1307320#p1307320
// gscartsw_lite EXT pinout:
// Pin 1: GND
// Pin 2: Override
// Pin 3: N/C
// Pin 4: +5V
// Pin 5: IN_BIT0
// Pin 6: IN_BIT1
// Pin 7: IN_BIT2
// Pin 8: N/C
//
// Reference for no active input state: https://shmups.system11.org/viewtopic.php?t=50851&start=4976
//
// When no input is active, a 50% off/on cycle can be seen for each pin as it cycles through the inputs. Pin5 the fastest, Pin7 the slowest. If we can monitor each
// pin then we can detect when there is no active input.
//
//        Pin7 Pin6 Pin5
// Port1  0     0     0
// Port2  0     0     1
// Port3  0     1     0
// Port4  0     1     1 
// Port5  1     0     0
// Port6  1     0     1
// Port7  1     1     0
// Port8  1     1     1

uint8_t fpdc = 0; // 1 = 50% duty cycle was detected / all ports are in-active
uint8_t bit[3] = {0,0,0};
float val[3] = {0,0,0};

for(uint8_t i = 0; i < 3; i++){ // read in analog pin voltages, read each pin 4x in a row to ensure an accurate read, combats if using too large of a pull-down resistor
  for(uint8_t j = 0; j < 4; j++){
    val[i] = analogRead(apin[i]);
  }
  if((val[i]/211) >= high){ // if voltage is greater than or equal to the voltage defined for a high, increase highcount by 1 for that analog pin
    highcount[i]++;
  }
}

if(samcc == samsize){               // when the "samsize" number of samples has been taken, if the voltage was "high" for more than "dch" # of the "samsize" samples, set the bit to 1
  for(uint8_t i = 0; i < 3; i++){
    if(highcount[i] > dch)          // if the number of "high" samples per "samsize" are greater than "dch" set bit to 1.  
      bit[i] = 1;
    else if(highcount[i] > dcl)     // if the number of "high" samples is greater than "dcl" and less than "dch" (50% high samples) the switch must be cycling inputs, therefor no active input
      fpdc = 1;
  }
}


if(((bit[2] != bitprev[2] || bit[1] != bitprev[1] || bit[0] != bitprev[0]) || (allgscartoff == 1)) && (samcc == samsize) && !(fpdc)){
  //Detect which scart port is now active and change profile accordingly
  if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 0)){ // 0 0 0
    if(RT5Xir == 2){irsend.sendNEC(0xB3,0x92,2);delay(30);} // RT5X profile 1 
    if(RT4Kir == 2)irsend.sendNEC(0x49,0x0B,2); // RT4K profile 1

    if(SVS==2)Serial.println(F("remote prof1\r"));
    else sendSVS(201);
  } 
  else if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 1)){ // 0 0 1
    if(RT5Xir == 2){irsend.sendNEC(0xB3,0x93,2);delay(30);} // RT5X profile 2
    if(RT4Kir == 2)irsend.sendNEC(0x49,0x07,2);  // RT4K profile 2

    if(SVS==2)Serial.println(F("remote prof2\r"));
    else sendSVS(202);
  }
  else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 0)){ // 0 1 0
    if(RT5Xir == 2){irsend.sendNEC(0xB3,0xCC,2);delay(30);} // RT5X profile 3
    if(RT4Kir == 2)irsend.sendNEC(0x49,0x03,2);  // RT4K profile 3

    if(SVS==2)Serial.println(F("remote prof3\r"));
    else sendSVS(203);
  }
  else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 1)){ // 0 1 1
    if(RT5Xir == 2){irsend.sendNEC(0xB3,0x8E,2);delay(30);} // RT5X profile 4
    if(RT4Kir == 2)irsend.sendNEC(0x49,0x0A,2);  // RT4K profile 4

    if(SVS==2)Serial.println(F("remote prof4\r"));
    else sendSVS(204);
  }
  else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 0)){ // 1 0 0
    if(RT5Xir == 2){irsend.sendNEC(0xB3,0x8F,2);delay(30);} // RT5X profile 5
    if(RT4Kir == 2)irsend.sendNEC(0x49,0x06,2);  // RT4K profile 5

    if(SVS==2)Serial.println(F("remote prof5\r"));
    else sendSVS(205);
  } 
  else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 1)){ // 1 0 1
    if(RT5Xir == 2){irsend.sendNEC(0xB3,0xC8,2);delay(30);} // RT5X profile 6
    if(RT4Kir == 2)irsend.sendNEC(0x49,0x02,2);  // RT4K profile 6

    if(SVS==2)Serial.println(F("remote prof6\r"));
    else sendSVS(206);
  }   
  else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 0)){ // 1 1 0
    if(RT5Xir == 2){irsend.sendNEC(0xB3,0x8A,2);delay(30);} // RT5X profile 7
    if(RT4Kir == 2)irsend.sendNEC(0x49,0x09,2);  // RT4K profile 7

    if(SVS==2)Serial.println(F("remote prof7\r"));
    else sendSVS(207);
  } 
  else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 1)){ // 1 1 1
    if(RT5Xir == 2){irsend.sendNEC(0xB3,0x8B,2);delay(30);} // RT5X profile 8
    if(RT4Kir == 2)irsend.sendNEC(0x49,0x05,2);  // RT4K profile 8

    if(SVS==2)Serial.println(F("remote prof8\r"));
    else sendSVS(208);
  }
  
  if(allgscartoff) allgscartoff = 0;
  bitprev[0] = bit[0];
  bitprev[1] = bit[1];
  bitprev[2] = bit[2];
  fpdcprev = fpdc;

}

if((fpdccount == (fpdccountmax - 1)) && (fpdc != fpdcprev) && (samcc == samsize)){ // if no active port has been detected for fpdccountmax periods
  
  allgscartoff = 1;
  memset(bitprev,0,sizeof(bitprev));
  fpdcprev = fpdc;

  if(DP0 && allgscartoff2 && ((previnput == "0" || previnput == "discon" || previnput == "In0 " || previnput == "In00") && // cross-checks Extron status
                            (previnput2 == "0" || previnput2 == "discon" || previnput2 == "In0 " || previnput2 == "In00"))){
    if(SVS==1)sendSVS(0);
    else if(SVS==2)Serial.println(F("remote prof12\r"));
  }

}

if(fpdc && (samcc == samsize)){ // if no active port has been detected, loop counter until active port
  if(fpdccount == (fpdccountmax - 1))
    fpdccount = 0;
  else 
    fpdccount++;
}
else if(samcc == samsize){
  fpdccount = 0;
}

if(debugGscart1){ // debug
delay(200);
// Serial.print(F("A0 voltage:         "));Serial.print(val[0]/211);Serial.print(F("v    SC: "));Serial.print(samcc);Serial.print(F("  fpdccount: "));Serial.print(fpdccount);
// Serial.print(F(" fpdc: "));Serial.print(fpdc);Serial.print(F(" fpdcprev: "));Serial.print(fpdcprev);
// Serial.print(F(" /-/ bit0: "));Serial.print(bit[0]);Serial.print(F(" bitprev[0]: "));Serial.print(bitprev[0]);Serial.print(F(" highcount0: "));Serial.print(highcount[0]);
// Serial.print(F(" allgoff: "));Serial.print(allgscartoff);Serial.print(F(" allgoff2: "));Serial.println(allgscartoff2);
Serial.print(F("A0 voltage:         "));Serial.println(val[0]/211);
Serial.print(F("A1 voltage:         "));Serial.println(val[1]/211);
Serial.print(F("A2 voltage:         "));Serial.println(val[2]/211);
}

if(samcc < samsize) // increment counter until "samsize" has been reached then reset counter and "highcount"
  samcc++;
else{
  samcc = 1;
  memset(highcount,0,sizeof(highcount));
}

} // end readGscart1()

void readGscart2(){

// https://shmups.system11.org/viewtopic.php?p=1307320#p1307320
// gscartsw_lite EXT pinout:
// Pin 1: GND
// Pin 2: Override
// Pin 3: N/C
// Pin 4: +5V
// Pin 5: IN_BIT0
// Pin 6: IN_BIT1
// Pin 7: IN_BIT2
// Pin 8: N/C
//
// Reference for no active input state: https://shmups.system11.org/viewtopic.php?t=50851&start=4976
//
// When no input is active, a 50% off/on cycle can be seen for each pin as it cycles through the inputs. Pin5 the fastest, Pin7 the slowest. If we can monitor each
// pin then we can detect when there is no active input.
//
//        Pin7 Pin6 Pin5
// Port1  0     0     0
// Port2  0     0     1
// Port3  0     1     0
// Port4  0     1     1 
// Port5  1     0     0
// Port6  1     0     1
// Port7  1     1     0
// Port8  1     1     1


uint8_t fpdc = 0;
uint8_t bit[3] = {0,0,0};
float val[3] = {0,0,0};

for(uint8_t i = 0; i < 3; i++){
  for(uint8_t j = 0; j < 4; j++){
    val[i] = analogRead(apin2[i]);
  }
  if((val[i]/211) >= high2){
    highcount2[i]++;
  }
}

if(samcc2 == samsize){              // when the "samsize" number of samples has been taken, if the voltage was high for more than dch of the samples, set the bit to 1
  for(uint8_t i = 0; i < 3; i++){   // if the voltage was high for only dcl to dch samples, set an all in-active ports flag (fpdc = 1)
    if(highcount2[i] > dch)         // how many "high" samples per "samsize" are required for a bit to be 1.  
      bit[i] = 1;
    else if(highcount2[i] > dcl)
      fpdc = 1;
  }
}


if(((bit[2] != bitprev2[2] || bit[1] != bitprev2[1] || bit[0] != bitprev2[0]) || (allgscartoff2 == 1)) && (samcc2 == samsize) && !(fpdc)){
      //Detect which scart port is now active and change profile accordingly
      if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 0)){ // 0 0 0
        if(RT5Xir == 2){irsend.sendNEC(0xB3,0xC4,2);delay(30);} // RT5X profile 9
        if(RT4Kir == 2)irsend.sendNEC(0x49,0x01,2);  // RT4K profile 9

        if(SVS==2)Serial.println(F("remote prof9\r"));
        else sendSVS(209);
      } 
      else if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 1)){ // 0 0 1
        if(RT5Xir == 2){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
        if(RT4Kir == 2)irsend.sendNEC(0x49,0x25,2);  // RT4K profile 10

        if(SVS==2)Serial.println(F("remote prof10\r"));
        else sendSVS(210);
      }
      else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 0)){ // 0 1 0
        if(RT4Kir == 2)irsend.sendNEC(0x49,0x26,2);  // RT4K profile 11

        if(SVS==2)Serial.println(F("remote prof11\r"));
        else sendSVS(211);
      }
      else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 1)){ // 0 1 1
        if(RT4Kir == 2)irsend.sendNEC(0x49,0x27,2); // RT4K profile 12

        if(SVS==2)Serial.println(F("remote prof12\r"));
        else sendSVS(212);
      }
      else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 0))sendSVS(213); // 1 0 0
      else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 1))sendSVS(214); // 1 0 1
      else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 0))sendSVS(215); // 1 1 0
      else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 1))sendSVS(216); // 1 1 1

      if(allgscartoff2) allgscartoff2 = 0;
      bitprev2[0] = bit[0];
      bitprev2[1] = bit[1];
      bitprev2[2] = bit[2];
      fpdcprev2 = fpdc;

}

if((fpdccount2 == (fpdccountmax - 1)) && (fpdc != fpdcprev2) && (samcc2 == samsize)){ // if all in-active ports flag has been detected for "fpdccountmax" periods 
  
  allgscartoff2 = 1;
  memset(bitprev2,0,sizeof(bitprev2));
  fpdcprev2 = fpdc;
  
  if(DP0 && allgscartoff && ((previnput == "0" || previnput == "discon" || previnput == "In0 " || previnput == "In00") &&  // cross-checks Extron status
                            (previnput2 == "0" || previnput2 == "discon" || previnput2 == "In0 " || previnput2 == "In00"))){
    if(SVS==1)sendSVS(0);
    else if(SVS==2)Serial.println(F("remote prof12\r"));
  }

}

if(fpdc && (samcc2 == samsize)){
  if(fpdccount2 == (fpdccountmax - 1)) 
    fpdccount2 = 0;
  else 
    fpdccount2++;
}
else if(samcc2 == samsize){
  fpdccount2 = 0;
}

if(debugGscart2){  
delay(200);
// Serial.print(F("A3 voltage:         "));Serial.print(val[0]/211);Serial.print(F("v    SC: "));Serial.print(samcc2);Serial.print(F("  fpdccount2: "));Serial.print(fpdccount2);
// Serial.print(F(" fpdc: "));Serial.print(fpdc);Serial.print(F(" fpdcprev2: "));Serial.print(fpdcprev2);
// Serial.print(F(" /-/ bit0: "));Serial.print(bit[0]);Serial.print(F(" bitprev2[0]: "));Serial.print(bitprev2[0]);Serial.print(" highcount0: ");Serial.print(highcount2[0]);
// Serial.print(F(" allgoff: "));Serial.print(allgscartoff);Serial.print(F(" allgoff2: "));Serial.println(allgscartoff2);
Serial.print(F("A3 voltage:         "));Serial.println(val[0]/211);
Serial.print(F("A4 voltage:         "));Serial.println(val[1]/211);
Serial.print(F("A5 voltage:         "));Serial.println(val[2]/211);
}


if(samcc2 < samsize) 
  samcc2++;
else{
  samcc2 = 1;
  memset(highcount2,0,sizeof(highcount2));
}

} // end readGscart2()


void all_extron_inactive_ports_check(){

    // when both Extron switches match In0 or In00 (no active ports), both gscart/gcomp are disconnected or all ports in-active, a default profile can be loaded if DP0 is enabled
    if(((previnput == "In0 " || previnput == "In00") && (previnput2 == "In0 " || previnput2 == "In00" || previnput2 == "discon")) && DP0 
         && allgscartoff && allgscartoff2 && voutMatrix[eoutput.toInt()] && (previnput2 == "discon" || voutMatrix[eoutput2.toInt()+32])){
      if(RT5Xir == 1){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
      if(RT4Kir == 1)irsend.sendNEC(0x49,0x27,2); // RT4K profile 12


      if(SVS==0)Serial.println(F("remote prof12\r"));
      else if(SVS==1)sendSVS(0);

      previnput = "0";
      if(previnput2 != "discon")previnput2 = "0";
      
    }

    if(previnput == "0" && previnput2.substring(0,2) == "In")previnput = "In00";  // changes previnput "0" state to "In00" when there is a newly active input on the other switch
    if(previnput2 == "0" && previnput.substring(0,2) == "In")previnput2 = "In00";

} // end of all_inactive_ports_check

void irRec(){

  uint8_t ir_recv_command = 0;
  uint8_t ir_recv_address = 0;

  if(TinyReceiverDecode()){

    ir_recv_command = TinyIRReceiverData.Command;
    ir_recv_address = TinyIRReceiverData.Address;
        
    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && extrabuttonprof == 2){
      if(ir_recv_command == 11){ // profile button 1
        svsbutton += 1;
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        svsbutton += 2;
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
        svsbutton += 3;
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        svsbutton += 4;
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        svsbutton += 5;
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        svsbutton += 6;
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        svsbutton += 7;
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        svsbutton += 8;
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 1){ // profile button 9
        svsbutton += 9;
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 37 || ir_recv_command == 38 || ir_recv_command == 39){ // profile buttons 10,11,12
        svsbutton += 0;
        nument++;
        ir_recv_command = 0;
      }
      else{
        extrabuttonprof = 0;
        svsbutton = "";
        nument = 0;
      }

      
    } // end of extrabutton == 2

    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && extrabuttonprof == 1){ // if AUX8 was pressed and a profile button is pressed next,
      if(ir_recv_command == 11){ // profile button 1                                                         // load "SVS" profiles 1 - 12 (profile button 1 - 12).
        sendSVS(auxprof[0]);                                                                               // Can be changed to "ANY" SVS profile in the OPTIONS section
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        sendSVS(auxprof[1]);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
        sendSVS(auxprof[2]);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        sendSVS(auxprof[3]);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        sendSVS(auxprof[4]);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        sendSVS(auxprof[5]);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        sendSVS(auxprof[6]);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        sendSVS(auxprof[7]);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 1){ // profile button 9
        sendSVS(auxprof[8]);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 37){ // profile button 10
        sendSVS(auxprof[9]);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 38){ // profile button 11
        sendSVS(auxprof[10]);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 39){ // profile button 12
        sendSVS(auxprof[11]);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 63){
        ir_recv_command = 0;
        extrabuttonprof++;

      }
      else{
        extrabuttonprof = 0;
        svsbutton = "";
        nument = 0;
      }
      
    } // end extrabuttonprof == 1


    if(nument == 3){
      sendSVS(svsbutton);
      nument = 0;
      svsbutton = "";
      extrabuttonprof = 0;
      ir_recv_command = 0;
    }
    
    if(TinyIRReceiverData.Flags == IRDATA_FLAGS_IS_REPEAT){repeatcount++;} // directional buttons have to be held down for just a bit before repeating

    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT){ // block most buttons from being repeated when held
      repeatcount = 0;
      if(ir_recv_command == 63){
        //Serial.println(F("remote aux8\r"));
        extrabuttonprof++;
      }
      else if(ir_recv_command == 62){
        Serial.println(F("remote aux7\r"));
      }
      else if(ir_recv_command == 61){
        Serial.println(F("remote aux6\r"));
      }
      else if(ir_recv_command == 60){
        Serial.println(F("remote aux5\r"));
      }
      else if(ir_recv_command == 59){
        Serial.println(F("remote aux4\r"));
      }
      else if(ir_recv_command == 58){
        Serial.println(F("remote aux3\r"));
      }
      else if(ir_recv_command == 57){
        Serial.println(F("remote aux2\r"));
      }
      else if(ir_recv_command == 56){
        Serial.println(F("remote aux1\r"));
      }
      else if(ir_recv_command == 52){
        Serial.println(F("remote res1\r"));
      }
      else if(ir_recv_command == 53){
        Serial.println(F("remote res2\r"));
      }
      else if(ir_recv_command == 54){
        Serial.println(F("remote res3\r"));
      }
      else if(ir_recv_command == 55){
        Serial.println(F("remote res4\r"));
      }
      else if(ir_recv_command == 51){
        Serial.println(F("remote res480p\r"));
      }
      else if(ir_recv_command == 50){
        Serial.println(F("remote res1440p\r"));
      }
      else if(ir_recv_command == 49){
        Serial.println(F("remote res1080p\r"));
      }
      else if(ir_recv_command == 48){
        Serial.println(F("remote res4k\r"));
      }
      else if(ir_recv_command == 47){
        Serial.println(F("remote buffer\r"));
      }
      else if(ir_recv_command == 44){
        Serial.println(F("remote genlock\r"));
      }
      else if(ir_recv_command == 46){
        Serial.println(F("remote safe\r"));
      }
      else if(ir_recv_command == 86){
        Serial.println(F("remote pause\r"));
      }
      else if(ir_recv_command == 45){
        Serial.println(F("remote phase\r"));
      }
      else if(ir_recv_command == 43){
        Serial.println(F("remote gain\r"));
      }
      else if(ir_recv_command == 36){
        Serial.println(F("remote prof\r"));
      }
      else if(ir_recv_command == 11){
        Serial.println(F("remote prof1\r"));
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x92,1);delay(30);irsend.sendNEC(0xB3,0x92,1);} // RT5X profile 1 
      }
      else if(ir_recv_command == 7){
        Serial.println(F("remote prof2\r"));
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x93,1);delay(30);irsend.sendNEC(0xB3,0x93,1);} // RT5X profile 2
      }
      else if(ir_recv_command == 3){
        Serial.println(F("remote prof3\r"));
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0xCC,1);delay(30);irsend.sendNEC(0xB3,0xCC,1);} // RT5X profile 3
      }
      else if(ir_recv_command == 10){
        Serial.println(F("remote prof4\r"));
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x8E,1);delay(30);irsend.sendNEC(0xB3,0x8E,1);} // RT5X profile 4
      }
      else if(ir_recv_command == 6){
        Serial.println(F("remote prof5\r"));
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x8F,1);delay(30);irsend.sendNEC(0xB3,0x8F,1);} // RT5X profile 5
      }
      else if(ir_recv_command == 2){
        Serial.println(F("remote prof6\r"));
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0xC8,1);delay(30);irsend.sendNEC(0xB3,0xC8,1);} // RT5X profile 6
      }
      else if(ir_recv_command == 9){
        Serial.println(F("remote prof7\r"));
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x8A,1);delay(30);irsend.sendNEC(0xB3,0x8A,1);} // RT5X profile 7
      }
      else if(ir_recv_command == 5){
        Serial.println(F("remote prof8\r"));
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x8B,1);delay(30);irsend.sendNEC(0xB3,0x8B,1);} // RT5X profile 8
      }
      else if(ir_recv_command == 1){
        Serial.println(F("remote prof9\r"));
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0xC4,1);delay(30);irsend.sendNEC(0xB3,0xC4,1);} // RT5X profile 9
      }
      else if(ir_recv_command == 37){
        Serial.println(F("remote prof10\r"));
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x87,1);delay(30);irsend.sendNEC(0xB3,0x87,1);} // RT5X profile 10
      }
      else if(ir_recv_command == 38){
        Serial.println(F("remote prof11\r"));
      }
      else if(ir_recv_command == 39){
        Serial.println(F("remote prof12\r"));
      }
      else if(ir_recv_command == 35){
        Serial.println(F("remote adc\r"));
      }
      else if(ir_recv_command == 34){
        Serial.println(F("remote sfx\r"));
      }
      else if(ir_recv_command == 33){
        Serial.println(F("remote scaler\r"));
      }
      else if(ir_recv_command == 32){
        Serial.println(F("remote output\r"));
      }
      else if(ir_recv_command == 17){
        Serial.println(F("remote input\r"));
      }
      else if(ir_recv_command == 41){
        Serial.println(F("remote stat\r"));
      }
      else if(ir_recv_command == 40){
        Serial.println(F("remote diag\r"));
      }
      else if(ir_recv_command == 66){
        Serial.println(F("remote back\r"));
      }
      else if(ir_recv_command == 83){
        Serial.println(F("remote ok\r"));
      }
      else if(ir_recv_command == 79){
        Serial.println(F("remote right\r"));
      }
      else if(ir_recv_command == 16){
        Serial.println(F("remote down\r"));
      }
      else if(ir_recv_command == 87){
        Serial.println(F("remote left\r"));
      }
      else if(ir_recv_command == 24){
        Serial.println(F("remote up\r"));
      }
      else if(ir_recv_command == 92){
        Serial.println(F("remote menu\r"));
      }
      else if(ir_recv_command == 26){
        if(pwrtoggle){
          Serial.println(F("pwr on\r"));
          pwrtoggle = 0;
        }
        else{
          Serial.println(F("remote pwr\r"));
          pwrtoggle = 1;
        }
      }
    }
    else if(ir_recv_address == 73 && repeatcount > 4){ // directional buttons have to be held down for just a bit before repeating
      if(ir_recv_command == 24){
        Serial.println(F("remote up\r"));
      }
      else if(ir_recv_command == 16){
        Serial.println(F("remote down\r"));
      }
      else if(ir_recv_command == 87){
        Serial.println(F("remote left\r"));
      }
      else if(ir_recv_command == 79){
        Serial.println(F("remote right\r"));
      }
    } // end of if(ir_recv_address
    
    if(ir_recv_address == 73 && repeatcount > 15){ // when directional buttons are held down for even longer... turbo directional mode
      if(ir_recv_command == 87){
        for(int i=0;i<4;i++){
          Serial.println(F("remote left\r"));
        }
      }
      else if(ir_recv_command == 79){
        for(int i=0;i<4;i++){
          Serial.println(F("remote right\r"));
        }
      }
    } // end of turbo directional mode
    
  } // end of TinyReceiverDecode()   
} // end of irRec()

void sendSVS(int num){
  Serial.print(F("SVS NEW INPUT="));
  Serial.print(num);
  Serial.println(F("\r"));
  delay(1000);
  Serial.print(F("SVS CURRENT INPUT="));
  Serial.print(num);
  Serial.println(F("\r"));
}

void sendSVS(String num){
  Serial.print(F("SVS NEW INPUT="));
  Serial.print(num);
  Serial.println(F("\r"));
  delay(1000);
  Serial.print(F("SVS CURRENT INPUT="));
  Serial.print(num);
  Serial.println(F("\r"));
}
