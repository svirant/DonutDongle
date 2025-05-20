/*
* RT4K Donut Dongle v1.2
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

#define IR_SEND_PIN 11  // Optional IR LED Emitter for RT5X compatibility. Sends IR data out Arduino pin D11
#define IR_RECEIVE_PIN 2 // Optional IR Receiver on pin D2

#include <TinyIRReceiver.hpp>
#include <IRremote.hpp>       // found in the built-in Library Manager
#include <SoftwareSerial.h>
#include <AltSoftSerial.h>  // https://github.com/PaulStoffregen/AltSoftSerial in order to have a 3rd Serial port for 2nd Extron Switch / alt sw2
                            // Step 1 - Goto the github link above. Click the GREEN "<> Code" box and "Download ZIP"
                            // Step 2 - In Arudino IDE; goto "Sketch" -> "Include Library" -> "Add .ZIP Library"
/*
////////////////////
//    OPTIONS    //
//////////////////
*/

uint8_t debugE1CAP = 0; // line ~372
uint8_t debugE2CAP = 0; // line ~761

uint16_t const offset = 0; // Only needed for multiple Donut Dongles (DD). Set offset so 2nd,3rd,etc boards don't overlap SVS profiles. (e.g. offset = 300;) 
                      // MUST use SVS=1 on additional DDs. If using the IR receiver, recommended to have it only connected to the DD with lowest offset.


uint8_t const SVS = 1; //     "Remote" profiles are profiles that are assigned to buttons 1-12 on the RT4K remote. "SVS" profiles reside under the "/profile/SVS/" directory 
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



bool const S0  = false;        // (Profile 0) 
                         //
                         //  ** Recommended to remove any /profile/SVS/S0_<user defined>.rt4 profiles and leave this option "false" if using in tandem with the Scalable Video Switch. **
                         //
                         // set "true" to load "Remote Profile 12" instead of "S0_<user definted>.rt4" (if SVS=0) when all ports are in-active on 1st Extron switch (and 2nd if connected). 
                         // You can assign it to a generic HDMI profile for example.
                         // If your device has a 12th input, SVS will be used instead. "If" you also have an active 2nd Extron Switch, Remote Profile 12
                         // will only load if "BOTH" switches have all in-active ports.
                         // 
                         // 
                         // If SVS=1, /profile/SVS/ "S0_<user defined>.rt4" will be used instead of Remote Profile 12
                         //
                         // If SVS=2, Remote Profile 12 will be used instead of "S0_<user defined>.rt4"
                         //
                         //
                         // default is false // also recommended to set false to filter out unstable Extron inputs that can result in spamming the RT4K with profile changes 
                       


////////////////////////// 
                        // Choosing the above two options can get quite confusing (even for myself) so maybe this will help a little more:
                        //
                        // when S0=0 and SVS=0, button profiles 1 - 12 are used for EXTRON sw1, and SVS for EVERYTHING else
                        // when S0=0 and SVS=1, SVS profiles are used for everything
                        // when S0=0 and SVS=2, button profiles 1 - 8 are used for GSCART sw1, 9 - 12 for GSCART sw2 (ports 1 - 4), and SVS for EVERYTHING else
                        // when S0=1 and SVS=0, button profiles 1 - 11 are used for EXTRON sw1 and Remote Profile 12 as "Profile S0", and SVS for everything else 
                        // when S0=1 and SVS=1, SVS profiles for everything, and uses S0_<user defined>.rt4 as "Profile 0" 
                        // when S0=1 and SVS=2, button profiles 1 - 8 are used for GSCART sw1, 9 - 11 for GSCART sw2 (ports 1 - 3) and Remote Profile 12 as "Profile S0", and SVS for EVERYTHING else
                        //
//////////////////////////



