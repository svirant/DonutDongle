/*
* RT4K Donut Dongle v0.6
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


int offset = 0; // Only needed for multiple Donut Dongles (DD). Set offset so 2nd,3rd,etc boards don't overlap SVS profiles. (e.g. offset = 300;) 
                // MUST use SVS=1 on additional DDs. If using the IR receiver, recommended to have it only connected to the DD with offset = 0.


uint8_t SVS = 1; //     "Remote" profiles are profiles that are assigned to buttons 1-12 on the RT4K remote. "SVS" profiles reside under the "/profile/SVS/" directory 
             //     on the SD card.  This option allows you to choose which ones to call when a console is powered on.  Remote profiles allow you to easily change 
             //     the profile being used for a console's switch input if your setup is in flux. SVS require you to rename the file itself on the SD card which is 
             //     a little more work.  Regardless, SVS profiles will need to be used for console switch inputs over 12.
             //
             // **** Make sure "Auto Load SVS" is "On" under the RT4K Profiles menu. A requirement for most options ****
             //
             // 0 - use "Remote" profiles 1-12 for up to 12 inputs on 1st Extron Switch and SVS 13 - 99 for everything over 12. Only SVS profiles are used on 2nd Extron Switch if connected.
             //
             //     - remote profiles 1-12 for 1st Extron or TESmart Switch (If S0 below is set to true - remote profile 12 is used when all ports are in-active)
             //     - SVS  12 - 99  for 1st Extron or TESmart (S0 is true)
             //     - SVS  13 - 99  for 1st Extron or TESmart (S0 is false)
             //     - SVS 101 - 199 for 2nd Extron or TESmart
             //     - SVS 201 - 208 for 1st gScart
             //     - SVS 209 - 216 for 2nd gScart
             //
             // 1 - use only "SVS" profiles.
             //     Make sure "Auto Load SVS" is "On" under the RT4K Profiles menu
             //     RT4K checks the /profile/SVS subfolder for profiles and need to be named: "S<input number>_<user defined>.rt4"
             //     For example, SVS input 2 would look for a profile that is named S2_SNES.rt4
             //     If there’s more than one profile that fits the pattern, the first match is used
             //
             //     - SVS   1 -  99 for 1st Extron or TESmart
             //     - SVS 101 - 199 for 2nd Extron or TESmart
             //     - SVS 201 - 208 for 1st gScart
             //     - SVS 209 - 216 for 2nd gScart
             //     - SVS   0       for S0 option mentioned below
             //
             //  ** If S0 below is set to true, create "/profile/SVS/S0_<user defined>.rt4" for when all ports are in-active. Ex: S0_HDMI.rt4
             //
             // 2 - use "Remote" profiles 1-12 for gscart/gcomp switches. Remote profile 1-8 for 1st gscart switch, 9-12 for inputs 1-4 on 2nd gscart switch.
             //     inputs 5-8 on the 2nd gscart switch will use SVS profiles 213 - 216
             //
             //     - remote profiles 1-8 for 1st gScart, 9 - 12 for first 4 inputs on 2nd gScart (If S0 below is set to true - remote profile 12 is used when all ports are in-active)
             //     - SVS 213 -  216 for remaining inputs 5 - 8 on 2nd gScart 
             //     - SVS   1 -  99 for 1st Extron or TESmart
             //     - SVS 101 - 199 for 2nd Extron or TESmart
             //
             //
             //



bool S0  = false;        // (Profile 0) 
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
                       


////////////////////////// 
                        // Choosing the above two options can get quite confusing (even for myself) so maybe this will help a little more:
                        //
                        // when S0=0 and SVS=0, button profiles 1 - 12 are used for EXTRON sw1, and SVS for EVERYTHING else
                        // when S0=0 and SVS=1, SVS profiles are used for everything
                        // when S0=0 and SVS=2, button profiles 1 - 8 are used for GSCART sw1, 9 - 12 for GSCART sw2 (ports 1 - 4), and SVS for EVERYTHING else
                        // when S0=1 and SVS=0, button profiles 1 - 11 are used for EXTRON sw1 and 12 as "Profile 0", and SVS for everything else 
                        // when S0=1 and SVS=1, SVS profiles for everything, and uses S0_<user defined>.rt4 as "Profile 0" 
                        // when S0=1 and SVS=2, button profiles 1 - 8 are used for GSCART sw1, 9 - 11 for GSCART sw2 (ports 1 -3) and 12 as "Profile 0", and SVS for EVERYTHING else
                        //
//////////////////////////



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
                     //     (S0 - if enabled uses Profile 10 on the RT5X)
                     //
                     // 2 = enabled for gscart switch only (remote profiles 1-8 for first gscart, 9-10 for first 2 inputs on second gscart)

uint8_t RT4Kir = 0;      // 0 = disables IR Emitter for RetroTink 4K
                     // 1 = enabled for Extron sw1 switch, TESmart HDMI, or Otaku Games Scart Switch if connected
                     //     sends Profile 1 - 12 commands to RetroTink 4K. Must have IR LED emitter connected.
                     //     (S0 - if enabled uses Profile 12 on the RT4K)
                     //
                     // 2 = enabled for gscart switch only (remote profiles 1-8 for first gscart, 9-12 for first 4 inputs on second gscart)

uint8_t MTVIKIir = 0;    // Must have IR "Receiver" connected to the Donut Dongle for option 1 & 2.
                     // 0 = disables IR Receiver -> Serial Control for MT-VIKI 8 Port HDMI switch
                     //
                     // 1 = MT-VIKI 8 Port HDMI switch connected to "Extron sw1"
                     //     Using the RT4K Remote w/ the IR Receiver, AUX8 + profile button changes the MT-VIKI Input over Serial.
                     //     Sends auxprof SVS profiles listed below.
                     //
                     // 2 = MT-VIKI 8 Port HDMI switch connected to "Extron sw2"
                     //     Using the RT4K Remote w/ the IR Receiver, AUX8 + profile button changes the MT-VIKI Input over Serial.
                     //     Sends auxprof SVS profiles listed below. You can change them below to 101 - 108 to prevent SVS profile conflicts if needed.

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
uint8_t fpdcprev[2] = {0,0}; // stores 50% duty cycle detection
uint8_t fpdccount[2] = {0,0}; // number of times a 50% duty cycle period has been detected
uint8_t allgscartoff[2] = {2,2}; // 0 = at least 1 port is active, 1 = no ports are active, 2 = disconnected or not used yet
uint8_t otakuoff[2] = {2,2}; // 0 = at least 1 port is active, 1 = no ports are active, 2 = disconnected or not used yet
uint8_t samcc[2] = {0,0}; // ADC sample cycle counter
uint8_t highcount[2][3] = {{0,0,0},{0,0,0}}; // number of high samples recorded for bit0, bit1, bit2
uint8_t bitprev[2][3] = {{0,0,0},{0,0,0}}; // stores previous bit state
byte const apin[2][3] = {{A0,A1,A2},{A3,A4,A5}}; // defines analog pins used to read bit0, bit1, bit2
uint8_t gctl = 0; // disable gscart/gcomp override by default until all boards have voltage divider
uint8_t auxgsw[2] = {0,0}; // gscart sw1,sw2 toggle override

// gscart / gcomp adjustment Global variables for port detection
float highsamvolt[2] = {1.2,1.2}; // for gscart sw1,sw2 rise above this voltage for a high sample
uint8_t dch = 15; // (duty cycle high) at least this many high samples per "samsize" for a high bit (~75% duty cycle)
uint8_t dcl = 5; // (duty cycle low) at least this many high samples and less than "dch" per "samsize" indicate all inputs are in-active (~50% duty cycle)
uint8_t samsize = 20; // total number of ADC samples required to capture at least 1 period
uint8_t fpdccountmax = 3; // number of periods required when in the 50% duty cycle state before a Profile 0 is triggered.

// Extron sw1 / alt sw1 software serial port -> MAX3232 TTL IC
#define rxPin 3 // sets Extron sw1 Rx pin to D3 on Arduino
#define txPin 4 // sets Extron sw1 Tx pin to D4 ...
SoftwareSerial extronSerial = SoftwareSerial(rxPin,txPin); // setup a software serial port for listening to Extron sw1 / alt sw1

// Extron sw2 / al2 sw1 software serial port -> MAX3232 TTL IC
AltSoftSerial extronSerial2; // setup yet another serial port for listening to Extron sw2 / alt sw2. hardcoded to pins D8 / D9

// Extron Global variables
byte ecapbytes[2][13]; // used to store first 13 captured bytes / messages for Extron                
String ecap[2]; // used to store Extron status messages for Extron in String format
String einput[2]; // used to store first 4 chars of Extron input
String previnput[2] = {"discon","discon"}; // used to keep track of previous input
String eoutput[2]; // used to store first 2 chars of Extron output

// IR Global variables
uint8_t pwrtoggle = 0; // used to toggle remote power button command (on/off) when using the optional IR Receiver
uint8_t repeatcount = 0; // used to help emulate the repeat nature of directional button presses
uint8_t extrabuttonprof = 0; // used to keep track of AUX8 button presses for addtional button profiles
String svsbutton; // used to store 3 digit SVS profile when AUX8 is double pressed
uint8_t nument = 0; // used to keep track of how many digits have been entered for 3 digit SVS profile
IRsend irsend;
byte viki1[4] = {0xA5,0x5A,0x00,0xCC};
byte viki2[4] = {0xA5,0x5A,0x01,0xCC};
byte viki3[4] = {0xA5,0x5A,0x02,0xCC};
byte viki4[4] = {0xA5,0x5A,0x03,0xCC};
byte viki5[4] = {0xA5,0x5A,0x04,0xCC};
byte viki6[4] = {0xA5,0x5A,0x05,0xCC};
byte viki7[4] = {0xA5,0x5A,0x06,0xCC};
byte viki8[4] = {0xA5,0x5A,0x07,0xCC};

////////////////////////////////////////////////////////////////////////

void setup(){

    pinMode(12,OUTPUT);
    digitalWrite(12,LOW); // gscart sw1 override set low for query mode by default
    pinMode(10,OUTPUT);
    digitalWrite(10,LOW); // gscart sw2 override set low for query mode by default
    PORTC &= B11111000;  // disable A0-A2 pullup resistors, uses external pulldown resistors
    PORTC &= B11000111;  // disable A3-A5 pullup resistors, ...
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

} /////////////////////////////////// end of void loop ////////////////////////////////////

void readExtron1(){

    // listens to the Extron sw1 Port for changes
    // SIS Command Responses reference - Page 77 https://media.extron.com/public/download/files/userman/XP300_Matrix_B.pdf
    if(extronSerial.available() > 0){ // if there is data available for reading, read
    extronSerial.readBytes(ecapbytes[0],13); // read in and store only the first 13 bytes for every status message received from 1st Extron SW port
    }
    ecap[0] = String((char *)ecapbytes[0]); // convert bytes to String for Extron switches


    if(ecap[0].substring(0,3) == "Out"){ // store only the input and output states, some Extron devices report output first instead of input
      einput[0] = ecap[0].substring(6,10);
      eoutput[0] = ecap[0].substring(3,5);
    }
    else if(ecap[0].substring(0,1) == "F"){ // detect if switch has changed auto/manual states
      einput[0] = ecap[0].substring(4,8);
      eoutput[0] = "00";
    }
    else if(ecap[0].substring(0,3) == "Rpr"){ // detect if a Preset has been used
      einput[0] = ecap[0].substring(0,5);
      eoutput[0] = "00";
    }
    else{                             // less complex switches only report input status, no output status
      einput[0] = ecap[0].substring(0,4);
      eoutput[0] = "00";
    }


    // for Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references voutMaxtrix
    if((einput[0].substring(0,2) == "In" && voutMatrix[eoutput[0].toInt()]) || (einput[0].substring(0,3) == "Rpr")){
      if(einput[0] == "In1 " || einput[0] == "In01" || einput[0] == "Rpr01"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x92,2);delay(30);} // RT5X profile 1 
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x0B,2); // RT4K profile 1

        if(SVS==0)sendRBP(1);
        else sendSVS(1);
      }
      else if(einput[0] == "In2 " || einput[0] == "In02" || einput[0] == "Rpr02"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x93,2);delay(30);} // RT5X profile 2
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x07,2); // RT4K profile 2

        if(SVS==0)sendRBP(2);
        else sendSVS(2);
      }
      else if(einput[0] == "In3 " || einput[0] == "In03" || einput[0] == "Rpr03"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xCC,2);delay(30);} // RT5X profile 3
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x03,2); // RT4K profile 3

        if(SVS==0)sendRBP(3);
        else sendSVS(3);
      }
      else if(einput[0] == "In4 " || einput[0] == "In04" || einput[0] == "Rpr04"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8E,2);delay(30);} // RT5X profile 4
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x0A,2); // RT4K profile 4

        if(SVS==0)sendRBP(4);
        else sendSVS(4);
      }
      else if(einput[0] == "In5 " || einput[0] == "In05" || einput[0] == "Rpr05"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8F,2);delay(30);} // RT5X profile 5
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x06,2); // RT4K profile 5

        if(SVS==0)sendRBP(5);
        else sendSVS(5);
      }
      else if(einput[0] == "In6 " || einput[0] == "In06" || einput[0] == "Rpr06"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC8,2);delay(30);} // RT5X profile 6
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x02,2); // RT4K profile 6

        if(SVS==0)sendRBP(6);
        else sendSVS(6);
      }
      else if(einput[0] == "In7 " || einput[0] == "In07" || einput[0] == "Rpr07"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8A,2);delay(30);} // RT5X profile 7
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x09,2); // RT4K profile 7

        if(SVS==0)sendRBP(7);
        else sendSVS(7);
      }
      else if(einput[0] == "In8 " || einput[0] == "In08" || einput[0] == "Rpr08"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8B,2);delay(30);} // RT5X profile 8
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x05,2); // RT4K profile 8

        if(SVS==0)sendRBP(8);
        else sendSVS(8);
      }
      else if(einput[0] == "In9 " || einput[0] == "In09" || einput[0] == "Rpr09"){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC4,2);delay(30);} // RT5X profile 9
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x01,2); // RT4K profile 9

        if(SVS==0)sendRBP(9);
        else sendSVS(9);
      }
      else if(einput[0] == "In10" || einput[0] == "Rpr10"){
        if((RT5Xir == 1) && !S0){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x25,2); // RT4K profile 10

        if(SVS==0)sendRBP(10);
        else sendSVS(10);
      }
      else if(einput[0] == "In11" || einput[0] == "Rpr11"){
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x26,2); // RT4K profile 11

        if(SVS==0)sendRBP(11);
        else sendSVS(11);
      }
      else if(einput[0] == "In12" || einput[0] == "Rpr12"){
        if((RT4Kir == 1) && !S0)irsend.sendNEC(0x49,0x27,2); // RT4K profile 12

        if((SVS==0 && !S0))sendRBP(12); // okay to use this profile if S0 is set to false
        else sendSVS(12);
      }
      else if(einput[0].substring(0,3) == "Rpr"){
        sendSVS(einput[0].substring(3,5));
      }
      else if(einput[0] != "In0 " && einput[0] != "In00"){ // for inputs 13-99 (SVS only)
        sendSVS(einput[0].substring(2,4));
      }

      previnput[0] = einput[0];

      // Exton S0
      // when both Extron switches match In0 or In00 (no active ports), both gscart/gcomp/otaku are disconnected or all ports in-active, a S0 Profile can be loaded if S0 is enabled
      if(((einput[0] == "In0 " || einput[0] == "In00") && (previnput[1] == "In0 " || previnput[1] == "In00" || previnput[1] == "discon")) && S0 
        && otakuoff[0] && otakuoff[1] && allgscartoff[0] && allgscartoff[1] && voutMatrix[eoutput[0].toInt()] && (previnput[1] == "discon" || voutMatrix[eoutput[1].toInt()+32])){

      if(RT5Xir == 1){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
      if(RT4Kir == 1)irsend.sendNEC(0x49,0x27,2); // RT4K profile 12

      if(SVS==0)sendRBP(12);
      else if(SVS==1)sendSVS(0);

      previnput[0] = "0";
      if(previnput[1] != "discon")previnput[1] = "0";
      
      } // end of Extron S0

      if(previnput[0] == "0" && previnput[1].substring(0,2) == "In")previnput[0] = "In00";  // changes previnput[0] "0" state to "In00" when there is a newly active input on the other switch
      if(previnput[1] == "0" && previnput[0].substring(0,2) == "In")previnput[1] = "In00";

    }

    // for TESmart / MT-VIKI HDMI switch on Extron sw1 / alt sw1
    if(ecapbytes[0][4] == 17 || ecapbytes[0][4] == 95){
      if(ecapbytes[0][6] == 22 || ecapbytes[0][11] == 48){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x92,2);delay(30);} // RT5X profile 1 
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x0B,2); // RT4K profile 1

        if(SVS==0)sendRBP(1);
        else sendSVS(1);
      }
      else if(ecapbytes[0][6] == 23 || ecapbytes[0][11] == 49){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x93,2);delay(30);} // RT5X profile 2
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x07,2); // RT4K profile 2

        if(SVS==0)sendRBP(2);
        else sendSVS(2);
      }
      else if(ecapbytes[0][6] == 24 || ecapbytes[0][11] == 50){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xCC,2);delay(30);} // RT5X profile 3
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x03,2); // RT4K profile 3

        if(SVS==0)sendRBP(3);
        else sendSVS(3);
      }
      else if(ecapbytes[0][6] == 25 || ecapbytes[0][11] == 51){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8E,2);delay(30);} // RT5X profile 4
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x0A,2); // RT4K profile 4

        if(SVS==0)sendRBP(4);
        else sendSVS(4);
      }
      else if(ecapbytes[0][6] == 26 || ecapbytes[0][11] == 52){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8F,2);delay(30);} // RT5X profile 5
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x06,2); // RT4K profile 5

        if(SVS==0)sendRBP(5);
        else sendSVS(5);
      }
      else if(ecapbytes[0][6] == 27 || ecapbytes[0][11] == 53){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC8,2);delay(30);} // RT5X profile 6
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x02,2); // RT4K profile 6

        if(SVS==0)sendRBP(6);
        else sendSVS(6);
      }
      else if(ecapbytes[0][6] == 28 || ecapbytes[0][11] == 54){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8A,2);delay(30);} // RT5X profile 7
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x09,2); // RT4K profile 7

        if(SVS==0)sendRBP(7);
        else sendSVS(7);
      }
      else if(ecapbytes[0][6] == 29 || ecapbytes[0][11] == 55){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8B,2);delay(30);} // RT5X profile 8
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x05,2); // RT4K profile 8

        if(SVS==0)sendRBP(8);
        else sendSVS(8);
      }
      else if(ecapbytes[0][6] == 30){
        if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC4,2);delay(30);} // RT5X profile 9
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x01,2); // RT4K profile 9

        if(SVS==0)sendRBP(9);
        else sendSVS(9);
      }
      else if(ecapbytes[0][6] == 31){
        if((RT5Xir == 1) && !S0){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x25,2); // RT4K profile 10

        if(SVS==0)sendRBP(10);
        else sendSVS(10);
      }
      else if(ecapbytes[0][6] == 32){
        if(RT4Kir == 1)irsend.sendNEC(0x49,0x26,2); // RT4K profile 11

        if(SVS==0)sendRBP(11);
        else sendSVS(11);
      }
      else if(ecapbytes[0][6] == 33){
        if((RT4Kir == 1) && !S0)irsend.sendNEC(0x49,0x27,2); // RT4K profile 12

        if(SVS==0 && !S0)sendRBP(12); // okay to use this profile if S0 is set to false
        else sendSVS(12);
      }
      else if(ecapbytes[0][6] > 33 && ecapbytes[0][6] < 38){
        sendSVS(ecapbytes[0][6] - 21);
      }
    }

    // set ecapbytes[0] to 0 for next read
    memset(ecapbytes[0],0,sizeof(ecapbytes[0]));

    // for Otaku Games Scart Switch
    if(ecap[0].substring(0,6) == "remote"){
      otakuoff[0] = 0;
      if(ecap[0].substring(0,13) == "remote prof10"){
          if((RT5Xir == 1) && !S0){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x25,2); // RT4K profile 10

          if(SVS==0) sendRBP(10);
          else sendSVS(10);
      }
      else if(ecap[0].substring(0,13) == "remote prof12"){
        otakuoff[0] = 1;
        if(S0 && otakuoff[1] && allgscartoff[0] && allgscartoff[1] && ((previnput[0] == "0" || previnput[0] == "discon" || previnput[0] == "In0 " || previnput[0] == "In00") && // cross-checks gscart, otaku2, Extron status
                            (previnput[1] == "0" || previnput[1] == "discon" || previnput[1] == "In0 " || previnput[1] == "In00"))){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x27,2); // RT4K profile 12
          if(SVS == 0){
            sendRBP(12);
          }
          else if(SVS == 1){
            sendSVS(0);
          }
        }
      }
      else if(ecap[0].substring(0,12) == "remote prof1"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x92,2);delay(30);} // RT5X profile 1 
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x0B,2); // RT4K profile 1

          if(SVS==0) sendRBP(1);
          else sendSVS(1);
      }
      else if(ecap[0].substring(0,12) == "remote prof2"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x93,2);delay(30);} // RT5X profile 2
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x07,2); // RT4K profile 2

          if(SVS==0) sendRBP(2);
          else sendSVS(2);
      }
      else if(ecap[0].substring(0,12) == "remote prof3"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0xCC,2);delay(30);} // RT5X profile 3
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x03,2); // RT4K profile 3

          if(SVS==0) sendRBP(3);
          else sendSVS(3);
      }
      else if(ecap[0].substring(0,12) == "remote prof4"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8E,2);delay(30);} // RT5X profile 4
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x0A,2); // RT4K profile 4

          if(SVS==0) sendRBP(4);
          else sendSVS(4);
      }
      else if(ecap[0].substring(0,12) == "remote prof5"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8F,2);delay(30);} // RT5X profile 5
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x06,2); // RT4K profile 5

          if(SVS==0) sendRBP(5);
          else sendSVS(5);
      }
      else if(ecap[0].substring(0,12) == "remote prof6"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC8,2);delay(30);} // RT5X profile 6
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x02,2); // RT4K profile 6

          if(SVS==0) sendRBP(6);
          else sendSVS(6);
      }
      else if(ecap[0].substring(0,12) == "remote prof7"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8A,2);delay(30);} // RT5X profile 7
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x09,2); // RT4K profile 7

          if(SVS==0) sendRBP(7);
          else sendSVS(7);
      }
      else if(ecap[0].substring(0,12) == "remote prof8"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x8B,2);delay(30);} // RT5X profile 8
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x05,2); // RT4K profile 8

          if(SVS==0) sendRBP(8);
          else sendSVS(8);
      }
      else if(ecap[0].substring(0,12) == "remote prof9"){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0xC4,2);delay(30);} // RT5X profile 9
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x01,2); // RT4K profile 9

          if(SVS==0) sendRBP(9);
          else sendSVS(9);
      }
    }

} // end of readExtron1()

void readExtron2(){
    
    // listens to the Extron sw2 Port for changes
    if(extronSerial2.available() > 0){ // if there is data available for reading, read
    extronSerial2.readBytes(ecapbytes[1],13); // read in and store only the first 13 bytes for every status message received from 2nd Extron port
    }
    ecap[1] = String((char *)ecapbytes[1]);

    if(ecap[1].substring(0,3) == "Out"){ // store only the input and output states, some Extron devices report output first instead of input
      einput[1] = ecap[1].substring(6,10);
      eoutput[1] = ecap[1].substring(3,5);
    }
    else if(ecap[1].substring(0,1) == "F"){ // detect if switch has changed auto/manual states
      einput[1] = ecap[1].substring(4,8);
      eoutput[1] = "00";
    }
    else if(ecap[1].substring(0,3) == "Rpr"){ // detect if a Preset has been used
      einput[1] = ecap[1].substring(0,5);
      eoutput[1] = "00";
    }
    else{                              // less complex switches only report input status, no output status
      einput[1] = ecap[1].substring(0,4);
      eoutput[1] = "00";
    }


    // For Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references voutMaxtrix
    if((einput[1].substring(0,2) == "In" && voutMatrix[eoutput[1].toInt()+32]) || (einput[1].substring(0,3) == "Rpr")){
      if(einput[1].substring(0,3) == "Rpr"){
        sendSVS(einput[1].substring(3,5).toInt()+100);
      }
      else if(einput[1] != "In0 " && einput[1] != "In00"){ // much easier method for switch 2 since ALL inputs will respond with SVS commands regardless of SVS option above
        if(einput[1].substring(3,4) == " ") 
          sendSVS(einput[1].substring(2,3).toInt()+100);
        else 
          sendSVS(einput[1].substring(2,4).toInt()+100);
      }

      previnput[1] = einput[1];
      
      // Extron2 S0
      // when both Extron switches match In0 or In00 (no active ports), both gscart/gcomp/otaku are disconnected or all ports in-active, a Profile 0 can be loaded if S0 is enabled
      if(((einput[1] == "In0 " || einput[1] == "In00") && (previnput[0] == "In0 " || previnput[0] == "In00" || previnput[0] == "discon")) && S0 
        && otakuoff[0] && otakuoff[1] && allgscartoff[0] && allgscartoff[1] && voutMatrix[eoutput[1].toInt()+32] && (previnput[0] == "discon" || voutMatrix[eoutput[0].toInt()])){

      if(RT5Xir == 1){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
      if(RT4Kir == 1)irsend.sendNEC(0x49,0x27,2); // RT4K profile 12

      if(SVS==0)sendRBP(12);
      else if(SVS==1)sendSVS(0);

      previnput[1] = "0";
      if(previnput[0] != "discon")previnput[0] = "0";
      
      } // end of Extron2 S0

      if(previnput[0] == "0" && previnput[1].substring(0,2) == "In")previnput[0] = "In00";  // changes previnput[0] "0" state to "In00" when there is a newly active input on the other switch
      if(previnput[1] == "0" && previnput[0].substring(0,2) == "In")previnput[1] = "In00";

    }

    // for TESmart / MT-VIKI HDMI switch on Extron sw2 Port
    if(ecapbytes[1][4] == 17 && ecapbytes[1][6] > 21 && ecapbytes[1][6] < 38){ // TESmart
      sendSVS(ecapbytes[1][6] + 79);
    }
    else if(ecapbytes[1][4] == 95 && ecapbytes[1][11] > 47 && ecapbytes[1][11] < 56){ // MT-VIKI 
      sendSVS(ecapbytes[1][11] + 53);
    }

    // set ecapbytes[1] to 0 for next read
    memset(ecapbytes[1],0,sizeof(ecapbytes[1]));

    // for Otaku Games Scart Switch
    if(ecap[1].substring(0,6) == "remote"){
      otakuoff[1] = 0;
      if(ecap[1].substring(0,13) == "remote prof10") sendSVS(110);
      else if(ecap[1].substring(0,13) == "remote prof12"){
        otakuoff[1] = 1;
        if(S0 && otakuoff[0] && allgscartoff[0] && allgscartoff[1] && ((previnput[0] == "0" || previnput[0] == "discon" || previnput[0] == "In0 " || previnput[0] == "In00") && // cross-checks gscart, otaku, Extron status
                            (previnput[1] == "0" || previnput[1] == "discon" || previnput[1] == "In0 " || previnput[1] == "In00"))){
          if(RT5Xir == 1){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
          if(RT4Kir == 1)irsend.sendNEC(0x49,0x27,2); // RT4K profile 12
          if(SVS == 0){
            sendRBP(12);
          }
          else if(SVS == 1){
            sendSVS(0);
          }
        }
      }
      else if(ecap[1].substring(0,12) == "remote prof1") sendSVS(101);
      else if(ecap[1].substring(0,12) == "remote prof2") sendSVS(102);
      else if(ecap[1].substring(0,12) == "remote prof3") sendSVS(103);
      else if(ecap[1].substring(0,12) == "remote prof4") sendSVS(104);
      else if(ecap[1].substring(0,12) == "remote prof5") sendSVS(105);
      else if(ecap[1].substring(0,12) == "remote prof6") sendSVS(106);
      else if(ecap[1].substring(0,12) == "remote prof7") sendSVS(107);
      else if(ecap[1].substring(0,12) == "remote prof8") sendSVS(108);
      else if(ecap[1].substring(0,12) == "remote prof9") sendSVS(109);
    }

}// end of readExtron2()

void readGscart1(){

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
      val[i] = analogRead(apin[0][i]);
    }
    if((val[i]/211) >= highsamvolt[0]){ // if voltage is greater than or equal to the voltage defined for a high, increase highcount[0] by 1 for that analog pin
      highcount[0][i]++;
    }
  }

  if(samcc[0] == samsize){               // when the "samsize" number of samples has been taken, if the voltage was "high" for more than "dch" # of the "samsize" samples, set the bit to 1
    for(uint8_t i = 0; i < 3; i++){
      if(highcount[0][i] > dch)          // if the number of "high" samples per "samsize" are greater than "dch" set bit to 1.  
        bit[i] = 1;
      else if(highcount[0][i] > dcl)     // if the number of "high" samples is greater than "dcl" and less than "dch" (50% high samples) the switch must be cycling inputs, therefor no active input
        fpdc = 1;
    }
  }


  if(((bit[2] != bitprev[0][2] || bit[1] != bitprev[0][1] || bit[0] != bitprev[0][0]) || (allgscartoff[0] == 1)) && (samcc[0] == samsize) && !(fpdc)){
    //Detect which scart port is now active and change profile accordingly
    if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 0)){ // 0 0 0
      if(RT5Xir == 2){irsend.sendNEC(0xB3,0x92,2);delay(30);} // RT5X profile 1 
      if(RT4Kir == 2)irsend.sendNEC(0x49,0x0B,2); // RT4K profile 1

      if(SVS==2)sendRBP(1);
      else sendSVS(201);
    } 
    else if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 1)){ // 0 0 1
      if(RT5Xir == 2){irsend.sendNEC(0xB3,0x93,2);delay(30);} // RT5X profile 2
      if(RT4Kir == 2)irsend.sendNEC(0x49,0x07,2);  // RT4K profile 2

      if(SVS==2)sendRBP(2);
      else sendSVS(202);
    }
    else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 0)){ // 0 1 0
      if(RT5Xir == 2){irsend.sendNEC(0xB3,0xCC,2);delay(30);} // RT5X profile 3
      if(RT4Kir == 2)irsend.sendNEC(0x49,0x03,2);  // RT4K profile 3

      if(SVS==2)sendRBP(3);
      else sendSVS(203);
    }
    else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 1)){ // 0 1 1
      if(RT5Xir == 2){irsend.sendNEC(0xB3,0x8E,2);delay(30);} // RT5X profile 4
      if(RT4Kir == 2)irsend.sendNEC(0x49,0x0A,2);  // RT4K profile 4

      if(SVS==2)sendRBP(4);
      else sendSVS(204);
    }
    else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 0)){ // 1 0 0
      if(RT5Xir == 2){irsend.sendNEC(0xB3,0x8F,2);delay(30);} // RT5X profile 5
      if(RT4Kir == 2)irsend.sendNEC(0x49,0x06,2);  // RT4K profile 5

      if(SVS==2)sendRBP(5);
      else sendSVS(205);
    } 
    else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 1)){ // 1 0 1
      if(RT5Xir == 2){irsend.sendNEC(0xB3,0xC8,2);delay(30);} // RT5X profile 6
      if(RT4Kir == 2)irsend.sendNEC(0x49,0x02,2);  // RT4K profile 6

      if(SVS==2)sendRBP(6);
      else sendSVS(206);
    }   
    else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 0)){ // 1 1 0
      if(RT5Xir == 2){irsend.sendNEC(0xB3,0x8A,2);delay(30);} // RT5X profile 7
      if(RT4Kir == 2)irsend.sendNEC(0x49,0x09,2);  // RT4K profile 7

      if(SVS==2)sendRBP(7);
      else sendSVS(207);
    } 
    else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 1)){ // 1 1 1
      if(RT5Xir == 2){irsend.sendNEC(0xB3,0x8B,2);delay(30);} // RT5X profile 8
      if(RT4Kir == 2)irsend.sendNEC(0x49,0x05,2);  // RT4K profile 8

      if(SVS==2)sendRBP(8);
      else sendSVS(208);
    }
    
    if(allgscartoff[0]) allgscartoff[0] = 0;
    bitprev[0][0] = bit[0];
    bitprev[0][1] = bit[1];
    bitprev[0][2] = bit[2];
    fpdcprev[0] = fpdc;

  }

  if((fpdccount[0] == (fpdccountmax - 1)) && (fpdc != fpdcprev[0]) && (samcc[0] == samsize)){ // if no active port has been detected for fpdccountmax periods
    
    allgscartoff[0] = 1;
    memset(bitprev[0],0,sizeof(bitprev[0]));
    fpdcprev[0] = fpdc;

    if(S0 && otakuoff[0] && otakuoff[1] && allgscartoff[1] && ((previnput[0] == "0" || previnput[0] == "discon" || previnput[0] == "In0 " || previnput[0] == "In00") && // cross-checks otaku, gscart2, Extron status
                              (previnput[1] == "0" || previnput[1] == "discon" || previnput[1] == "In0 " || previnput[1] == "In00"))){
      if(SVS==1)sendSVS(0);
      else if(SVS==2)sendRBP(12);
    }

  }

  if(fpdc && (samcc[0] == samsize)){ // if no active port has been detected, loop counter until active port
    if(fpdccount[0] == (fpdccountmax - 1))
      fpdccount[0] = 0;
    else 
      fpdccount[0]++;
  }
  else if(samcc[0] == samsize){
    fpdccount[0] = 0;
  }


  if(samcc[0] < samsize) // increment counter until "samsize" has been reached then reset counter and "highcount[0]"
    samcc[0]++;
  else{
    samcc[0] = 1;
    memset(highcount[0],0,sizeof(highcount[0]));
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
      val[i] = analogRead(apin[1][i]);
    }
    if((val[i]/211) >= highsamvolt[1]){
      highcount[1][i]++;
    }
  }

  if(samcc[1] == samsize){              // when the "samsize" number of samples has been taken, if the voltage was high for more than dch of the samples, set the bit to 1
    for(uint8_t i = 0; i < 3; i++){   // if the voltage was high for only dcl to dch samples, set an all in-active ports flag (fpdc = 1)
      if(highcount[1][i] > dch)         // how many "high" samples per "samsize" are required for a bit to be 1.  
        bit[i] = 1;
      else if(highcount[1][i] > dcl)
        fpdc = 1;
    }
  }


  if(((bit[2] != bitprev[1][2] || bit[1] != bitprev[1][1] || bit[0] != bitprev[1][0]) || (allgscartoff[1] == 1)) && (samcc[1] == samsize) && !(fpdc)){
        //Detect which scart port is now active and change profile accordingly
        if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 0)){ // 0 0 0
          if(RT5Xir == 2){irsend.sendNEC(0xB3,0xC4,2);delay(30);} // RT5X profile 9
          if(RT4Kir == 2)irsend.sendNEC(0x49,0x01,2);  // RT4K profile 9

          if(SVS==2)sendRBP(9);
          else sendSVS(209);
        } 
        else if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 1)){ // 0 0 1
          if(RT5Xir == 2){irsend.sendNEC(0xB3,0x87,2);delay(30);} // RT5X profile 10
          if(RT4Kir == 2)irsend.sendNEC(0x49,0x25,2);  // RT4K profile 10

          if(SVS==2)sendRBP(10);
          else sendSVS(210);
        }
        else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 0)){ // 0 1 0
          if(RT4Kir == 2)irsend.sendNEC(0x49,0x26,2);  // RT4K profile 11

          if(SVS==2)sendRBP(11);
          else sendSVS(211);
        }
        else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 1)){ // 0 1 1
          if(RT4Kir == 2)irsend.sendNEC(0x49,0x27,2); // RT4K profile 12

          if(SVS==2)sendRBP(12);
          else sendSVS(212);
        }
        else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 0))sendSVS(213); // 1 0 0
        else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 1))sendSVS(214); // 1 0 1
        else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 0))sendSVS(215); // 1 1 0
        else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 1))sendSVS(216); // 1 1 1

        if(allgscartoff[1]) allgscartoff[1] = 0;
        bitprev[1][0] = bit[0];
        bitprev[1][1] = bit[1];
        bitprev[1][2] = bit[2];
        fpdcprev[1] = fpdc;

  }

  if((fpdccount[1] == (fpdccountmax - 1)) && (fpdc != fpdcprev[1]) && (samcc[1] == samsize)){ // if all in-active ports flag has been detected for "fpdccountmax" periods 
    
    allgscartoff[1] = 1;
    memset(bitprev[1],0,sizeof(bitprev[1]));
    fpdcprev[1] = fpdc;
    
    if(S0 && allgscartoff && otakuoff[0] && otakuoff[1] && ((previnput[0] == "0" || previnput[0] == "discon" || previnput[0] == "In0 " || previnput[0] == "In00") &&  // cross-checks gscart, otaku, Extron status
                              (previnput[1] == "0" || previnput[1] == "discon" || previnput[1] == "In0 " || previnput[1] == "In00"))){
      if(SVS==1)sendSVS(0);
      else if(SVS==2)sendRBP(12);
    }

  }

  if(fpdc && (samcc[1] == samsize)){
    if(fpdccount[1] == (fpdccountmax - 1)) 
      fpdccount[1] = 0;
    else 
      fpdccount[1]++;
  }
  else if(samcc[1] == samsize){
    fpdccount[1] = 0;
  }


  if(samcc[1] < samsize) 
    samcc[1]++;
  else{
    samcc[1] = 1;
    memset(highcount[1],0,sizeof(highcount[1]));
  }

} // end readGscart2()

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
        if(MTVIKIir == 0)sendSVS(auxprof[0]);                                                               // Can be changed to "ANY" SVS profile in the OPTIONS section
        if(MTVIKIir == 1){extronSerial.write(viki1,4);sendSVS(auxprof[0]);}
        if(MTVIKIir == 2){extronSerial2.write(viki1,4);sendSVS(auxprof[0]);}                                                                            
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        if(MTVIKIir == 0)sendSVS(auxprof[1]);
        if(MTVIKIir == 1){extronSerial.write(viki2,4);sendSVS(auxprof[1]);}
        if(MTVIKIir == 2){extronSerial2.write(viki2,4);sendSVS(auxprof[1]);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
      if(MTVIKIir == 0)sendSVS(auxprof[2]);
        if(MTVIKIir == 1){extronSerial.write(viki3,4);sendSVS(auxprof[2]);}
        if(MTVIKIir == 2){extronSerial2.write(viki3,4);sendSVS(auxprof[2]);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        if(MTVIKIir == 0)sendSVS(auxprof[3]);
        if(MTVIKIir == 1){extronSerial.write(viki4,4);sendSVS(auxprof[3]);}
        if(MTVIKIir == 2){extronSerial2.write(viki4,4);sendSVS(auxprof[3]);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        if(MTVIKIir == 0)sendSVS(auxprof[4]);
        if(MTVIKIir == 1){extronSerial.write(viki5,4);sendSVS(auxprof[4]);}
        if(MTVIKIir == 2){extronSerial2.write(viki5,4);sendSVS(auxprof[4]);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        if(MTVIKIir == 0)sendSVS(auxprof[5]);
        if(MTVIKIir == 1){extronSerial.write(viki6,4);sendSVS(auxprof[5]);}
        if(MTVIKIir == 2){extronSerial2.write(viki6,4);sendSVS(auxprof[5]);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        if(MTVIKIir == 0)sendSVS(auxprof[6]);
        if(MTVIKIir == 1){extronSerial.write(viki7,4);sendSVS(auxprof[6]);}
        if(MTVIKIir == 2){extronSerial2.write(viki7,4);sendSVS(auxprof[6]);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        if(MTVIKIir == 0)sendSVS(auxprof[7]);
        if(MTVIKIir == 1){extronSerial.write(viki8,4);sendSVS(auxprof[7]);}
        if(MTVIKIir == 2){extronSerial2.write(viki8,4);sendSVS(auxprof[7]);}
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

    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && gctl && (auxgsw[0] || auxgsw[1])){ // if AUX5 or AUX6 was pressed and a profile button is pressed next,
      if(ir_recv_command == 11){ // profile button 1
        if(auxgsw[0]){overrideGscart(1);auxgsw[0] = 0;}
        else if(auxgsw[1]){overrideGscart(9);auxgsw[1] = 0;}
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        if(auxgsw[0]){overrideGscart(2);auxgsw[0] = 0;}
        else if(auxgsw[1]){overrideGscart(10);auxgsw[1] = 0;}
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
        if(auxgsw[0]){overrideGscart(3);auxgsw[0] = 0;}
        else if(auxgsw[1]){overrideGscart(11);auxgsw[1] = 0;}
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        if(auxgsw[0]){overrideGscart(4);auxgsw[0] = 0;}
        else if(auxgsw[1]){overrideGscart(12);auxgsw[1] = 0;}
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        if(auxgsw[0]){overrideGscart(5);auxgsw[0] = 0;}
        else if(auxgsw[1]){overrideGscart(13);auxgsw[1] = 0;}
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        if(auxgsw[0]){overrideGscart(6);auxgsw[0] = 0;}
        else if(auxgsw[1]){overrideGscart(14);auxgsw[1] = 0;}
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        if(auxgsw[0]){overrideGscart(7);auxgsw[0] = 0;}
        else if(auxgsw[1]){overrideGscart(15);auxgsw[1] = 0;}
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        if(auxgsw[0]){overrideGscart(8);auxgsw[0] = 0;}
        else if(auxgsw[1]){overrideGscart(16);auxgsw[1] = 0;}
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 1 || ir_recv_command == 37 || ir_recv_command == 38 || ir_recv_command == 39){ // profile button 9,10,11,12 will turn off manual mode, back to auto mode
        if(auxgsw[0]){
          PORTB &= B11101111;  // D12 (PB4) LOW / enables auto-switching
          DDRC &= B11111000; // set A0-A2 as inputs
          PORTC &= B11111000;  // disable A0-A2 pullup resistors, uses external pulldown resistors
          auxgsw[0] = 0;
        }
        else if(auxgsw[1]){
          PORTB &= B11111011;  // D10 (PB2) LOW / enables auto-switching
          DDRC &= B11000111;  // set A3-A5 as inputs
          PORTC &= B11000111;  // disable A3-A5 pullup resistors, uses external pulldown resistors
          auxgsw[1] = 0;
        }
        ir_recv_command = 0;
      }
      else{
        auxgsw[0] = 0;
        auxgsw[1] = 0;
      }
      
    } // end auxgsw[0] = 1, auxgsw[1] = 1

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
        //Serial.println(F("remote aux8\r")); aux8
        extrabuttonprof++;
      }
      else if(ir_recv_command == 62){
        Serial.println(F("remote aux7\r"));
      }
      else if(ir_recv_command == 61){
        //Serial.println(F("remote aux6\r")); // aux6
        auxgsw[1] = 1;
      }
      else if(ir_recv_command == 60){
        //Serial.println(F("remote aux5\r")); // aux5
        auxgsw[0] = 1;
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
        sendRBP(1);
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x92,1);delay(30);irsend.sendNEC(0xB3,0x92,1);} // RT5X profile 1 
      }
      else if(ir_recv_command == 7){
        sendRBP(2);
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x93,1);delay(30);irsend.sendNEC(0xB3,0x93,1);} // RT5X profile 2
      }
      else if(ir_recv_command == 3){
        sendRBP(3);
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0xCC,1);delay(30);irsend.sendNEC(0xB3,0xCC,1);} // RT5X profile 3
      }
      else if(ir_recv_command == 10){
        sendRBP(4);
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x8E,1);delay(30);irsend.sendNEC(0xB3,0x8E,1);} // RT5X profile 4
      }
      else if(ir_recv_command == 6){
        sendRBP(5);
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x8F,1);delay(30);irsend.sendNEC(0xB3,0x8F,1);} // RT5X profile 5
      }
      else if(ir_recv_command == 2){
        sendRBP(6);
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0xC8,1);delay(30);irsend.sendNEC(0xB3,0xC8,1);} // RT5X profile 6
      }
      else if(ir_recv_command == 9){
        sendRBP(7);
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x8A,1);delay(30);irsend.sendNEC(0xB3,0x8A,1);} // RT5X profile 7
      }
      else if(ir_recv_command == 5){
        sendRBP(8);
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x8B,1);delay(30);irsend.sendNEC(0xB3,0x8B,1);} // RT5X profile 8
      }
      else if(ir_recv_command == 1){
        sendRBP(9);
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0xC4,1);delay(30);irsend.sendNEC(0xB3,0xC4,1);} // RT5X profile 9
      }
      else if(ir_recv_command == 37){
        sendRBP(10);
        if(RT5Xir >= 1){irsend.sendNEC(0xB3,0x87,1);delay(30);irsend.sendNEC(0xB3,0x87,1);} // RT5X profile 10
      }
      else if(ir_recv_command == 38){
        sendRBP(11);
      }
      else if(ir_recv_command == 39){
        sendRBP(12);
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
  Serial.print(num + offset);
  Serial.println(F("\r"));
  delay(1000);
  Serial.print(F("SVS CURRENT INPUT="));
  Serial.print(num + offset);
  Serial.println(F("\r"));
}

void sendSVS(String num){
  Serial.print(F("SVS NEW INPUT="));
  Serial.print(num.toInt() + offset);
  Serial.println(F("\r"));
  delay(1000);
  Serial.print(F("SVS CURRENT INPUT="));
  Serial.print(num.toInt() + offset);
  Serial.println(F("\r"));
}

void sendRBP(int prof){ // send Remote Button Profile
  Serial.print(F("remote prof"));
  Serial.print(prof);
  Serial.println(F("\r"));
}

void overrideGscart(uint8_t port){ // disable auto switching and allows gscart port select
  if(port <= 8){
    DDRC |= B00000111; // only set A0-A2 as outputs
    digitalWrite(12,HIGH); // D12 / gscart sw1 override set HIGH to select port (disables auto switching)
    if(port == 1){
      PORTC &= B11111000; // set low bits with 0
    }
    else if(port == 2){
      PORTC &= B11111000; // set low bits with 0
      PORTC |= B00000001; // set high bits with 1
    }
    else if(port == 3){
      PORTC &= B11111000; // set low bits with 0
      PORTC |= B00000010; // set high bits with 1
    }
    else if(port == 4){
      PORTC &= B11111000; // set low bits with 0
      PORTC |= B00000011; // set high bits with 1
    }
    else if(port == 5){
      PORTC &= B11111000; // set low bits with 0
      PORTC |= B00000100; // set high bits with 1
    }
    else if(port == 6){
      PORTC &= B11111000; // set low bits with 0
      PORTC |= B00000101; // set high bits with 1
    }
    else if(port == 7){
      PORTC &= B11111000; // set low bits with 0
      PORTC |= B00000110; // set high bits with 1
    }
    else if(port == 8){
      PORTC &= B11111000; // set low bits with 0
      PORTC |= B00000111; // set high bits with 1
    }
  }
  else if(port >= 9){
    DDRC |= B00111000; // only set A3-A5 as outputs
    digitalWrite(10,HIGH); // D10 / gscart sw2 override set HIGH to select port (disables auto switching)
    if(port == 9){
      PORTC &= B11000111; // set low bits with 0
    }
    else if(port == 10){
      PORTC &= B11000111; // set low bits with 0
      PORTC |= B00001000; // set high bits with 1
    }
    else if(port == 11){
      PORTC &= B11000111; // set low bits with 0
      PORTC |= B00010000; // set high bits with 1
    }
    else if(port == 12){
      PORTC &= B11000111; // set low bits with 0
      PORTC |= B00011000; // set high bits with 1
    }
    else if(port == 13){
      PORTC &= B11000111; // set low bits with 0
      PORTC |= B00100000; // set high bits with 1
    }
    else if(port == 14){
      PORTC &= B11000111; // set low bits with 0
      PORTC |= B00101000; // set high bits with 1
    }
    else if(port == 15){
      PORTC &= B11000111; // set low bits with 0
      PORTC |= B00110000; // set high bits with 1
    }
    else if(port == 16){
      PORTC &= B11000111; // set low bits with 0
      PORTC |= B00111000; // set high bits with 1
    }
  }
} // end of overrideGscart()