uint8_t const voutMatrix[65] = {1,  // MATRIX switchers // by default ALL input changes to any/all outputs result in a profile change
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
                           

                           // ** Must be on firmware version 3.7 or higher **
uint8_t const RT5Xir = 1;     // 0 = disables IR Emitter for RetroTink 5x
                              // 1 = enabled for Extron sw1 / alt sw1, TESmart HDMI, MT-ViKi, or Otaku Games Scart Switch if connected
                              //     sends Profile 1 - 10 commands to RetroTink 5x. Must have IR LED emitter connected.
                              //
                              // 2 = enabled for gscart switch only (remote profiles 1-8 for first gscart, 9-10 for first 2 inputs on second gscart)
                              //
                              // 3 = enabled for Extron sw2 / alt sw2, TESmart HDMI, MT-ViKi, or Otaku Games Scart Switch if connected 
                              //     sends Profile 1 - 10 commands to RetroTink 5x. Must have IR LED emitter connected.

uint8_t const OSSCir = 0;     // 0 = disables IR Emitter for OSSC
                              // 1 = enabled for Extron sw1 switch, TESmart HDMI, or Otaku Games Scart Switch if connected
                              //     sends Profile 1 - 14 commands to OSSC. Must have IR LED emitter connected.
                              //     
                              // 2 = enabled for gscart switch only (remote profiles 1-8 for first gscart, 9-14 for first 6 inputs on second gscart)

uint8_t const MTVIKIir = 0;   // Must have IR "Receiver" connected to the Donut Dongle for option 1 & 2.
                              // 0 = disables IR Receiver -> Serial Control for MT-VIKI 8 Port HDMI switch
                              //
                              // 1 = MT-VIKI 8 Port HDMI switch connected to "Extron sw1"
                              //     Using the RT4K Remote w/ the IR Receiver, AUX8 + profile button changes the MT-VIKI Input over Serial.
                              //     Sends auxprof SVS profiles listed below.
                              //
                              // 2 = MT-VIKI 8 Port HDMI switch connected to "Extron sw2"
                              //     Using the RT4K Remote w/ the IR Receiver, AUX8 + profile button changes the MT-VIKI Input over Serial.
                              //     Sends auxprof SVS profiles listed below. You can change them below to 101 - 108 to prevent SVS profile conflicts if needed.


uint8_t const TESmartir = 1;  // Must have IR "Receiver" connected to the Donut Dongle for option 1 and above.
                              // 0 = disables IR Receiver -> Serial Control for TESmart 16x1 Port HDMI switch
                              //
                              // 1 = TESmart 16x1 HDMI switch connected to "alt sw1"
                              //     Using the RT4K Remote w/ the IR Receiver, AUX7 + profile button changes the Input over Serial. AUX7 + AUX1 - AUX4 for Input 13 - 16.
                              //     Sends SVS profile 1 - 16 as well.
                              //
                              // 2 = TESmart 16x1 HDMI switch connected to "alt sw2"
                              //     Using the RT4K Remote w/ the IR Receiver, AUX8 + profile button changes the Input over Serial. AUX8 + AUX1 - AUX4 for Input 13 - 16.
                              //     Sends SVS profile 101 - 116 as well.
                              //  ** this option overrides auxprof shown below  **
                              //
                              // 3 = TESmart 16x1 HDMI switch connected to BOTH "alt sw1" and "alt sw2"
                              //     Use AUX7 and AUX8 buttons as described above.
                              //  ** this option overrides auxprof shown below  **

uint8_t const auxprof[12] =   // Assign SVS profiles to IR remote profile buttons. 
                              // Replace 1, 2, 3, etc below with "ANY" SVS profile number.
                              // Press AUX8 then profile button to load. Must have IR Receiver connected and Serial connection to RT4K.
                              //
                              // ** Will not work if TESmartir above is set to 2 or 3 **
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

uint8_t const gctl = 1; // 1 = Enables gscart/gcomp manual input selection (Default)
                        // 0 = Disable
                        //
                        // ** Only supported on vers 5.x gscart/gcomp switches **
                        //
                        // AUX5 button + 1-8 button for gscart sw1, button 1 = input 1, etc
                        // AUX5 + 9-12 button to return to auto switching
                        //
                        // AUX6 button + 1-8 button for gscart sw2, button 1 = input 1, etc
                        // AUX6 + 9-12 button to return to auto switching
                        // 
                        // (RT5x and OSSC may require a repeat of the button combo for the IR signal to send.)

String const auxpower = "LG"; // AUX8 + Power button sends power off/on via IR Emitter. "LG" OLEX CX is the only one implemented atm. 

uint8_t const legacyOR[] = {1,2,3}; // For Legacy RT5x remote, when gctl = 1, you can use the bottom-most 3 buttons for manual input selection of gscart/gcomp inputs.
                                    //                         set to input 1, 2, and 3 as default. Values can be: 1 - 8, for gscart/gcomp sw1 only at the moment.
                                    //
                                    // Use the "Back/Exit" button to re-enable auto switching.
                                    //
                                    // ** Only supported on vers 5.x gscart/gcomp switches **



// gscart / gcomp adjustment variables for port detection
float const highsamvolt[2] = {1.6,1.6}; // for gscart sw1,sw2 rise above this voltage for a high sample
byte const apin[2][3] = {{A0,A1,A2},{A3,A4,A5}}; // defines analog pins used to read bit0, bit1, bit2
uint8_t const dch = 15; // (duty cycle high) at least this many high samples per "samsize" for a high bit (~75% duty cycle)
uint8_t const dcl = 5; // (duty cycle low) at least this many high samples and less than "dch" per "samsize" indicate all inputs are in-active (~50% duty cycle)
uint8_t const samsize = 20; // total number of ADC samples required to capture at least 1 period
uint8_t const fpdccountmax = 3; // number of periods required when in the 50% duty cycle state before a Profile 0 is triggered.


uint8_t extrabuttonprof = 0; // 3 = disabled | If you want to use AUX7, AUX8 buttons to control Scalable Video Switch inputs instead. 
                             // 0 = enabled (default)
                             //
                             // Used to keep track of AUX8 button presses for addtional button profiles
                             //

////////////////////////////////////////////////////////////////////////

// gscart / gcomp Global variables
uint8_t fpdcprev[2] = {0,0}; // stores 50% duty cycle detection
uint8_t fpdccount[2] = {0,0}; // number of times a 50% duty cycle period has been detected
uint8_t allgscartoff[2] = {2,2}; // 0 = at least 1 port is active, 1 = no ports are active, 2 = disconnected or not used yet
uint8_t samcc[2] = {0,0}; // ADC sample cycle counter
uint8_t highcount[2][3] = {{0,0,0},{0,0,0}}; // number of high samples recorded for bit0, bit1, bit2
uint8_t bitprev[2][3] = {{0,0,0},{0,0,0}}; // stores previous bit state
uint8_t auxgsw[2] = {0,0}; // gscart sw1,sw2 toggle override
uint8_t lastginput = 1; // used to keep track of overrideGscart last input

// TESmart remote control
uint8_t auxTESmart[2] = {0,0}; // used to keep track if aux7 was pressed to change inputs on TESmart 16x1 HDMI switch via RT4K remote.

// Extron sw1 / alt sw1 software serial port -> MAX3232 TTL IC (when jumpers set to "H")
SoftwareSerial extronSerial = SoftwareSerial(3,4); // setup a software serial port for listening to Extron sw1 / alt sw1. rxPin =3 / txPin = 4

// Extron sw2 / alt sw2 software serial port -> MAX3232 TTL IC (when jumpers set to "H")
AltSoftSerial extronSerial2; // setup yet another serial port for listening to Extron sw2 / alt sw2. hardcoded to pins D8 / D9

// Extron Global variables
String previnput[2] = {"discon","discon"}; // used to keep track of previous input
uint8_t otakuoff[2] = {2,2}; // 0 = at least 1 port is active, 1 = no ports are active, 2 = disconnected or not used yet
uint8_t eoutput[2]; // used to store Extron output

// IR Global variables
uint8_t repeatcount = 0; // used to help emulate the repeat nature of directional button presses
String svsbutton; // used to store 3 digit SVS profile when AUX8 is double pressed
uint8_t nument = 0; // used to keep track of how many digits have been entered for 3 digit SVS profile

// sendRTwake global variables
uint16_t currentProf[2] = {1,0}; // first index: 0 = remote button profile, 1 = SVS profiles. second index: profile number
bool RTwake = false;
unsigned long currentTime = 0;
unsigned long prevTime = 0;
unsigned long prevBlinkTime = 0;


////////////////////////////////////////////////////////////////////////

void setup(){

    pinMode(A2,INPUT);pinMode(A1,INPUT);pinMode(A0,INPUT); // set gscart1 port as input
    pinMode(A5,INPUT);pinMode(A4,INPUT);pinMode(A3,INPUT); // set gscart2 port as input
    initPCIInterruptForTinyReceiver(); // for IR Receiver
    Serial.begin(9600); // set the baud rate for the RT4K Serial Connection
    while(!Serial){;}   // allow connection to establish before continuing
    Serial.print(F("\r")); // clear RT4K Serial buffer
    extronSerial.begin(9600); // set the baud rate for the Extron sw1 Connection
    extronSerial.setTimeout(150); // sets the timeout for reading / saving into a string
    extronSerial2.begin(9600); // set the baud rate for Extron sw2 Connection
    extronSerial2.setTimeout(150); // sets the timeout for reading / saving into a string for the Extron sw2 Connection
    pinMode(LED_BUILTIN, OUTPUT); // initialize builtin led for RTwake

} // end of setup

void loop(){

  // below are a list of functions that loop over and over to read in port changes and other misc tasks. you can disable them by commenting them out

  readIR(); // intercepts the remote's button presses and relays them through the Serial interface giving a much more responsive experience and new functionality

  readGscart1(); // reads A0,A1,A2 pins to see which port, if any, are active

  readGscart2(); // reads A3,A4,A5 pins to see which port, if any, are active

  readExtron1(); // also reads TESmart HDMI and Otaku Games Scart switch on "alt sw1" port

  readExtron2(); // also reads TESmart HDMI and Otaku Games Scart switch on "alt sw2" port

  if(RTwake)sendRTwake(11900); // 11900 is 11.9 seconds. After waking the RT4K, wait this amount of time before re-sending the latest profile change.

} // end of loop()

void readExtron1(){

    byte ecapbytes[13]; // used to store first 13 captured bytes / messages for Extron                
    String ecap; // used to store Extron status messages for Extron in String format
    String einput; // used to store first 4 chars of Extron input

    // listens to the Extron sw1 Port for changes
    // SIS Command Responses reference - Page 77 https://media.extron.com/public/download/files/userman/XP300_Matrix_B.pdf
    if(extronSerial.available() > 0){ // if there is data available for reading, read
    extronSerial.readBytes(ecapbytes,13); // read in and store only the first 13 bytes for every status message received from 1st Extron SW port
      if(debugE1CAP){
        Serial.print(F("ecap HEX: "));
        for(int i=0;i<13;i++){
          Serial.print(ecapbytes[i],HEX);Serial.print(F(" "));
        }
        Serial.println(F("\r"));
        ecap = String((char *)ecapbytes);
        Serial.print(F("ecap ASCII: "));Serial.println(ecap);
      }
    }
    ecap = String((char *)ecapbytes); // convert bytes to String for Extron switches


    if(ecap.substring(0,3) == "Out"){ // store only the input and output states, some Extron devices report output first instead of input
      einput = ecap.substring(6,10);
      eoutput[0] = ecap.substring(3,5).toInt();
    }
    else if(ecap.substring(0,1) == "F"){ // detect if switch has changed auto/manual states
      einput = ecap.substring(4,8);
      eoutput[0] = 0;
    }
    else if(ecap.substring(0,3) == "Rpr"){ // detect if a Preset has been used
      einput = ecap.substring(0,5);
      eoutput[0] = 0;
    }
    else{                             // less complex switches only report input status, no output status
      einput = ecap.substring(0,4);
      eoutput[0] = 0;
    }


    // for Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references voutMaxtrix
    if((einput.substring(0,2) == "In" && voutMatrix[eoutput[0]]) || (einput.substring(0,3) == "Rpr")){
      if(einput == "In1 " || einput == "In01" || einput == "Rpr01"){
        if(RT5Xir == 1)sendIR("5x",1,2); // RT5X profile 1 
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",1,3); // OSSC profile 1

        if(SVS==0)sendRBP(1);
        else sendSVS(1);
      }
      else if(einput == "In2 " || einput == "In02" || einput == "Rpr02"){
        if(RT5Xir == 1)sendIR("5x",2,2); // RT5X profile 2
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",2,3); // OSSC profile 2

        if(SVS==0)sendRBP(2);
        else sendSVS(2);
      }
      else if(einput == "In3 " || einput == "In03" || einput == "Rpr03"){
        if(RT5Xir == 1)sendIR("5x",3,2); // RT5X profile 3
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",3,3); // OSSC profile 3

        if(SVS==0)sendRBP(3);
        else sendSVS(3);
      }
      else if(einput == "In4 " || einput == "In04" || einput == "Rpr04"){
        if(RT5Xir == 1)sendIR("5x",4,2); // RT5X profile 4
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",4,3); // OSSC profile 4

        if(SVS==0)sendRBP(4);
        else sendSVS(4);
      }
      else if(einput == "In5 " || einput == "In05" || einput == "Rpr05"){
        if(RT5Xir == 1)sendIR("5x",5,2); // RT5X profile 5
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",5,3); // OSSC profile 5

        if(SVS==0)sendRBP(5);
        else sendSVS(5);
      }
      else if(einput == "In6 " || einput == "In06" || einput == "Rpr06"){
        if(RT5Xir == 1)sendIR("5x",6,2); // RT5X profile 6
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",6,3); // OSSC profile 6

        if(SVS==0)sendRBP(6);
        else sendSVS(6);
      }
      else if(einput == "In7 " || einput == "In07" || einput == "Rpr07"){
        if(RT5Xir == 1)sendIR("5x",7,2); // RT5X profile 7
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",7,3); // OSSC profile 7

        if(SVS==0)sendRBP(7);
        else sendSVS(7);
      }
      else if(einput == "In8 " || einput == "In08" || einput == "Rpr08"){
        if(RT5Xir == 1)sendIR("5x",8,2); // RT5X profile 8
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",8,3); // OSSC profile 8

        if(SVS==0)sendRBP(8);
        else sendSVS(8);
      }
      else if(einput == "In9 " || einput == "In09" || einput == "Rpr09"){
        if(RT5Xir == 1)sendIR("5x",9,2); // RT5X profile 9
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",9,3); // OSSC profile 9

        if(SVS==0)sendRBP(9);
        else sendSVS(9);
      }
      else if(einput == "In10" || einput == "Rpr10"){
        if(RT5Xir == 1)sendIR("5x",10,2); // RT5X profile 10
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",10,3); // OSSC profile 10

        if(SVS==0)sendRBP(10);
        else sendSVS(10);
      }
      else if(einput == "In11" || einput == "Rpr11"){
        if(OSSCir == 1)sendIR("ossc",11,3); // OSSC profile 11

        if(SVS==0)sendRBP(11);
        else sendSVS(11);
      }
      else if(einput == "In12" || einput == "Rpr12"){
        if(OSSCir == 1)sendIR("ossc",12,3); // OSSC profile 12

        if((SVS==0 && !S0))sendRBP(12); // okay to use this profile if S0 is set to false
        else sendSVS(12);
      }
      else if(einput == "In13" || einput == "Rpr13"){
        if(OSSCir == 1)sendIR("ossc",13,3); // OSSC profile 13
        sendSVS(13);
      }
      else if(einput == "In14" || einput == "Rpr14"){
        if(OSSCir == 1)sendIR("ossc",14,3); // OSSC profile 14
        sendSVS(14);
      }
      else if(einput.substring(0,3) == "Rpr"){
        sendSVS(einput.substring(3,5).toInt());
      }
      else if(einput != "In0 " && einput != "In00"){ // for inputs 13-99 (SVS only)
        sendSVS(einput.substring(2,4).toInt());
      }

      previnput[0] = einput;

      // Extron S0
      // when both Extron switches match In0 or In00 (no active ports), both gscart/gcomp/otaku are disconnected or all ports in-active, a S0 Profile can be loaded if S0 is enabled
      if(S0 && ((einput == "In0 " || einput == "In00") && 
        (previnput[1] == "In0 " || previnput[1] == "In00" || previnput[1] == "discon")) && 
        otakuoff[0] && otakuoff[1] && allgscartoff[0] && allgscartoff[1] && 
        voutMatrix[eoutput[0]] && (previnput[1] == "discon" || voutMatrix[eoutput[1]+32])){

        if(SVS == 1)sendSVS(0);
        else sendRBP(12);

        previnput[0] = "0";
        if(previnput[1] != "discon")previnput[1] = "0";
      
      } // end of Extron S0

      if(previnput[0] == "0" && previnput[1].substring(0,2) == "In")previnput[0] = "In00";  // changes previnput[0] "0" state to "In00" when there is a newly active input on the other switch
      if(previnput[1] == "0" && previnput[0].substring(0,2) == "In")previnput[1] = "In00";

    }

    // for TESmart 4K60 / TESmart 4K30 / MT-VIKI HDMI switch on Extron sw1 / alt sw1
    if(ecapbytes[4] == 17 || ecapbytes[3] == 17 || ecapbytes[4] == 95){
      if(ecapbytes[6] == 22 || ecapbytes[5] == 22 || ecapbytes[11] == 48){
        if(RT5Xir == 1)sendIR("5x",1,2); // RT5X profile 1 
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",1,3); // OSSC profile 1

        if(SVS==0)sendRBP(1);
        else sendSVS(1);
      }
      else if(ecapbytes[6] == 23 || ecapbytes[5] == 23 || ecapbytes[11] == 49){
        if(RT5Xir == 1)sendIR("5x",2,2); // RT5X profile 2
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",2,3); // OSSC profile 2

        if(SVS==0)sendRBP(2);
        else sendSVS(2);
      }
      else if(ecapbytes[6] == 24 || ecapbytes[5] == 24 || ecapbytes[11] == 50){
        if(RT5Xir == 1)sendIR("5x",3,2); // RT5X profile 3
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",3,3); // OSSC profile 3

        if(SVS==0)sendRBP(3);
        else sendSVS(3);
      }
      else if(ecapbytes[6] == 25 || ecapbytes[5] == 25 || ecapbytes[11] == 51){
        if(RT5Xir == 1)sendIR("5x",4,2); // RT5X profile 4
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",4,3); // OSSC profile 4

        if(SVS==0)sendRBP(4);
        else sendSVS(4);
      }
      else if(ecapbytes[6] == 26 || ecapbytes[5] == 26 || ecapbytes[11] == 52){
        if(RT5Xir == 1)sendIR("5x",5,2); // RT5X profile 5
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",5,3); // OSSC profile 5

        if(SVS==0)sendRBP(5);
        else sendSVS(5);
      }
      else if(ecapbytes[6] == 27 || ecapbytes[5] == 27 || ecapbytes[11] == 53){
        if(RT5Xir == 1)sendIR("5x",6,2); // RT5X profile 6
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",6,3); // OSSC profile 6

        if(SVS==0)sendRBP(6);
        else sendSVS(6);
      }
      else if(ecapbytes[6] == 28 || ecapbytes[5] == 28 || ecapbytes[11] == 54){
        if(RT5Xir == 1)sendIR("5x",7,2); // RT5X profile 7
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",7,3); // OSSC profile 7

        if(SVS==0)sendRBP(7);
        else sendSVS(7);
      }
      else if(ecapbytes[6] == 29 || ecapbytes[5] == 29 || ecapbytes[11] == 55){
        if(RT5Xir == 1)sendIR("5x",8,2); // RT5X profile 8
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",8,3); // OSSC profile 8

        if(SVS==0)sendRBP(8);
        else sendSVS(8);
      }
      else if(ecapbytes[6] == 30 || ecapbytes[5] == 30){
        if(RT5Xir == 1)sendIR("5x",9,2); // RT5X profile 9
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",9,3); // OSSC profile 9

        if(SVS==0)sendRBP(9);
        else sendSVS(9);
      }
      else if(ecapbytes[6] == 31 || ecapbytes[5] == 31){
        if(RT5Xir == 1)sendIR("5x",10,2); // RT5X profile 10
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",10,3); // OSSC profile 10

        if(SVS==0)sendRBP(10);
        else sendSVS(10);
      }
      else if(ecapbytes[6] == 32 || ecapbytes[5] == 32){
        if(OSSCir == 1)sendIR("ossc",11,3); // OSSC profile 11

        if(SVS==0)sendRBP(11);
        else sendSVS(11);
      }
      else if(ecapbytes[6] == 33 || ecapbytes[5] == 33){
        //if((OSSCir == 1) && !S0)sendIR("ossc",12,3); // OSSC profile 12
        if(OSSCir == 1)sendIR("ossc",12,3); // OSSC profile 12

        if(SVS==0 && !S0)sendRBP(12); // okay to use this profile if S0 is set to false
        else sendSVS(12);
      }
      else if(ecapbytes[6] == 34 || ecapbytes[5] == 34){
        if(OSSCir == 1)sendIR("ossc",13,3); // OSSC profile 13
        sendSVS(13);
      }
      else if(ecapbytes[6] == 35 || ecapbytes[5] == 35){
        if(OSSCir == 1)sendIR("ossc",14,3); // OSSC profile 14
        sendSVS(14);
      }
      else if(ecapbytes[6] > 35 && ecapbytes[6] < 38){
        sendSVS(ecapbytes[6] - 21);
      }
      else if(ecapbytes[5] > 35 && ecapbytes[5] < 38){
        sendSVS(ecapbytes[5] - 21);
      }      
    }

    // set ecapbytes to 0 for next read
    memset(ecapbytes,0,sizeof(ecapbytes)); // ecapbytes is local variable, but superstitious clearing regardless :) 

    // for Otaku Games Scart Switch 1
    if(ecap.substring(0,6) == "remote"){
      otakuoff[0] = 0;
      if(ecap.substring(0,13) == "remote prof10"){
          if(RT5Xir == 1)sendIR("5x",10,2); // RT5X profile 10
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 1)sendIR("ossc",10,3); // OSSC profile 10

          if(SVS==0) sendRBP(10);
          else sendSVS(10);
      }
      else if(ecap.substring(0,13) == "remote prof12"){
        otakuoff[0] = 1;
        if(S0 && otakuoff[1] && 
          allgscartoff[0] && allgscartoff[1] && 
          ((previnput[0] == "0" || previnput[0] == "discon" || previnput[0] == "In0 " || previnput[0] == "In00") && // cross-checks gscart, otaku2, Extron status
           (previnput[1] == "0" || previnput[1] == "discon" || previnput[1] == "In0 " || previnput[1] == "In00"))){
          
          if(SVS == 1)sendSVS(0);
          else sendRBP(12);

        }
      }
      else if(ecap.substring(0,12) == "remote prof1"){
          if(RT5Xir == 1)sendIR("5x",1,2); // RT5X profile 1 
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 1)sendIR("ossc",1,3); // OSSC profile 1

          if(SVS==0) sendRBP(1);
          else sendSVS(1);
      }
      else if(ecap.substring(0,12) == "remote prof2"){
          if(RT5Xir == 1)sendIR("5x",2,2); // RT5X profile 2
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 1)sendIR("ossc",2,3); // OSSC profile 2

          if(SVS==0) sendRBP(2);
          else sendSVS(2);
      }
      else if(ecap.substring(0,12) == "remote prof3"){
          if(RT5Xir == 1)sendIR("5x",3,2); // RT5X profile 3
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 1)sendIR("ossc",3,3); // OSSC profile 3

          if(SVS==0) sendRBP(3);
          else sendSVS(3);
      }
      else if(ecap.substring(0,12) == "remote prof4"){
          if(RT5Xir == 1)sendIR("5x",4,2); // RT5X profile 4
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 1)sendIR("ossc",4,3); // OSSC profile 4

          if(SVS==0) sendRBP(4);
          else sendSVS(4);
      }
      else if(ecap.substring(0,12) == "remote prof5"){
          if(RT5Xir == 1)sendIR("5x",5,2); // RT5X profile 5
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 1)sendIR("ossc",5,3); // OSSC profile 5

          if(SVS==0) sendRBP(5);
          else sendSVS(5);
      }
      else if(ecap.substring(0,12) == "remote prof6"){
          if(RT5Xir == 1)sendIR("5x",6,2); // RT5X profile 6
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 1)sendIR("ossc",6,3); // OSSC profile 6

          if(SVS==0) sendRBP(6);
          else sendSVS(6);
      }
      else if(ecap.substring(0,12) == "remote prof7"){
          if(RT5Xir == 1)sendIR("5x",7,2); // RT5X profile 7
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 1)sendIR("ossc",7,3); // OSSC profile 7

          if(SVS==0) sendRBP(7);
          else sendSVS(7);
      }
      else if(ecap.substring(0,12) == "remote prof8"){
          if(RT5Xir == 1)sendIR("5x",8,2); // RT5X profile 8
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 1)sendIR("ossc",8,3); // OSSC profile 8

          if(SVS==0) sendRBP(8);
          else sendSVS(8);
      }
      else if(ecap.substring(0,12) == "remote prof9"){
          if(RT5Xir == 1)sendIR("5x",9,2); // RT5X profile 9
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 1)sendIR("ossc",9,3); // OSSC profile 9

          if(SVS==0) sendRBP(9);
          else sendSVS(9);
      }
    }

} // end of readExtron1()

void readExtron2(){
    
    byte ecapbytes[13]; // used to store first 13 captured bytes / messages for Extron                
    String ecap; // used to store Extron status messages for Extron in String format
    String einput; // used to store first 4 chars of Extron input

    // listens to the Extron sw2 Port for changes
    if(extronSerial2.available() > 0){ // if there is data available for reading, read
    extronSerial2.readBytes(ecapbytes,13); // read in and store only the first 13 bytes for every status message received from 2nd Extron port
      if(debugE2CAP){
        Serial.print(F("ecap2 HEX: "));
        for(int i=0;i<13;i++){
          Serial.print(ecapbytes[i],HEX);Serial.print(F(" "));
        }
        Serial.println(F("\r"));
        ecap = String((char *)ecapbytes);
        Serial.print(F("ecap2 ASCII: "));Serial.println(ecap);
      }
    }
    ecap = String((char *)ecapbytes);

    if(ecap.substring(0,3) == "Out"){ // store only the input and output states, some Extron devices report output first instead of input
      einput = ecap.substring(6,10);
      eoutput[1] = ecap.substring(3,5).toInt();
    }
    else if(ecap.substring(0,1) == "F"){ // detect if switch has changed auto/manual states
      einput = ecap.substring(4,8);
      eoutput[1] = 0;
    }
    else if(ecap.substring(0,3) == "Rpr"){ // detect if a Preset has been used
      einput = ecap.substring(0,5);
      eoutput[1] = 0;
    }
    else{                              // less complex switches only report input status, no output status
      einput = ecap.substring(0,4);
      eoutput[1] = 0;
    }


    // For Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references voutMaxtrix
    if((einput.substring(0,2) == "In" && voutMatrix[eoutput[1]+32]) || (einput.substring(0,3) == "Rpr")){
      if(einput.substring(0,3) == "Rpr"){
        sendSVS(einput.substring(3,5).toInt()+100);
      }
      else if(einput != "In0 " && einput != "In00"){ // much easier method for switch 2 since ALL inputs will respond with SVS commands regardless of SVS option above
        if(einput.substring(3,4) == " ") 
          sendSVS(einput.substring(2,3).toInt()+100);
        else 
          sendSVS(einput.substring(2,4).toInt()+100);
      }

      previnput[1] = einput;
      
      // Extron2 S0
      // when both Extron switches match In0 or In00 (no active ports), both gscart/gcomp/otaku are disconnected or all ports in-active, a Profile 0 can be loaded if S0 is enabled
      if(S0 && otakuoff[0] && otakuoff[1] &&
        allgscartoff[0] && allgscartoff[1] && 
        ((einput == "In0 " || einput == "In00") && 
        (previnput[0] == "In0 " || previnput[0] == "In00" || previnput[0] == "discon")) && 
        (previnput[0] == "discon" || voutMatrix[eoutput[0]]) && voutMatrix[eoutput[1]+32]){

        if(SVS == 1)sendSVS(0);
        else sendRBP(12);

        previnput[1] = "0";
        if(previnput[0] != "discon")previnput[0] = "0";
      
      } // end of Extron2 S0

      if(previnput[0] == "0" && previnput[1].substring(0,2) == "In")previnput[0] = "In00";  // changes previnput[0] "0" state to "In00" when there is a newly active input on the other switch
      if(previnput[1] == "0" && previnput[0].substring(0,2) == "In")previnput[1] = "In00";

    }


    // for TESmart 4K60 / TESmart 4K30 / MT-VIKI HDMI switch on Extron alt sw2 Port
    if(ecapbytes[4] == 17 || ecapbytes[3] == 17 || ecapbytes[4] == 95){
      if(ecapbytes[6] == 22 || ecapbytes[5] == 22 || ecapbytes[11] == 48){
        if(RT5Xir == 3)sendIR("5x",1,2); // RT5X profile 1 
        sendSVS(101);
      }
      else if(ecapbytes[6] == 23 || ecapbytes[5] == 23 || ecapbytes[11] == 49){
        if(RT5Xir == 3)sendIR("5x",2,2); // RT5X profile 2
        sendSVS(102);
      }
      else if(ecapbytes[6] == 24 || ecapbytes[5] == 24 || ecapbytes[11] == 50){
        if(RT5Xir == 3)sendIR("5x",3,2); // RT5X profile 3
        sendSVS(103);
      }
      else if(ecapbytes[6] == 25 || ecapbytes[5] == 25 || ecapbytes[11] == 51){
        if(RT5Xir == 3)sendIR("5x",4,2); // RT5X profile 4
        sendSVS(104);
      }
      else if(ecapbytes[6] == 26 || ecapbytes[5] == 26 || ecapbytes[11] == 52){
        if(RT5Xir == 3)sendIR("5x",5,2); // RT5X profile 5
        sendSVS(105);
      }
      else if(ecapbytes[6] == 27 || ecapbytes[5] == 27 || ecapbytes[11] == 53){
        if(RT5Xir == 3)sendIR("5x",6,2); // RT5X profile 6
        sendSVS(106);
      }
      else if(ecapbytes[6] == 28 || ecapbytes[5] == 28 || ecapbytes[11] == 54){
        if(RT5Xir == 3)sendIR("5x",7,2); // RT5X profile 7
        sendSVS(107);
      }
      else if(ecapbytes[6] == 29 || ecapbytes[5] == 29 || ecapbytes[11] == 55){
        if(RT5Xir == 3)sendIR("5x",8,2); // RT5X profile 8
        sendSVS(108);
      }
      else if(ecapbytes[6] == 30 || ecapbytes[5] == 30){
        if(RT5Xir == 3)sendIR("5x",9,2); // RT5X profile 9
        sendSVS(109);
      }
      else if(ecapbytes[6] == 31 || ecapbytes[5] == 31){
        if(RT5Xir == 3)sendIR("5x",10,2); // RT5X profile 10
        sendSVS(110);
      }
      else if(ecapbytes[6] > 31 && ecapbytes[6] < 38){
        sendSVS(ecapbytes[6] + 79);
      }
      else if(ecapbytes[5] > 31 && ecapbytes[5] < 38){
        sendSVS(ecapbytes[5] + 79);
      }
    }

    // set ecapbytes to 0 for next read
    memset(ecapbytes,0,sizeof(ecapbytes)); // ecapbytes is local variable, but superstitious clearing regardless :) 

    // for Otaku Games Scart Switch 2
    if(ecap.substring(0,6) == "remote"){
      otakuoff[1] = 0;
      if(ecap.substring(0,13) == "remote prof10"){
        if(RT5Xir == 3)sendIR("5x",10,2); // RT5X profile 10
        sendSVS(110);
      }
      else if(ecap.substring(0,13) == "remote prof12"){
        otakuoff[1] = 1;
        if(S0 && otakuoff[0] && 
          allgscartoff[0] && allgscartoff[1] && 
          ((previnput[0] == "0" || previnput[0] == "discon" || previnput[0] == "In0 " || previnput[0] == "In00") && // cross-checks gscart, otaku, Extron status
          (previnput[1] == "0" || previnput[1] == "discon" || previnput[1] == "In0 " || previnput[1] == "In00"))){

            if(SVS == 1)sendSVS(0);
            else sendRBP(12);

        }
      }
      else if(ecap.substring(0,12) == "remote prof1"){
        if(RT5Xir == 3)sendIR("5x",1,2); // RT5X profile 1 
        sendSVS(101);
      }
      else if(ecap.substring(0,12) == "remote prof2"){
        if(RT5Xir == 3)sendIR("5x",2,2); // RT5X profile 2
        sendSVS(102);
      }
      else if(ecap.substring(0,12) == "remote prof3"){
        if(RT5Xir == 3)sendIR("5x",3,2); // RT5X profile 3
        sendSVS(103);
      }
      else if(ecap.substring(0,12) == "remote prof4"){
        if(RT5Xir == 3)sendIR("5x",4,2); // RT5X profile 4
        sendSVS(104);
      }
      else if(ecap.substring(0,12) == "remote prof5"){
        if(RT5Xir == 3)sendIR("5x",5,2); // RT5X profile 5
        sendSVS(105);
      }
      else if(ecap.substring(0,12) == "remote prof6"){
        if(RT5Xir == 3)sendIR("5x",6,2); // RT5X profile 6
        sendSVS(106);
      }
      else if(ecap.substring(0,12) == "remote prof7"){
        if(RT5Xir == 3)sendIR("5x",7,2); // RT5X profile 7
        sendSVS(107);
      }
      else if(ecap.substring(0,12) == "remote prof8"){
        if(RT5Xir == 3)sendIR("5x",8,2); // RT5X profile 8
        sendSVS(108);
      }
      else if(ecap.substring(0,12) == "remote prof9"){
        if(RT5Xir == 3)sendIR("5x",9,2); // RT5X profile 9
        sendSVS(109);
      }
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

  for(uint8_t i=0;i<3;i++){ // read in analog pin voltages, read each pin 4x in a row to ensure an accurate read, combats if using too large of a pull-down resistor
    for(uint8_t j=0;j<4;j++){
      val[i] = analogRead(apin[0][i]);
    }
    if((val[i]/211) >= highsamvolt[0]){ // if voltage is greater than or equal to the voltage defined for a high, increase highcount[0] by 1 for that analog pin
      highcount[0][i]++;
    }
  }

  if(samcc[0] == samsize){               // when the "samsize" number of samples has been taken, if the voltage was "high" for more than "dch" # of the "samsize" samples, set the bit to 1
    for(uint8_t i=0;i<3;i++){
      if(highcount[0][i] > dch)          // if the number of "high" samples per "samsize" are greater than "dch" set bit to 1.  
        bit[i] = 1;
      else if(highcount[0][i] > dcl)     // if the number of "high" samples is greater than "dcl" and less than "dch" (50% high samples) the switch must be cycling inputs, therefor no active input
        fpdc = 1;
    }
  }



  if(((bit[2] != bitprev[0][2] || bit[1] != bitprev[0][1] || bit[0] != bitprev[0][0]) || (allgscartoff[0] == 1)) && (samcc[0] == samsize) && !(fpdc)){
    //Detect which scart port is now active and change profile accordingly
    if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 0)){ // 0 0 0
      if(RT5Xir == 2)sendIR("5x",1,2); // RT5X profile 1 
      if(RT5Xir && OSSCir)delay(500);
      if(OSSCir == 2)sendIR("ossc",1,3); // OSSC profile 1

      if(SVS==2)sendRBP(1);
      else sendSVS(201);
    } 
    else if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 1)){ // 0 0 1
      if(RT5Xir == 2)sendIR("5x",2,2); // RT5X profile 2
      if(RT5Xir && OSSCir)delay(500);
      if(OSSCir == 2)sendIR("ossc",2,3); // OSSC profile 2

      if(SVS==2)sendRBP(2);
      else sendSVS(202);
    }
    else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 0)){ // 0 1 0
      if(RT5Xir == 2)sendIR("5x",3,2); // RT5X profile 3
      if(RT5Xir && OSSCir)delay(500);
      if(OSSCir == 2)sendIR("ossc",3,3); // OSSC profile 3

      if(SVS==2)sendRBP(3);
      else sendSVS(203);
    }
    else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 1)){ // 0 1 1
      if(RT5Xir == 2)sendIR("5x",4,2); // RT5X profile 4
      if(RT5Xir && OSSCir)delay(500);
      if(OSSCir == 2)sendIR("ossc",4,3); // OSSC profile 4

      if(SVS==2)sendRBP(4);
      else sendSVS(204);
    }
    else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 0)){ // 1 0 0
      if(RT5Xir == 2)sendIR("5x",5,2); // RT5X profile 5
      if(RT5Xir && OSSCir)delay(500);
      if(OSSCir == 2)sendIR("ossc",5,3); // OSSC profile 5

      if(SVS==2)sendRBP(5);
      else sendSVS(205);
    } 
    else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 1)){ // 1 0 1
      if(RT5Xir == 2)sendIR("5x",6,2); // RT5X profile 6
      if(RT5Xir && OSSCir)delay(500);
      if(OSSCir == 2)sendIR("ossc",6,3); // OSSC profile 6

      if(SVS==2)sendRBP(6);
      else sendSVS(206);
    }   
    else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 0)){ // 1 1 0
      if(RT5Xir == 2)sendIR("5x",7,2); // RT5X profile 7
      if(RT5Xir && OSSCir)delay(500);
      if(OSSCir == 2)sendIR("ossc",7,3); // OSSC profile 7

      if(SVS==2)sendRBP(7);
      else sendSVS(207);
    } 
    else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 1)){ // 1 1 1
      if(RT5Xir == 2)sendIR("5x",8,2); // RT5X profile 8
      if(RT5Xir && OSSCir)delay(500);
      if(OSSCir == 2)sendIR("ossc",8,3); // OSSC profile 8

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

    if(S0 && otakuoff[0] && 
      otakuoff[1] && allgscartoff[1] && 
      ((previnput[0] == "0" || previnput[0] == "discon" || previnput[0] == "In0 " || previnput[0] == "In00") && // cross-checks otaku, gscart2, Extron status
       (previnput[1] == "0" || previnput[1] == "discon" || previnput[1] == "In0 " || previnput[1] == "In00"))){

      if(SVS == 1)sendSVS(0);
      else sendRBP(12);

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

  for(uint8_t i=0;i<3;i++){
    for(uint8_t j=0;j<4;j++){
      val[i] = analogRead(apin[1][i]);
    }
    if((val[i]/211) >= highsamvolt[1]){
      highcount[1][i]++;
    }
  }

  if(samcc[1] == samsize){              // when the "samsize" number of samples has been taken, if the voltage was high for more than dch of the samples, set the bit to 1
    for(uint8_t i=0;i<3;i++){   // if the voltage was high for only dcl to dch samples, set an all in-active ports flag (fpdc = 1)
      if(highcount[1][i] > dch)         // how many "high" samples per "samsize" are required for a bit to be 1.  
        bit[i] = 1;
      else if(highcount[1][i] > dcl)
        fpdc = 1;
    }
  }


  if(((bit[2] != bitprev[1][2] || bit[1] != bitprev[1][1] || bit[0] != bitprev[1][0]) || (allgscartoff[1] == 1)) && (samcc[1] == samsize) && !(fpdc)){
        //Detect which scart port is now active and change profile accordingly
        if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 0)){ // 0 0 0
          if(RT5Xir == 2)sendIR("5x",9,2); // RT5X profile 9
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 2)sendIR("ossc",9,3); // OSSC profile 9

          if(SVS==2)sendRBP(9);
          else sendSVS(209);
        } 
        else if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 1)){ // 0 0 1
          if(RT5Xir == 2)sendIR("5x",10,2); // RT5X profile 10
          if(RT5Xir && OSSCir)delay(500);
          if(OSSCir == 2)sendIR("ossc",10,3); // OSSC profile 10

          if(SVS==2)sendRBP(10);
          else sendSVS(210);
        }
        else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 0)){ // 0 1 0
          if(OSSCir == 2)sendIR("ossc",11,3); // OSSC profile 11

          if(SVS==2)sendRBP(11);
          else sendSVS(211);
        }
        else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 1)){ // 0 1 1
          if(OSSCir == 2)sendIR("ossc",12,3); // OSSC profile 12

          if(SVS==2 && !S0)sendRBP(12);
          else sendSVS(212);
        }
        else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 0)){ // 1 0 0
          if(OSSCir == 2)sendIR("ossc",13,3); // OSSC profile 13
          sendSVS(213);
        }
        else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 1)){ // 1 0 1
          if(OSSCir == 2)sendIR("ossc",14,3); // OSSC profile 14
          sendSVS(214);
        }
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
    
    if(S0 && allgscartoff[0] && 
      otakuoff[0] && otakuoff[1] && 
      ((previnput[0] == "0" || previnput[0] == "discon" || previnput[0] == "In0 " || previnput[0] == "In00") &&  // cross-checks gscart, otaku, Extron status
       (previnput[1] == "0" || previnput[1] == "discon" || previnput[1] == "In0 " || previnput[1] == "In00"))){

      if(SVS == 1)sendSVS(0);
      else sendRBP(12);

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

void readIR(){

  uint8_t ir_recv_command = 0;
  uint8_t ir_recv_address = 0;
  byte viki1[4] = {0xA5,0x5A,0x00,0xCC};
  byte viki2[4] = {0xA5,0x5A,0x01,0xCC};
  byte viki3[4] = {0xA5,0x5A,0x02,0xCC};
  byte viki4[4] = {0xA5,0x5A,0x03,0xCC};
  byte viki5[4] = {0xA5,0x5A,0x04,0xCC};
  byte viki6[4] = {0xA5,0x5A,0x05,0xCC};
  byte viki7[4] = {0xA5,0x5A,0x06,0xCC};
  byte viki8[4] = {0xA5,0x5A,0x07,0xCC};

  byte tesmart1[6] = {0xAA,0xBB,0x03,0x01,0x01,0xEE};
  byte tesmart2[6] = {0xAA,0xBB,0x03,0x01,0x02,0xEE};
  byte tesmart3[6] = {0xAA,0xBB,0x03,0x01,0x03,0xEE};
  byte tesmart4[6] = {0xAA,0xBB,0x03,0x01,0x04,0xEE};
  byte tesmart5[6] = {0xAA,0xBB,0x03,0x01,0x05,0xEE};
  byte tesmart6[6] = {0xAA,0xBB,0x03,0x01,0x06,0xEE};
  byte tesmart7[6] = {0xAA,0xBB,0x03,0x01,0x07,0xEE};
  byte tesmart8[6] = {0xAA,0xBB,0x03,0x01,0x08,0xEE};
  byte tesmart9[6] = {0xAA,0xBB,0x03,0x01,0x09,0xEE};
  byte tesmart10[6] = {0xAA,0xBB,0x03,0x01,0x0A,0xEE};
  byte tesmart11[6] = {0xAA,0xBB,0x03,0x01,0x0B,0xEE};
  byte tesmart12[6] = {0xAA,0xBB,0x03,0x01,0x0C,0xEE};
  byte tesmart13[6] = {0xAA,0xBB,0x03,0x01,0x0D,0xEE};
  byte tesmart14[6] = {0xAA,0xBB,0x03,0x01,0x0E,0xEE};
  byte tesmart15[6] = {0xAA,0xBB,0x03,0x01,0x0F,0xEE};
  byte tesmart16[6] = {0xAA,0xBB,0x03,0x01,0x10,0xEE};

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
        if(MTVIKIir == 0 && TESmartir < 2)sendSVS(auxprof[0]);                                                               // Can be changed to "ANY" SVS profile in the OPTIONS section
        if(MTVIKIir == 1){extronSerial.write(viki1,4);sendSVS(auxprof[0]);}
        if(MTVIKIir == 2){extronSerial2.write(viki1,4);sendSVS(auxprof[0]);}
        if(TESmartir > 1){extronSerial2.write(tesmart1,6);sendSVS(101);};                                                                      
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        if(MTVIKIir == 0 && TESmartir < 2)sendSVS(auxprof[1]);
        if(MTVIKIir == 1){extronSerial.write(viki2,4);sendSVS(auxprof[1]);}
        if(MTVIKIir == 2){extronSerial2.write(viki2,4);sendSVS(auxprof[1]);}
        if(TESmartir > 1){extronSerial2.write(tesmart2,6);sendSVS(102);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
        if(MTVIKIir == 0 && TESmartir < 2)sendSVS(auxprof[2]);
        if(MTVIKIir == 1){extronSerial.write(viki3,4);sendSVS(auxprof[2]);}
        if(MTVIKIir == 2){extronSerial2.write(viki3,4);sendSVS(auxprof[2]);}
        if(TESmartir > 1){extronSerial2.write(tesmart3,6);sendSVS(103);}; 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        if(MTVIKIir == 0 && TESmartir < 2)sendSVS(auxprof[3]);
        if(MTVIKIir == 1){extronSerial.write(viki4,4);sendSVS(auxprof[3]);}
        if(MTVIKIir == 2){extronSerial2.write(viki4,4);sendSVS(auxprof[3]);}
        if(TESmartir > 1){extronSerial2.write(tesmart4,6);sendSVS(104);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        if(MTVIKIir == 0 && TESmartir < 2)sendSVS(auxprof[4]);
        if(MTVIKIir == 1){extronSerial.write(viki5,4);sendSVS(auxprof[4]);}
        if(MTVIKIir == 2){extronSerial2.write(viki5,4);sendSVS(auxprof[4]);}
        if(TESmartir > 1){extronSerial2.write(tesmart5,6);sendSVS(105);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        if(MTVIKIir == 0 && TESmartir < 2)sendSVS(auxprof[5]);
        if(MTVIKIir == 1){extronSerial.write(viki6,4);sendSVS(auxprof[5]);}
        if(MTVIKIir == 2){extronSerial2.write(viki6,4);sendSVS(auxprof[5]);}
        if(TESmartir > 1){extronSerial2.write(tesmart6,6);sendSVS(106);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        if(MTVIKIir == 0 && TESmartir < 2)sendSVS(auxprof[6]);
        if(MTVIKIir == 1){extronSerial.write(viki7,4);sendSVS(auxprof[6]);}
        if(MTVIKIir == 2){extronSerial2.write(viki7,4);sendSVS(auxprof[6]);}
        if(TESmartir > 1){extronSerial2.write(tesmart7,6);sendSVS(107);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        if(MTVIKIir == 0 && TESmartir < 2)sendSVS(auxprof[7]);
        if(MTVIKIir == 1){extronSerial.write(viki8,4);sendSVS(auxprof[7]);}
        if(MTVIKIir == 2){extronSerial2.write(viki8,4);sendSVS(auxprof[7]);}
        if(TESmartir > 1){extronSerial2.write(tesmart8,6);sendSVS(108);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 1){ // profile button 9
        if(TESmartir < 2)sendSVS(auxprof[8]);
        if(TESmartir > 1){extronSerial2.write(tesmart9,6);sendSVS(109);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 37){ // profile button 10
        if(TESmartir < 2)sendSVS(auxprof[9]);
        if(TESmartir > 1){extronSerial2.write(tesmart10,6);sendSVS(110);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 38){ // profile button 11
        if(TESmartir < 2)sendSVS(auxprof[10]);
        if(TESmartir > 1){extronSerial2.write(tesmart11,6);sendSVS(111);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 39){ // profile button 12
        if(TESmartir < 2)sendSVS(auxprof[11]);
        if(TESmartir > 1){extronSerial2.write(tesmart12,6);sendSVS(112);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 56){ // aux1 button
        if(TESmartir > 1){extronSerial2.write(tesmart13,6);sendSVS(113);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 57){ // aux2 button
        if(TESmartir > 1){extronSerial2.write(tesmart14,6);sendSVS(114);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 58){ // aux3 button
        if(TESmartir > 1){extronSerial2.write(tesmart15,6);sendSVS(115);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 59){ // aux4 button
        if(TESmartir > 1){extronSerial2.write(tesmart16,6);sendSVS(116);};
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 26){ // Power button
        sendIR(auxpower,0,0);
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


    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && auxTESmart[0] == 1){ // if AUX7 was pressed and a profile button is pressed next
      if(ir_recv_command == 11){ // profile button 1
        extronSerial.write(tesmart1,6);sendSVS(1);                                                                    
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        extronSerial.write(tesmart2,6);sendSVS(2); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
        extronSerial.write(tesmart3,6);sendSVS(3); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        extronSerial.write(tesmart4,6);sendSVS(4); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        extronSerial.write(tesmart5,6);sendSVS(5); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        extronSerial.write(tesmart6,6);sendSVS(6); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        extronSerial.write(tesmart7,6);sendSVS(7); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        extronSerial.write(tesmart8,6);sendSVS(8); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 1){ // profile button 9
        extronSerial.write(tesmart9,6);sendSVS(9); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 37){ // profile button 10
        extronSerial.write(tesmart10,6);sendSVS(10); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 38){ // profile button 11
        extronSerial.write(tesmart11,6);sendSVS(11); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 39){ // profile button 12
        extronSerial.write(tesmart12,6);sendSVS(12); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 56){ // aux1 button
        extronSerial.write(tesmart13,6);sendSVS(13); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 57){ // aux2 button
        extronSerial.write(tesmart14,6);sendSVS(14); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 58){ // aux3 button
        extronSerial.write(tesmart15,6);sendSVS(15); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 59){ // aux4 button
        extronSerial.write(tesmart16,6);sendSVS(16);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }

      auxTESmart[0] = 0;
    }

    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && gctl && (auxgsw[0] || auxgsw[1])){ // if AUX5 or AUX6 was pressed and a profile button is pressed next,
      if(ir_recv_command == 11){ // profile button 1
        if(auxgsw[0])overrideGscart(1);
        else if(auxgsw[1])overrideGscart(9);
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        if(auxgsw[0])overrideGscart(2);
        else if(auxgsw[1])overrideGscart(10);
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
        if(auxgsw[0])overrideGscart(3);
        else if(auxgsw[1])overrideGscart(11);
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        if(auxgsw[0])overrideGscart(4);
        else if(auxgsw[1])overrideGscart(12);
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        if(auxgsw[0])overrideGscart(5);
        else if(auxgsw[1])overrideGscart(13);
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        if(auxgsw[0])overrideGscart(6);
        else if(auxgsw[1])overrideGscart(14);
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        if(auxgsw[0])overrideGscart(7);
        else if(auxgsw[1])overrideGscart(15);
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        if(auxgsw[0])overrideGscart(8);
        else if(auxgsw[1])overrideGscart(16);
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 1 || ir_recv_command == 37 || ir_recv_command == 38 || ir_recv_command == 39){ // profile button 9,10,11,12 will turn off manual mode, back to auto mode
        if(auxgsw[0]){
          pinMode(12,INPUT); // D12 back to floating, enables auto-switching
          pinMode(A2,INPUT);pinMode(A1,INPUT);pinMode(A0,INPUT); // set A0-A2 as inputs
          lastginput = 1;
          auxgsw[0] = 0;
        }
        else if(auxgsw[1]){
          pinMode(10,INPUT); // D10 back to floating, enables auto-switching
          pinMode(A5,INPUT);pinMode(A4,INPUT);pinMode(A3,INPUT); // set A3-A5 as inputs
          lastginput = 9;
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
      sendSVS(svsbutton.toInt());
      nument = 0;
      svsbutton = "";
      extrabuttonprof = 0;
      ir_recv_command = 0;
    }
    
    if(TinyIRReceiverData.Flags == IRDATA_FLAGS_IS_REPEAT){repeatcount++;} // directional buttons have to be held down for just a bit before repeating

    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT){ // block most buttons from being repeated when held
      repeatcount = 0;
      if(ir_recv_command == 63){
        //Serial.println(F("\rremote aux8\r")); aux8
        if(extrabuttonprof < 3)extrabuttonprof++;
        else Serial.println(F("\rremote aux8\r"));
      }
      else if(ir_recv_command == 62){
        if(TESmartir == 1 || TESmartir == 3)auxTESmart[0] = 1;
        else Serial.println(F("\rremote aux7\r"));
      }
      else if(ir_recv_command == 61){
        if(gctl)auxgsw[1] = 1;
        else Serial.println(F("\rremote aux6\r"));
      }
      else if(ir_recv_command == 60){
        if(gctl)auxgsw[0] = 1;
        else Serial.println(F("\rremote aux5\r"));
      }
      else if(ir_recv_command == 59){
        Serial.println(F("\rremote aux4\r"));
      }
      else if(ir_recv_command == 58){
        Serial.println(F("\rremote aux3\r"));
      }
      else if(ir_recv_command == 57){
        Serial.println(F("\rremote aux2\r"));
      }
      else if(ir_recv_command == 56){
        Serial.println(F("\rremote aux1\r"));
      }
      else if(ir_recv_command == 52){
        Serial.println(F("\rremote res1\r"));
      }
      else if(ir_recv_command == 53){
        Serial.println(F("\rremote res2\r"));
      }
      else if(ir_recv_command == 54){
        Serial.println(F("\rremote res3\r"));
      }
      else if(ir_recv_command == 55){
        Serial.println(F("\rremote res4\r"));
      }
      else if(ir_recv_command == 51){
        Serial.println(F("\rremote res480p\r"));
      }
      else if(ir_recv_command == 50){
        Serial.println(F("\rremote res1440p\r"));
      }
      else if(ir_recv_command == 49){
        Serial.println(F("\rremote res1080p\r"));
      }
      else if(ir_recv_command == 48){
        Serial.println(F("\rremote res4k\r"));
      }
      else if(ir_recv_command == 47){
        Serial.println(F("\rremote buffer\r"));
      }
      else if(ir_recv_command == 44){
        Serial.println(F("\rremote genlock\r"));
      }
      else if(ir_recv_command == 46){
        Serial.println(F("\rremote safe\r"));
      }
      else if(ir_recv_command == 86){
        Serial.println(F("\rremote pause\r"));
      }
      else if(ir_recv_command == 45){
        Serial.println(F("\rremote phase\r"));
      }
      else if(ir_recv_command == 43){
        Serial.println(F("\rremote gain\r"));
      }
      else if(ir_recv_command == 36){
        Serial.println(F("\rremote prof\r"));
      }
      else if(ir_recv_command == 11){
        sendRBP(1);
        if(RT5Xir >= 1){sendIR("5x",1,1);delay(30);sendIR("5x",1,1);} // RT5X profile 1
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1){sendIR("ossc",1,3);} // OSSC profile 1
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 7){
        sendRBP(2);
        if(RT5Xir >= 1){sendIR("5x",2,1);delay(30);sendIR("5x",2,1);} // RT5X profile 2
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1){sendIR("ossc",2,3);} // OSSC profile 2
      }
      else if(ir_recv_command == 3){
        sendRBP(3);
        if(RT5Xir >= 1){sendIR("5x",3,1);delay(30);sendIR("5x",3,1);} // RT5X profile 3
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1){sendIR("ossc",3,3);} // OSSC profile 3
      }
      else if(ir_recv_command == 10){
        sendRBP(4);
        if(RT5Xir >= 1){sendIR("5x",4,1);delay(30);sendIR("5x",4,1);} // RT5X profile 4
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1){sendIR("ossc",4,3);} // OSSC profile 4
      }
      else if(ir_recv_command == 6){
        sendRBP(5);
        if(RT5Xir >= 1){sendIR("5x",5,1);delay(30);sendIR("5x",5,1);} // RT5X profile 5
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1){sendIR("ossc",5,3);} // OSSC profile 5
      }
      else if(ir_recv_command == 2){
        sendRBP(6);
        if(RT5Xir >= 1){sendIR("5x",6,1);delay(30);sendIR("5x",6,1);} // RT5X profile 6
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1){sendIR("ossc",6,3);} // OSSC profile 6
      }
      else if(ir_recv_command == 9){
        sendRBP(7);
        if(RT5Xir >= 1){sendIR("5x",7,1);delay(30);sendIR("5x",7,1);} // RT5X profile 7
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1){sendIR("ossc",7,3);} // OSSC profile 7
      }
      else if(ir_recv_command == 5){
        sendRBP(8);
        if(RT5Xir >= 1){sendIR("5x",8,1);delay(30);sendIR("5x",8,1);} // RT5X profile 8
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1){sendIR("ossc",8,3);} // OSSC profile 8
      }
      else if(ir_recv_command == 1){
        sendRBP(9);
        if(RT5Xir >= 1){sendIR("5x",9,1);delay(30);sendIR("5x",9,1);} // RT5X profile 9
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1){sendIR("ossc",9,3);} // OSSC profile 9
      }
      else if(ir_recv_command == 37){
        sendRBP(10);
        if(RT5Xir >= 1){sendIR("5x",10,1);delay(30);sendIR("5x",10,1);} // RT5X profile 10
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1){sendIR("ossc",10,3);} // OSSC profile 10
      }
      else if(ir_recv_command == 38){
        if(OSSCir == 1){sendIR("ossc",11,3);} // OSSC profile 11
        sendRBP(11);
      }
      else if(ir_recv_command == 39){
        if(OSSCir == 1){sendIR("ossc",12,3);} // OSSC profile 12
        sendRBP(12);
      }
      else if(ir_recv_command == 35){
        Serial.println(F("\rremote adc\r"));
      }
      else if(ir_recv_command == 34){
        Serial.println(F("\rremote sfx\r"));
      }
      else if(ir_recv_command == 33){
        Serial.println(F("\rremote scaler\r"));
      }
      else if(ir_recv_command == 32){
        Serial.println(F("\rremote output\r"));
      }
      else if(ir_recv_command == 17){
        Serial.println(F("\rremote input\r"));
      }
      else if(ir_recv_command == 41){
        Serial.println(F("\rremote stat\r"));
      }
      else if(ir_recv_command == 40){
        Serial.println(F("\rremote diag\r"));
      }
      else if(ir_recv_command == 66){
        Serial.println(F("\rremote back\r"));
      }
      else if(ir_recv_command == 83){
        Serial.println(F("\rremote ok\r"));
      }
      else if(ir_recv_command == 79){
        Serial.println(F("\rremote right\r"));
      }
      else if(ir_recv_command == 16){
        Serial.println(F("\rremote down\r"));
      }
      else if(ir_recv_command == 87){
        Serial.println(F("\rremote left\r"));
      }
      else if(ir_recv_command == 24){
        Serial.println(F("\rremote up\r"));
      }
      else if(ir_recv_command == 92){
        Serial.println(F("\rremote menu\r"));
      }
      else if(ir_recv_command == 26){
        Serial.println(F("\rpwr on\r")); // wake
        Serial.println(F("\rremote pwr\r")); // sleep
        RTwake = true;
      }
    }
    else if(ir_recv_address == 73 && repeatcount > 4){ // directional buttons have to be held down for just a bit before repeating
      if(ir_recv_command == 24){
        Serial.println(F("\rremote up\r"));
      }
      else if(ir_recv_command == 16){
        Serial.println(F("\rremote down\r"));
      }
      else if(ir_recv_command == 87){
        Serial.println(F("\rremote left\r"));
      }
      else if(ir_recv_command == 79){
        Serial.println(F("\rremote right\r"));
      }
    } // end of if(ir_recv_address
    
    if(ir_recv_address == 73 && repeatcount > 15){ // when directional buttons are held down for even longer... turbo directional mode
      if(ir_recv_command == 87){
        for(uint8_t i=0;i<4;i++){
          Serial.println(F("\rremote left\r"));
        }
      }
      else if(ir_recv_command == 79){
        for(uint8_t i=0;i<4;i++){
          Serial.println(F("\rremote right\r"));
        }
      }
    } // end of turbo directional mode

    if(ir_recv_address == 128 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && gctl){ // legacy 5x remote
      if(ir_recv_command == 137 && gctl){overrideGscart(legacyOR[0]);}      // vol- / scanlines button
      else if(ir_recv_command == 72 && gctl){overrideGscart(legacyOR[1]);}  // mouse / h. sampling button
      else if(ir_recv_command == 135 && gctl){overrideGscart(legacyOR[2]);} // vol+ / interpolation button
      else if(ir_recv_command == 39 && gctl){
          pinMode(12,INPUT); // D12 back to floating, enables auto-switching
          pinMode(A2,INPUT);pinMode(A1,INPUT);pinMode(A0,INPUT); // set A0-A2 as inputs
          lastginput = 1;
      }  // back button
      else if(ir_recv_command == 64){} // down button
      else if(ir_recv_command == 56){} // up button
      else if(ir_recv_command == 55){} // left button
      else if(ir_recv_command == 57){} // right button
      else if(ir_recv_command == 19){} // ok button
      else if(ir_recv_command == 131){} // menu / output res button
      else if(ir_recv_command == 115){} // home / input button
      else if(ir_recv_command == 129){} // power button

    } // end of legacy 5x remote

    if(ir_recv_address == 124 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT){ // OSSC remote, nothing defined atm. (124 dec is 7C hex)
      if(ir_recv_command == 148){} // 1 button
      else if(ir_recv_command == 149){} // 2 button
      else if(ir_recv_command == 150){} // 3 button
      else if(ir_recv_command == 151){} // 4 button
      else if(ir_recv_command == 152){} // 5 button
      else if(ir_recv_command == 153){} // 6 button
      else if(ir_recv_command == 154){} // 7 button
      else if(ir_recv_command == 155){} // 8 button
      else if(ir_recv_command == 156){} // 9 button
      else if(ir_recv_command == 147){} // 0 button
      else if(ir_recv_command == 157){} // 10+ --/- button
      else if(ir_recv_command == 158){} // return <__S--> button
      else if(ir_recv_command == 180){} // up button
      else if(ir_recv_command == 179){} // down button
      else if(ir_recv_command == 181){} // left button
      else if(ir_recv_command == 182){} // right button
      else if(ir_recv_command == 184){} // ok button
      else if(ir_recv_command == 178){} // menu button
      else if(ir_recv_command == 173){} // L/R button
      else if(ir_recv_command == 139){} // cancel / PIC & clock & zoom button
      else if(ir_recv_command == 183){} // exit button
      else if(ir_recv_command == 166){} // info button
      else if(ir_recv_command == 131){} // red pause button
      else if(ir_recv_command == 130){} // green play button
      else if(ir_recv_command == 133){} // yellow stop button
      else if(ir_recv_command == 177){} // blue eject button
      else if(ir_recv_command == 186){} // >>| next button
      else if(ir_recv_command == 185){} // |<< previous button
      else if(ir_recv_command == 134){} // << button
      else if(ir_recv_command == 135){} // >> button
      else if(ir_recv_command == 128){} // Power button
    }
    else if(ir_recv_address == 122 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT){ // 122 is 7A hex
      if(ir_recv_command == 27){} // tone- button
      else if(ir_recv_command == 26){} // tone+ button
    }
    else if(ir_recv_address == 56 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT){ // 56 is 38 hex
      if(ir_recv_command == 14){} // vol+ button
      else if(ir_recv_command == 15){} // vol- button
      else if(ir_recv_command == 10){} // ch+ button
      else if(ir_recv_command == 11){} // ch- button
      else if(ir_recv_command == 18){} // p.n.s. button
      else if(ir_recv_command == 19){} // tv/av button
      else if(ir_recv_command == 24){} // mute speaker button
      //else if(ir_recv_command == ){} // TV button , I dont believe these are NEC based
      //else if(ir_recv_command == ){} // SAT / DVB button ... ^^^
      //else if(ir_recv_command == ){} // DVD / HIFI button ... ^^^
      
    } // end of OSSC remote
    
  } // end of TinyReceiverDecode()   
} // end of readIR()

void overrideGscart(uint8_t port){ // disable auto switching and allows gscart port select
  if(port <= 8){
    auxgsw[0] = 0;
    pinMode(12,OUTPUT);
    digitalWrite(12,LOW); // D12 / gscart sw1 override set LOW to select port (disables auto switching)
    pinMode(A2,OUTPUT);pinMode(A1,OUTPUT);pinMode(A0,OUTPUT);// set A0-A2 as outputs
    if(port == 1){
      digitalWrite(A2,LOW);digitalWrite(A1,LOW);digitalWrite(A0,LOW); // 000
    }
    else if(port == 2){
      digitalWrite(A2,LOW);digitalWrite(A1,LOW);digitalWrite(A0,HIGH); // 001
    }
    else if(port == 3){
      digitalWrite(A2,LOW);digitalWrite(A1,HIGH);digitalWrite(A0,LOW); // 010
    }
    else if(port == 4){
      digitalWrite(A2,LOW);digitalWrite(A1,HIGH);digitalWrite(A0,HIGH); // 011
    }
    else if(port == 5){
      digitalWrite(A2,HIGH);digitalWrite(A1,LOW);digitalWrite(A0,LOW); // 100
    }
    else if(port == 6){
      digitalWrite(A2,HIGH);digitalWrite(A1,LOW);digitalWrite(A0,HIGH); // 101
    }
    else if(port == 7){
      digitalWrite(A2,HIGH);digitalWrite(A1,HIGH);digitalWrite(A0,LOW); // 110
    }
    else if(port == 8){
      digitalWrite(A2,HIGH);digitalWrite(A1,HIGH);digitalWrite(A0,HIGH); // 111
    }
    if(lastginput == port || lastginput == 9){
      if(RT5Xir == 2){sendIR("5x",port,1);delay(30);sendIR("5x",port,1);} // RT5X profile 1 - 8 (based on port#)
      if(RT5Xir && OSSCir)delay(500);
      if(OSSCir == 2)sendIR("ossc",port,3); // OSSC profile 1 - 8 (based on port#)
      if(SVS==2)sendRBP(port);
      else sendSVS(200 + port);
    }
    lastginput = port;
  }
  else if(port >= 9){
    auxgsw[1] = 0;
    pinMode(10,OUTPUT);
    digitalWrite(10,LOW); // D10 / gscart sw2 override set LOW to select port (disables auto switching)
    pinMode(A5,OUTPUT);pinMode(A4,OUTPUT);pinMode(A3,OUTPUT); // set A3-A5 as outputs
    if(port == 9){
      digitalWrite(A5,LOW);digitalWrite(A4,LOW);digitalWrite(A3,LOW); // 000
      if(lastginput == port || lastginput == 1){
        if(RT5Xir == 2){sendIR("5x",port,1);delay(30);sendIR("5x",port,1);} // RT5X profile 9
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 2)sendIR("ossc",port,3); // OSSC profile 9
        if(SVS==2)sendRBP(port);
        else sendSVS(200 + port);
      }
      lastginput = port;
    }
    else if(port == 10){
      digitalWrite(A5,LOW);digitalWrite(A4,LOW);digitalWrite(A3,HIGH); // 001
      if(lastginput == port){
        if(RT5Xir == 2){sendIR("5x",port,1);delay(30);sendIR("5x",port,1);} // RT5X profile 10
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 2)sendIR("ossc",port,3); // OSSC profile 10
        if(SVS==2)sendRBP(port);
        else sendSVS(200 + port);
      }
      lastginput = port;
    }
    else if(port == 11){
      digitalWrite(A5,LOW);digitalWrite(A4,HIGH);digitalWrite(A3,LOW); // 010
      if(lastginput == port){
        if(OSSCir == 2)sendIR("ossc",port,3); // OSSC profile 11
        if(SVS==2)sendRBP(port);
        else sendSVS(200 + port);
      }
      lastginput = port;
    }
    else if(port == 12){
      digitalWrite(A5,LOW);digitalWrite(A4,HIGH);digitalWrite(A3,HIGH); // 011
      if(lastginput == port){
        if(OSSCir == 2)sendIR("ossc",port,3); // OSSC profile 12
        if(SVS==2 && !S0)sendRBP(port);
        else sendSVS(200 + port);
      }
      lastginput = port;
    }
    else if(port == 13){
      digitalWrite(A5,HIGH);digitalWrite(A4,LOW);digitalWrite(A3,LOW); // 100
      if(OSSCir == 2)sendIR("ossc",port,3); // OSSC profile 13
      if(lastginput == port)sendSVS(200 + port);
      lastginput = port;
    }
    else if(port == 14){
      digitalWrite(A5,HIGH);digitalWrite(A4,LOW);digitalWrite(A3,HIGH); // 101
      if(OSSCir == 2)sendIR("ossc",port,3); // OSSC profile 14
      if(lastginput == port)sendSVS(200 + port);
      lastginput = port;
    }
    else if(port == 15){
      digitalWrite(A5,HIGH);digitalWrite(A4,HIGH);digitalWrite(A3,LOW); // 110
      if(lastginput == port)sendSVS(200 + port);
      lastginput = port;
    }
    else if(port == 16){
      digitalWrite(A5,HIGH);digitalWrite(A4,HIGH);digitalWrite(A3,HIGH); // 111
      if(lastginput == port)sendSVS(200 + port);
      lastginput = port;
    }
  }
} // end of overrideGscart()

void sendSVS(uint16_t num){
  Serial.print(F("\rSVS NEW INPUT="));
  Serial.print(num + offset);
  Serial.println(F("\r"));
  delay(1000);
  Serial.print(F("\rSVS CURRENT INPUT="));
  Serial.print(num + offset);
  Serial.println(F("\r"));
  currentProf[0] = 1; // 1 is SVS profile
  currentProf[1] = num + offset;
}

void sendRBP(uint16_t prof){ // send Remote Button Profile
  Serial.print(F("\rremote prof"));
  Serial.print(prof);
  Serial.println(F("\r"));
  currentProf[0] = 0; // 0 is Remote Button Profile
  currentProf[1] = prof;
}

void sendIR(String type, uint8_t prof, uint8_t repeat){
  IRsend irsend;
  if(type == "5x" || type == "5X"){
    if(prof == 1){irsend.sendNEC(0xB3,0x92,repeat);} // RT5X profile 1 
    else if(prof == 2){irsend.sendNEC(0xB3,0x93,repeat);} // RT5X profile 2
    else if(prof == 3){irsend.sendNEC(0xB3,0xCC,repeat);} // RT5X profile 3
    else if(prof == 4){irsend.sendNEC(0xB3,0x8E,repeat);} // RT5X profile 4
    else if(prof == 5){irsend.sendNEC(0xB3,0x8F,repeat);} // RT5X profile 5
    else if(prof == 6){irsend.sendNEC(0xB3,0xC8,repeat);} // RT5X profile 6
    else if(prof == 7){irsend.sendNEC(0xB3,0x8A,repeat);} // RT5X profile 7
    else if(prof == 8){irsend.sendNEC(0xB3,0x8B,repeat);} // RT5X profile 8
    else if(prof == 9){irsend.sendNEC(0xB3,0xC4,repeat);} // RT5X profile 9
    else if(prof == 10){irsend.sendNEC(0xB3,0x87,repeat);} // RT5X profile 10
  }
  else if(type == "4k" || type == "4K"){
    if(prof == 1){irsend.sendNEC(0x49,0x0B,repeat);} // RT4K profile 1 
    else if(prof == 2){irsend.sendNEC(0x49,0x07,repeat);} // RT4K profile 2
    else if(prof == 3){irsend.sendNEC(0x49,0x03,repeat);} // RT4K profile 3
    else if(prof == 4){irsend.sendNEC(0x49,0x0A,repeat);} // RT4K profile 4
    else if(prof == 5){irsend.sendNEC(0x49,0x06,repeat);} // RT4K profile 5
    else if(prof == 6){irsend.sendNEC(0x49,0x02,repeat);} // RT4K profile 6
    else if(prof == 7){irsend.sendNEC(0x49,0x09,repeat);} // RT4K profile 7
    else if(prof == 8){irsend.sendNEC(0x49,0x05,repeat);} // RT4K profile 8
    else if(prof == 9){irsend.sendNEC(0x49,0x01,repeat);} // RT4K profile 9
    else if(prof == 10){irsend.sendNEC(0x49,0x25,repeat);} // RT4K profile 10
    else if(prof == 11){irsend.sendNEC(0x49,0x26,repeat);} // RT4K profile 11
    else if(prof == 12){irsend.sendNEC(0x49,0x27,repeat);} // RT4K profile 12
  }
  else if(type == "OSSC" || type == "ossc"){
    irsend.sendNEC(0x7C,0xB7,repeat); // exit
    delay(100);
    irsend.sendNEC(0x7C,0xB7,repeat); // exit 
    delay(100);
    irsend.sendNEC(0x7C,0x9D,repeat); // 10+ button
    delay(400);
    //if(prof == 0){irsend.sendNEC(0x7C,0x93,repeat);} // OSSC profile 0 not used atm
    if(prof == 1){irsend.sendNEC(0x7C,0x94,repeat);} // OSSC profile 1 
    else if(prof == 2){irsend.sendNEC(0x7C,0x95,repeat);} // OSSC profile 2
    else if(prof == 3){irsend.sendNEC(0x7C,0x96,repeat);} // OSSC profile 3
    else if(prof == 4){irsend.sendNEC(0x7C,0x97,repeat);} // OSSC profile 4
    else if(prof == 5){irsend.sendNEC(0x7C,0x98,repeat);} // OSSC profile 5
    else if(prof == 6){irsend.sendNEC(0x7C,0x99,repeat);} // OSSC profile 6
    else if(prof == 7){irsend.sendNEC(0x7C,0x9A,repeat);} // OSSC profile 7
    else if(prof == 8){irsend.sendNEC(0x7C,0x9B,repeat);} // OSSC profile 8
    else if(prof == 9){irsend.sendNEC(0x7C,0x9C,repeat);} // OSSC profile 9
    else if(prof == 10){irsend.sendNEC(0x7C,0x9D,repeat);delay(400);irsend.sendNEC(0x7C,0x93,repeat);} // OSSC profile 10
    else if(prof == 11){irsend.sendNEC(0x7C,0x9D,repeat);delay(400);irsend.sendNEC(0x7C,0x94,repeat);} // OSSC profile 11
    else if(prof == 12){irsend.sendNEC(0x7C,0x9D,repeat);delay(400);irsend.sendNEC(0x7C,0x95,repeat);} // OSSC profile 12
    else if(prof == 13){irsend.sendNEC(0x7C,0x9D,repeat);delay(400);irsend.sendNEC(0x7C,0x96,repeat);} // OSSC profile 13
    else if(prof == 14){irsend.sendNEC(0x7C,0x9D,repeat);delay(400);irsend.sendNEC(0x47C,0x97,repeat);} // OSSC profile 14
  }
  else if(type == "LG"){           // LG CX OLED
      irsend.sendNEC(0x04,0x08,0); // Power button
      irsend.sendNEC(0x00,0x00,0);
      irsend.sendNEC(0x00,0x00,0);
      irsend.sendNEC(0x00,0x00,0);
      delay(30);
      irsend.sendNEC(0x04,0x08,0); // send once more
      irsend.sendNEC(0x00,0x00,0);
      irsend.sendNEC(0x00,0x00,0);
      irsend.sendNEC(0x00,0x00,0);
  }
  
} // end of sendIR()

void sendRTwake(uint16_t mil){
    currentTime = millis();
    if(prevTime == 0)
      prevTime = millis();
    if((currentTime - prevTime) >= mil){
      prevTime = 0;
      prevBlinkTime = 0;
      currentTime = 0;
      RTwake = false;
      digitalWrite(LED_BUILTIN,LOW);
      if((currentProf[0] == 1 && currentProf[1] != 0) || (currentProf[0] == 0 && currentProf[1] != 12)){
        if(currentProf[0])
          sendSVS(currentProf[1]);
        else
          sendRBP(currentProf[1]);
      }
    }
    if(currentTime - prevBlinkTime >= 300){
      prevBlinkTime = currentTime;
      if(digitalRead(LED_BUILTIN) == LOW)digitalWrite(LED_BUILTIN,HIGH);
      else digitalWrite(LED_BUILTIN,LOW);
    }
} // end of sendRTwake()
