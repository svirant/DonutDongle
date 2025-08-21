/*
* Donut Dongle gameID v0.1a (Arduino Nano ESP32 only)
* Copyright(C) 2025 @Donutswdad
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation,either version 3 of the License,or
*(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not,see <http://www.gnu.org/licenses/>.
*/

#define SEND_LEDC_CHANNEL 0
#define IR_SEND_PIN 11    // Optional IR LED Emitter for RT5X compatibility. Sends IR data out Arduino pin D11
#define IR_RECEIVE_PIN 2  // Optional IR Receiver on pin D2
#define extronSerial Serial1
#define extronSerial2 Serial2
#define Serial Serial0 // ** COMMENT OUT THIS LINE ** to see output in Serial Monitor. Disables Serial output to RT4K.


#include <TinyIRReceiver.hpp>
#include <IRremote.hpp>       // found in the built-in Library Manager
#include <Arduino_JSON.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <EspUsbHostSerial_FTDI.h> // https://github.com/wakwak-koba/EspUsbHost in order to have FTDI support for the RT4K usb serial port, this is the easist method.
                                   // Step 1 - Goto the github link above. Click the GREEN "<> Code" box and "Download ZIP"
                                   // Step 2 - In Arudino IDE; goto "Sketch" -> "Include Library" -> "Add .ZIP Library"

struct Console {
  String Address;
  int DefaultProf;
  int Prof;
  int On;
  int King;
};

/*
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//    CONFIG     //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
*/
                   // format as so: {console address, Default Profile for console, current profile state (leave 0), power state (leave 0), active state (leave 0)}
                   //
                   // If using a "remote button profile" for the "Default Profile" which are valued 1 - 12, place a "-" before the profile number. 
                   // Example: -1 means "remote button profile 1"
                   //          -12 means "remote button profile 12"
                   //            0 means SVS profile 0
                   //            1 means SVS profile 1
                   //           12 means SVS profile 12
                   //           etc...
Console consoles[] = {{"http://ps1digital.local/gameid",-9,0,0,0}, // you can add more, but stay in this format
                      {"http://10.0.1.53/api/currentState",-10,0,0,0},
                   // {"http://ps2digital.local/gameid",102,0,0,0}, // remove leading "//" to uncomment and enable ps2digital
                   // {"http://10.0.0.14/api/currentState",104,0,0,0}, // address format for MemCardPro. replace IP address with your MCP address
                      {"http://n64digital.local/gameid",-7,0,0,0} // the last one in the list has no "," at the end
                      };


                   // If using a "remote button profile" for the "PROFILE" which are valued 1 - 12, place a "-" before the profile number. 
                   // Example: -1 means "remote button profile 1"
                   //          -12 means "remote button profile 12"
                   //            0 means SVS profile 0
                   //            1 means SVS profile 1
                   //           12 means SVS profile 12
                   //           etc...
                   //                      
                                 // {"<GAMEID>","PROFILE #"},
String gameDB[][2] = {{"00000000-00000000---00","7"}, // 7 is the "SVS PROFILE", would translate to a S7_<USER_DEFINED>.rt4 named profile under RT4K-SDcard/profile/SVS/
                      {"XSTATION","8"},               // XSTATION is the <GAMEID>
                      {"GM4E0100","505"},             // GameCube
                      {"3E5055B6-2E92DA52-N-45","501"}, // N64 MarioKart 64
                      {"635A2BFF-8B022326-N-45","502"}, // N64 Mario 64
                      {"DCBC50D1-09FD1AA3-N-45","503"}, // N64 Goldeneye 007
                      {"492F4B61-04E5146A-N-45","504"}, // N64 Wave Race 64
                      {"SLUS-00214","10"}, // PS1 Ridge Racer Revolution
                      {"SCUS-94300","9"}}; // PS1 Ridge Racer

// WiFi config is just below (line ~527)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
////////////////////
//    OPTIONS    //
//////////////////
*/

uint8_t const debugE1CAP = 0; // line ~766
uint8_t const debugE2CAP = 0; // line ~1252

bool const S0_pwr = false;       // Load "S0_pwr_profile" when all consoles defined below are off. Defined below.

int const S0_pwr_profile = 0;    // When all consoles definied below are off, load this profile. set to 0 means that S0_<whatever>.rt4 profile will load.
                                 // "S0_pwr" must be set true
                                 //
                                 // If using a "remote button profile" which are valued 1 - 12, place a "-" before the profile number. 
                                 // Example: -1 means "remote button profile 1"
                                 //          -12 means "remote button profile 12"

bool const S0_gameID = false;    // When a gameID match is not found for a powered on console, DefaultProf for that console will load


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
                      //
                      // 1 - use only "SVS" profiles.
                      //     Make sure "Auto Load SVS" is "On" under the RT4K Profiles menu
                      //     RT4K checks the /profile/SVS subfolder for profiles and need to be named: "S<input number>_<user defined>.rt4"
                      //     For example, SVS input 2 would look for a profile that is named S2_SNES.rt4
                      //     If there’s more than one profile that fits the pattern, the first match is used
                      //
                      //     - SVS   1 -  99 for 1st Extron or TESmart
                      //     - SVS 101 - 199 for 2nd Extron or TESmart
                      //     - SVS   0       for S0 option mentioned below
                      //
                      //  ** If S0 below is set to true, create "/profile/SVS/S0_<user defined>.rt4" for when all ports are in-active. Ex: S0_HDMI.rt4
                      //


bool const S0  = false;  // (Profile 0) default is false 
                         //
                         //  ** Recommended to remove any /profile/SVS/S0_<user defined>.rt4 profiles and leave this option "false" if using in tandem with the Scalable Video Switch. **
                         //  ** Does not work with MT-VIKI / TESmart HDMI switches **
                         //
                         // set "true" to load "Remote Profile 12" instead of "S0_<user definted>.rt4" (if SVS=0) when all ports are in-active on 1st Extron switch (and 2nd if connected). 
                         // You can assign it to a generic HDMI profile for example.
                         // If your device has a 12th input, SVS will be used instead. "If" you also have an active 2nd Extron Switch, Remote Profile 12
                         // will only load if "BOTH" switches have all in-active ports.
                         // 
                         // 
                         // If SVS=1, /profile/SVS/ "S0_<user defined>.rt4" will be used instead of Remote Profile 12
                         //


////////////////////////// 
                        // Choosing the above two options can get quite confusing (even for myself) so maybe this will help a little more:
                        //
                        // when S0=0 and SVS=0, button profiles 1 - 12 are used for EXTRON sw1, and SVS for EVERYTHING else
                        // when S0=0 and SVS=1, SVS profiles are used for everything
                        // when S0=1 and SVS=0, button profiles 1 - 11 are used for EXTRON sw1 and Remote Profile 12 as "Profile S0", and SVS for everything else 
                        // when S0=1 and SVS=1, SVS profiles for everything, and uses S0_<user defined>.rt4 as "Profile 0" 
                        //
//////////////////////////

// For Extron Matrix switches that support DSVP. RGBS and HDMI/DVI video types.
bool const automatrixSW1 = false; // set true for auto matrix switching on "SW1" port
bool const automatrixSW2 = false; // set true for auto matrix switching on "SW2" port

uint8_t const amSizeSW1 = 8; // number of input ports for auto matrix switching on SW1. Ex: 8,12,16,32
uint8_t const amSizeSW2 = 8; // number of input ports for auto matrix switching on SW2. ...


uint8_t const vinMatrix[65] = {0,  // MATRIX switchers  // When auto matrix mode is enabled: (automatrixSW1 / SW2 above)
                                                        // set to 1 for the auto switched input to trigger a Preset on SW1
                                                        // set to 2 for the auto switched input to trigger a Preset on SW2
                                                        // set to 3 for both SW1 & SW2
                                                        // set to 0 to disable this feature (default - aka input goes to all outputs unless defined in voutMatrix)
                                                        //
                                                        // set the following inputs to the desired Preset #
                                                        // (by default each input # is set to the same corresponding Preset #)
                           1,  // input 1 SW1
                           2,  // input 2
                           3,  // input 3
                           4,  // input 4
                           5,  // input 5
                           6,  // input 6
                           7,  // input 7
                           8,  // input 8
                           9,  // input 9
                           10,  // input 10
                           11,  // input 11
                           12,  // input 12
                           13,  // input 13
                           14,  // input 14
                           15,  // input 15
                           16,  // input 16
                           17,  // input 17
                           18,  // input 18
                           19,  // input 19
                           20,  // input 20
                           21,  // input 21
                           22,  // input 22
                           23,  // input 23
                           24,  // input 24
                           25,  // input 25
                           26,  // input 26
                           27,  // input 27
                           28,  // input 28
                           29,  // input 29
                           30,  // input 30
                           30,  // input 31
                           30,  // input 32
                               //
                               // ONLY USE FOR 2ND MATRIX SWITCH on SW2
                           1,  // 2ND MATRIX SWITCH input 1 SW2
                           2,  // 2ND MATRIX SWITCH input 2
                           3,  // 2ND MATRIX SWITCH input 3
                           4,  // 2ND MATRIX SWITCH input 4
                           5,  // 2ND MATRIX SWITCH input 5
                           6,  // 2ND MATRIX SWITCH input 6
                           7,  // 2ND MATRIX SWITCH input 7
                           8,  // 2ND MATRIX SWITCH input 8
                           9,  // 2ND MATRIX SWITCH input 9
                           10,  // 2ND MATRIX SWITCH input 10
                           11,  // 2ND MATRIX SWITCH input 11
                           12,  // 2ND MATRIX SWITCH input 12
                           13,  // 2ND MATRIX SWITCH input 13
                           14,  // 2ND MATRIX SWITCH input 14
                           15,  // 2ND MATRIX SWITCH input 15
                           16,  // 2ND MATRIX SWITCH input 16
                           17,  // 2ND MATRIX SWITCH input 17
                           18,  // 2ND MATRIX SWITCH input 18
                           19,  // 2ND MATRIX SWITCH input 19
                           20,  // 2ND MATRIX SWITCH input 20
                           21,  // 2ND MATRIX SWITCH input 21
                           22,  // 2ND MATRIX SWITCH input 22
                           23,  // 2ND MATRIX SWITCH input 23
                           24,  // 2ND MATRIX SWITCH input 24
                           25,  // 2ND MATRIX SWITCH input 25
                           26,  // 2ND MATRIX SWITCH input 26
                           27,  // 2ND MATRIX SWITCH input 27
                           28,  // 2ND MATRIX SWITCH input 28
                           29,  // 2ND MATRIX SWITCH input 29
                           30,  // 2ND MATRIX SWITCH input 30
                           30,  // 2ND MATRIX SWITCH input 31
                           30,  // 2ND MATRIX SWITCH input 32
                           };

uint8_t const voutMatrix[66] = {1,  // MATRIX switchers // When auto matrix mode is enabled: (automatrixSW1 / SW2 above)
                                                        // set to 1 for the auto switched input to go to ALL outputs (default)
                                                        // set to 0 to select outputs to be enabled/disabled as listed below
                                                        //
                                                        // When auto matrix mode is disabled: (automatrixSW1 / SW2 above)
                                                        // ALL input changes to any/all outputs result in a profile change
                                                        // set to 0 to select outputs from triggering profile changes
                           1,  // output 1 SW1 (1 = enabled, 0 = disabled)
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
                               // ONLY USE FOR 2ND MATRIX SWITCH on SW2
                           1,  // 2ND MATRIX SWITCH output 1 SW2 (1 = enabled, 0 = disabled)
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
                           1,  // leave set to 1
                           };
                           

                           // ** Must be on firmware version 3.7 or higher **
uint8_t const RT5Xir = 0;     // 0 = disables IR Emitter for RetroTink 5x
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

uint8_t const MTVir = 0;   // Must have IR "Receiver" connected to the Donut Dongle for option 1 & 2.
                              // 0 = disables IR Receiver -> Serial Control for MT-VIKI 8 Port HDMI switch
                              //
                              // 1 = MT-VIKI 8 Port HDMI switch connected to "Extron sw1"
                              //     Using the RT4K Remote w/ the IR Receiver, AUX8 + profile button changes the MT-VIKI Input over Serial.
                              //     Sends auxprof SVS profiles listed below.
                              //
                              // 2 = MT-VIKI 8 Port HDMI switch connected to "Extron sw2"
                              //     Using the RT4K Remote w/ the IR Receiver, AUX8 + profile button changes the MT-VIKI Input over Serial.
                              //     Sends auxprof SVS profiles listed below. You can change them below to 101 - 108 to prevent SVS profile conflicts if needed.

uint8_t const MTVb = 3;    // MT-VIKI Buttons
                              // 0 = disables MT-VIKI face/remote buttons from changing profiles (saves memory)
                              //
                              // 1 = enables changing profiles for MT-VIKI face/remote buttons when connected to "Extron sw1" port
                              //
                              // 2 = enables changing profiles for MT-VIKI face/remote buttons when connected to "Extron sw2" port
                              //
                              // 3 = enabled for BOTH sw1 & sw2

uint8_t const TESmartir = 0;  // Must have IR "Receiver" connected to the Donut Dongle for option 1 and above.
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


String const auxpower = "LG"; // AUX8 + Power button sends power off/on via IR Emitter. "LG" OLEX CX is the only one implemented atm. 

//////////////////

int currentProf = 32222;  // current SVS profile number, set high initially
unsigned long currentGameTime = 0;
unsigned long prevGameTime = 0;

// Extron Global variables
String previnput[2] = {"discon","discon"}; // used to keep track of previous input
uint8_t otakuoff[2] = {2,2}; // 0 = at least 1 port is active, 1 = no ports are active, 2 = disconnected or not used yet
uint8_t eoutput[2]; // used to store Extron output
String const sstack = "00000000000000000000000000000000"; // static stack of 32 "0" used for comparisons
String stack1 = "00000000000000000000000000000000"; 
String stack2 = "00000000000000000000000000000000"; 
int currentInputSW1 = -1;
int currentInputSW2 = -1;
byte const VERB[5] = {0x57,0x33,0x43,0x56,0x7C}; // sets matrix switch to verbose level 3

// MT-VIKI / TESmart serial commands
byte const viki1[4] = {0xA5,0x5A,0x00,0xCC};
byte const viki2[4] = {0xA5,0x5A,0x01,0xCC};
byte const viki3[4] = {0xA5,0x5A,0x02,0xCC};
byte const viki4[4] = {0xA5,0x5A,0x03,0xCC};
byte const viki5[4] = {0xA5,0x5A,0x04,0xCC};
byte const viki6[4] = {0xA5,0x5A,0x05,0xCC};
byte const viki7[4] = {0xA5,0x5A,0x06,0xCC};
byte const viki8[4] = {0xA5,0x5A,0x07,0xCC};

byte const tesmart1[6] = {0xAA,0xBB,0x03,0x01,0x01,0xEE};
byte const tesmart2[6] = {0xAA,0xBB,0x03,0x01,0x02,0xEE};
byte const tesmart3[6] = {0xAA,0xBB,0x03,0x01,0x03,0xEE};
byte const tesmart4[6] = {0xAA,0xBB,0x03,0x01,0x04,0xEE};
byte const tesmart5[6] = {0xAA,0xBB,0x03,0x01,0x05,0xEE};
byte const tesmart6[6] = {0xAA,0xBB,0x03,0x01,0x06,0xEE};
byte const tesmart7[6] = {0xAA,0xBB,0x03,0x01,0x07,0xEE};
byte const tesmart8[6] = {0xAA,0xBB,0x03,0x01,0x08,0xEE};
byte const tesmart9[6] = {0xAA,0xBB,0x03,0x01,0x09,0xEE};
byte const tesmart10[6] = {0xAA,0xBB,0x03,0x01,0x0A,0xEE};
byte const tesmart11[6] = {0xAA,0xBB,0x03,0x01,0x0B,0xEE};
byte const tesmart12[6] = {0xAA,0xBB,0x03,0x01,0x0C,0xEE};
byte const tesmart13[6] = {0xAA,0xBB,0x03,0x01,0x0D,0xEE};
byte const tesmart14[6] = {0xAA,0xBB,0x03,0x01,0x0E,0xEE};
byte const tesmart15[6] = {0xAA,0xBB,0x03,0x01,0x0F,0xEE};
byte const tesmart16[6] = {0xAA,0xBB,0x03,0x01,0x10,0xEE};

// LS timer variables
unsigned long LScurrentTime = 0; 
unsigned long LScurrentTime2 = 0;
unsigned long LSprevTime = 0;
unsigned long LSprevTime2 = 0;

// IR Global variables
uint8_t repeatcount = 0; // used to help emulate the repeat nature of directional button presses
String svsbutton; // used to store 3 digit SVS profile when AUX8 is double pressed
uint8_t nument = 0; // used to keep track of how many digits have been entered for 3 digit SVS profile

// sendRTwake global variables
uint16_t currWakeProf[2] = {1,0}; // first index: 0 = remote button profile, 1 = SVS profiles. second index: profile number
bool RTwake = false;
unsigned long currentTime = 0;
unsigned long prevTime = 0;
unsigned long prevBlinkTime = 0;

// TESmart remote control
uint8_t auxTESmart[2] = {0,0}; // used to keep track if aux7 was pressed to change inputs on TESmart 16x1 HDMI switch via RT4K remote.
uint8_t extrabuttonprof = 0;  // Used to keep track of AUX8 button presses for addtional button profiles

// VIKI Manual Switch variables
unsigned long sendtimer = 0;
unsigned long sendtimer2 = 0;
unsigned long ITEtimer = 0;
unsigned long ITEtimer2 = 0;
uint8_t ITEstatus[] = {3,0,0};
uint8_t ITEstatus2[] = {3,0,0};
bool ITErecv = 0;
bool ITErecv2 = 0;
bool listenITE = 1;
bool listenITE2 = 1;
uint8_t ITEinputnum = 0;
uint8_t ITEinputnum2 = 0;


class SerialFTDI : public EspUsbHostSerial_FTDI {
  public:
  String cprof = "null";
  String tcprof = "null";
  virtual void task(void) override {
    EspUsbHost::task();
    if (this->isReady()) {
      esp_err_t err = usb_host_transfer_submit(this->usbTransfer_recv);
      if (err != ESP_OK && err != ESP_ERR_NOT_FINISHED && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGI("EspUsbHostSerial", "usb_host_transfer_submit() err=%x", err);
      }
      if(cprof != "null" && currentProf != cprof.toInt()){
        currentProf = cprof.toInt();
        analogWrite(LED_GREEN,222);
        if(currentProf >= 0){
          tcprof = "\rSVS NEW INPUT=" + cprof + "\r";
          submit((uint8_t *)reinterpret_cast<const uint8_t*>(&tcprof[0]), tcprof.length());
          delay(1000);
          tcprof = "\rSVS CURRENT INPUT=" + cprof + "\r";
        }
        if(currentProf < 0){
          tcprof = "\rremote prof" + String((-1)*currentProf) + "\r";
          delay(1000); // only added so the green led stays lit for 1 second. not needed for "remote prof" command.
        }
        submit((uint8_t *)reinterpret_cast<const uint8_t*>(&tcprof[0]), tcprof.length());
        analogWrite(LED_GREEN,255);

      }
    }
  }
};

SerialFTDI usbHost;

const int consolelen = sizeof(consoles) / sizeof(consoles[0]); // length of consoles DB, used frequently
const int gameDBlen = sizeof(gameDB) / sizeof(gameDB[0]); // length of gameDB...

void DDloop(void *pvParameters);
void gameIDTimer(void *pvParameters);

void setup(){

  initPCIInterruptForTinyReceiver();            // for IR Receiver
  WiFi.begin("SSID","password"); // WiFi creds go here. MUST be a 2.4GHz WiFi AP. 5GHz is NOT supported by the Nano ESP32.
  WiFi.setHostname("donutdongle.local"); // set hostname, call it whatever you like!
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT); // GREEN led lights up for 1 second when a SVS profile is sent
  pinMode(LED_BLUE, OUTPUT); // BLUE led is a WiFi activity. Long periods of blue means one of the gameID servers is not connecting.
  analogWrite(LED_GREEN,255);
  analogWrite(LED_BLUE,255);
  uint16_t gTime = 2000;

  #ifdef Serial
    usbHost.begin(115200); // leave at 115200 for RT4K usb-host mode connection
  #endif

  Serial.begin(9600);                           // set the baud rate for the RT4K VGA serial connection
  extronSerial.begin(9600,SERIAL_8N1,3,4);   // set the baud rate for the Extron sw1 Connection
  extronSerial.setTimeout(150);                 // sets the timeout for reading / saving into a string
  extronSerial2.begin(9600,SERIAL_8N1,8,9);  // set the baud rate for Extron sw2 Connection
  extronSerial2.setTimeout(150);                // sets the timeout for reading / saving into a string for the Extron sw2 Connection

  xTaskCreate(DDloop,"DDloop",4096,NULL,1,NULL);
  xTaskCreate(gameIDTimer,"gameIDTimer",4096,(void *)&gTime,1,NULL);
  
}  // end of setup

void loop(){
   // 
   // leave empty
   //
}  // end of loop()

void DDloop(void *pvParameters){
  (void)pvParameters;

  for(;;){
    readIR();
    readExtron1();
    readExtron2();
    if(RTwake)sendRTwake(10000); // 10000 is 10 seconds. After waking the RT4K, wait this amount of time before re-sending the latest profile change.

    #ifdef Serial  
      usbHost.task();
    #endif
  }
} // end of DDloop

int fetchGameIDProf(String gameID,int dp){ // looks at gameDB for a gameID -> profile match
  for(int i = 0; i < gameDBlen; i++){      // returns "DefaultProf" for console if nothing found and S0_gameID = true
    if(gameDB[i][0] == gameID){            // returns "-1" (meaning dont change anything) if nothing found and S0_gameID = false
      return gameDB[i][1].toInt();
      break;
   }
  }
  if(S0_gameID) return dp;
  else return -1;
}  // end of fetchGameIDProf()

void readGameID(){ // queries addresses in "consoles" array for gameIDs
  String payload = "";
  int result = 0;
  for(int i = 0; i < consolelen; i++){
    if(WiFi.status() == WL_CONNECTED){ // wait for WiFi connection
      HTTPClient http;
      http.setConnectTimeout(2000); // give only 2 seconds per console to check gameID, is only honored for IP-based addresses
      http.begin(consoles[i].Address);
      analogWrite(LED_BLUE,222);
      int httpCode = http.GET();             // start connection and send HTTP header
      if(httpCode > 0 || httpCode == -11){   // httpCode will be negative on error, let the read error slide...
        if(httpCode == HTTP_CODE_OK){        // console is healthy // HTTP header has been sent and Server response header has been handled
          consoles[i].Address = replaceDomainWithIP(consoles[i].Address); // replace Domain with IP in consoles array. this allows setConnectTimeout to be honored
          payload = http.getString();        
          JSONVar MCPjson = JSON.parse(payload); // 
          if(JSON.typeof(MCPjson) != "undefined"){ // If the response is JSON, continue
            if(MCPjson.hasOwnProperty("gameID")){  // If JSON contains gameID, reset payload to it's value
              payload = (const char*) MCPjson["gameID"];
            }
          }
          result = fetchGameIDProf(payload,consoles[i].DefaultProf);
          consoles[i].On = 1;
          if(consoles[i].Prof != result && result != -1){ // gameID found for console, set as King, unset previous King, send profile change 
            consoles[i].Prof = result;
            consoles[i].King = 1;
            for(int j=0;j < consolelen;j++){ // set previous King to 0
              if(i != j && consoles[j].King == 1)
                consoles[j].King = 0;
            }
            if(consoles[i].Prof >= 0) sendSVS(consoles[i].Prof);
            else sendRBP((-1) * consoles[i].Prof);
          }
       } 
      } // end of if(httpCode > 0 || httpCode == -11)
      else{ // console is off, set attributes to 0, find a console that is On starting at the top of the gameID list, set as King, send profile
        consoles[i].On = 0;
        consoles[i].Prof = 33333;
        if(consoles[i].King == 1){
          currentProf = 33333;
          usbHost.cprof = String(33333);
          for(int k=0;k < consolelen;k++){
            if(i == k){
              consoles[k].King = 0;
              for(int l=0;l < consolelen;l++){ // find next Console that is on
                if(consoles[l].On == 1){
                  consoles[l].King = 1;
                  if(consoles[l].Prof >= 0) sendSVS(consoles[l].Prof);
                  else sendRBP((-1) * consoles[l].Prof);
                  break;
                }
              }
            }
   
          } // end of for()
        } // end of if()
        int count = 0;
        for(int m=0;m < consolelen;m++){
          if(consoles[m].On == 0) count++;
        }
        if(count == consolelen && S0_pwr){
          if(S0_pwr_profile >= 0) sendSVS(S0_pwr_profile);
          else sendRBP((-1) * S0_pwr_profile);
        }   
      } // end of else()
    http.end();
    analogWrite(LED_BLUE, 255);
    }  // end of WiFi connection
  }
}  // end of readGameID()

void gameIDTimer(void *pvParameters){
  uint16_t gTime = *((uint16_t *)pvParameters);

  for (;;){
    currentGameTime = millis();  // Init timer
    if(prevGameTime == 0)       // If previous timer not initialized, do so now.
      prevGameTime = millis();
    if((currentGameTime - prevGameTime) >= gTime){ // If it's been longer than gIDTime, readGameID() and reset the timer.
      currentGameTime = 0;
      prevGameTime = 0;
      readGameID();
    }
 }
}  // end of gameIDTimer()

String replaceDomainWithIP(String input){
  String result = input;
  int startIndex = 0;
  while(startIndex < result.length()){
    int httpPos = result.indexOf("http://",startIndex); // Look for "http://"
    if (httpPos == -1) break;  // No "http://" found
    int domainStart = httpPos + 7; // Set the position right after "http://"
    int domainEnd = result.indexOf('/',domainStart);  // Find the end of the domain (start of the path)
    if(domainEnd == -1) domainEnd = result.length();  // If no path, consider till the end of the string
    String domain = result.substring(domainStart,domainEnd);
    if(!isIPAddress(domain)){ // If the domain is not an IP address, replace it
      IPAddress ipAddress;
      if(WiFi.hostByName(domain.c_str(),ipAddress)){  // Perform DNS lookup
        result.replace(domain,ipAddress.toString()); // Replace the Domain with the IP address
      }
    }
    startIndex = domainEnd;  // Continue searching after the domain
  } // end of while()
  return result;
} // end of replaceDomainWithIP()

bool isIPAddress(String str){
  IPAddress ip;
  return ip.fromString(str);  // Returns true if the string is a valid IP address
} // end of isIPAddress()

void readExtron1(){

    byte ecapbytes[40]; // used to store first 44 captured bytes / messages for Extron                
    String ecap = "0000000000000000000000000000000000000000"; // used to store Extron status messages for Extron in String format
    String einput = "00000000000000000000000000000000"; // used to store Extron input

    if(automatrixSW1){ // if automatrixSW1 is set "true" in options, then "0LS" is sent every 250ms to see if an input has changed
      LS0time1(250);
    }

    // listens to the Extron sw1 Port for changes
    // SIS Command Responses reference - Page 77 https://media.extron.com/public/download/files/userman/XP300_Matrix_B.pdf
    if(extronSerial.available() > 0){ // if there is data available for reading, read
      extronSerial.readBytes(ecapbytes,40); // read in and store only the first 13 bytes for every status message received from 1st Extron SW port
      if(debugE1CAP){
        Serial.print(F("ecap HEX: "));
        for(uint8_t i=0;i<40;i++){
          Serial.print(ecapbytes[i],HEX);Serial.print(F(" "));
        }
        Serial.println(F("\r"));
        ecap = String((char *)ecapbytes);
        Serial.print(F("ecap ASCII: "));Serial.println(ecap);
      }
    }
    ecap = String((char *)ecapbytes); // convert bytes to String for Extron switches


    if(ecap.substring(0,3) == "Out" && !automatrixSW1){ // store only the input and output states, some Extron devices report output first instead of input
      einput = ecap.substring(6,10);
      eoutput[0] = ecap.substring(3,5).toInt();
    }
    else if(ecap.substring(0,1) == "F"){ // detect if switch has changed auto/manual states
      einput = ecap.substring(4,8);
      eoutput[0] = 33;
    }
    else if(ecap.substring(0,3) == "Rpr"){ // detect if a Preset has been used
      einput = ecap.substring(0,5);
      eoutput[0] = 33;
    }
    else if(ecap.substring(amSizeSW1 + 6,amSizeSW1 + 9) == "Rpr"){ // detect if a Preset has been used 
      einput = ecap.substring(amSizeSW1 + 6,amSizeSW1 + 11);
      eoutput[0] = 33;
    }
    else if(ecap.substring(amSizeSW1 + 7,amSizeSW1 + 10) == "Rpr"){ // detect if a Preset has been used 
      einput = ecap.substring(amSizeSW1 + 7,amSizeSW1 + 12);
      eoutput[0] = 33;
    }
    else if(ecap.substring(0,3) == "In0" && ecap.substring(4,7) != "All" && ecap.substring(5,8) != "All" && automatrixSW1){ // start of automatrix
      if(ecap.substring(0,4) == "In00"){
        einput = ecap.substring(5,amSizeSW1 + 5);
      }else 
        einput = ecap.substring(4,amSizeSW1 + 4);
      for(uint8_t i=0;i<amSizeSW1;i++){
        if(einput[i] != stack1[i] || einput[currentInputSW1 - 1] == '0'){ // check to see if anything changed
          stack1[i] = einput[i];
          if(einput[i] != '0'){
            currentInputSW1 = i+1;
            if(vinMatrix[0] == 1 || vinMatrix[0] == 3)
            {
              recallPreset(1,vinMatrix[currentInputSW1]);
            }else{
              setTie(1,currentInputSW1);
              if(RT5Xir == 1)sendIR("5x",currentInputSW1,2); // RT5X profile
              if(RT5Xir && OSSCir)delay(500);
              if(OSSCir == 1)sendIR("ossc",currentInputSW1,3); // OSSC profile

              if(SVS==0)sendRBP(currentInputSW1); 
              else sendSVS(currentInputSW1);
            }
          }
        }
      } //end of for loop
      if(einput.substring(0,amSizeSW1) == sstack.substring(0,amSizeSW1) 
        && stack1.substring(0,amSizeSW1) == sstack.substring(0,amSizeSW1) && currentInputSW1 != 0){ // check for all inputs being off

        currentInputSW1 = 0;
        previnput[0] = "0";
        setTie(1,currentInputSW1);

        if(S0 && (!automatrixSW2 && (previnput[1] == "0" || previnput[1] == "In0 " || previnput[1] == "In00" || previnput[1] == "discon"))
              && otakuoff[0] && otakuoff[1]
              && (!automatrixSW2 && (previnput[1] == "discon" || voutMatrix[eoutput[1]+32]))){

            if(SVS==0)sendRBP(12); 
            else sendSVS(currentInputSW1);
        }
      }
    } // end of automatrix 
    else{                             // less complex switches only report input status, no output status
      einput = ecap.substring(0,4);
      eoutput[0] = 33;
    }


    // for Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references voutMaxtrix
    if((einput.substring(0,2) == "In" && voutMatrix[eoutput[0]] && !automatrixSW1) || (einput.substring(0,3) == "Rpr")){
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
      if(S0 && (currentInputSW2 <= 0) && ((einput == "In0 " || einput == "In00") && 
        (previnput[1] == "In0 " || previnput[1] == "In00" || previnput[1] == "discon")) && 
        otakuoff[0] && otakuoff[1] && 
        voutMatrix[eoutput[0]] && (previnput[1] == "discon" || voutMatrix[eoutput[1]+32])){

        if(SVS == 1)sendSVS(0);
        else sendRBP(12);

        previnput[0] = "0";
        if(previnput[1] != "discon")previnput[1] = "0";
      
      } // end of Extron S0

      if(previnput[0] == "0" && previnput[1].substring(0,2) == "In")previnput[0] = "In00";  // changes previnput[0] "0" state to "In00" when there is a newly active input on the other switch
      if(previnput[1] == "0" && previnput[0].substring(0,2) == "In")previnput[1] = "In00";

    }

    // VIKI Manual Switch Detection (created by: https://github.com/Arthrimus)
    // ** hdmi output must be connected when powering on switch for ITE messages to appear, thus manual button detection working **

    if((millis() - ITEtimer > 1200) && (MTVb == 1 || MTVb == 3)){  // Timer that disables sending SVS serial commands using the ITE mux data when there has recently been an autoswitch command (prevents duplicate commands)
      listenITE = 1;  // Sets listenITE to 1 so the ITE mux data can be used to send SVS serial commands again
      ITErecv = 0; // Turns off ITErecv so the SVS serial commands are not repeated if an autoswitch command preceeded the ITE commands
      ITEtimer = millis(); // Resets timer to current millis() count to disable this function once the variables have been updated
    }


    if(ecap.startsWith("=") && listenITE && (MTVb == 1 || MTVb == 3)){   // checks if the serial command from the VIKI starts with "=" This indicates that the command is an ITE mux status message
      if(ecap.substring(10,11) == "P"){        // checks the last value of the IT6635 mux. P3 points to inputs 1,2,3 / P2 points to inputs 4,5,6 / P1 input 7 / P0 input 8
        ITEstatus[0] = ecap.substring(11,12).toInt();
      }
      if(ecap.substring(18,20) == ">0"){       // checks the value of the IT66535 IC that points to Dev->0. P2 is input 1 / P1 is input 2 / P0 is input 3
        ITEstatus[1] = ecap.substring(12,13).toInt();
      }
      if(ecap.substring(18,20) == ">1"){       // checks the value of the IT66535 IC that points to Dev->1. P2 is input 4 / P1 is input 5 / P0 is input 6
        ITEstatus[2] = ecap.substring(12,13).toInt();
      }

      ITErecv = 1;                             // sets ITErecv to 1 indicating that an ITE message has been received and an SVS command can be sent once the sendtimer elapses
      sendtimer = millis();                    // resets sendtimer to millis()
      ITEtimer = millis();                    // resets ITEtimer to millis()
    }


    if((millis() - sendtimer > 300) && ITErecv && (MTVb == 1 || MTVb == 3)){ // wait 300ms to make sure all ITE messages are received in order to complete ITEstatus
      if(ITEstatus[0] == 3){                   // Checks if port 3 of the IT6635 chip is currently selected
        if(ITEstatus[1] == 2) ITEinputnum = 1;   // Checks if port 2 of the IT66353 DEV0 chip is selected, Sets ITEinputnum to input 1
        else if(ITEstatus[1] == 1) ITEinputnum = 2;   // Checks if port 1 of the IT66353 DEV0 chip is selected, Sets ITEinputnum to input 2
        else if(ITEstatus[1] == 0) ITEinputnum = 3;   // Checks if port 0 of the IT66353 DEV0 chip is selected, Sets ITEinputnum to input 3
      }
      else if(ITEstatus[0] == 2){                 // Checks if port 2 of the IT6635 chip is currently selected
        if(ITEstatus[2] == 2) ITEinputnum = 4;   // Checks if port 2 of the IT66353 DEV1 chip is selected, Sets ITEinputnum to input 4
        else if(ITEstatus[2] == 1) ITEinputnum = 5;   // Checks if port 1 of the IT66353 DEV1 chip is selected, Sets ITEinputnum to input 5
        else if(ITEstatus[2] == 0) ITEinputnum = 6;   // Checks if port 0 of the IT66353 DEV1 chip is selected, Sets ITEinputnum to input 6
      }
      else if(ITEstatus[0] == 1) ITEinputnum = 7;   // Checks if port 1 of the IT6635 chip is currently selected, Sets ITEinputnum to input 7
      else if(ITEstatus[0] == 0) ITEinputnum = 8;   // Checks if port 0 of the IT6635 chip is currently selected, Sets ITEinputnum to input 8

      ITErecv = 0;                              // sets ITErecv to 0 to prevent the message from being resent
      sendtimer = millis();                     // resets sendtimer to millis()
    }

    // for TESmart 4K60 / TESmart 4K30 / MT-VIKI HDMI switch on Extron sw1 / alt sw1
    if(ecapbytes[4] == 17 || ecapbytes[3] == 17 || ecapbytes[4] == 95 || ITEinputnum > 0){
      if(ecapbytes[6] == 22 || ecapbytes[5] == 22 || ecapbytes[11] == 48 || ITEinputnum == 1){
        if(RT5Xir == 1)sendIR("5x",1,2); // RT5X profile 1 
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",1,3); // OSSC profile 1

        if(SVS==0)sendRBP(1);
        else sendSVS(1);
      }
      else if(ecapbytes[6] == 23 || ecapbytes[5] == 23 || ecapbytes[11] == 49 || ITEinputnum == 2){
        if(RT5Xir == 1)sendIR("5x",2,2); // RT5X profile 2
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",2,3); // OSSC profile 2

        if(SVS==0)sendRBP(2);
        else sendSVS(2);
      }
      else if(ecapbytes[6] == 24 || ecapbytes[5] == 24 || ecapbytes[11] == 50 || ITEinputnum == 3){
        if(RT5Xir == 1)sendIR("5x",3,2); // RT5X profile 3
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",3,3); // OSSC profile 3

        if(SVS==0)sendRBP(3);
        else sendSVS(3);
      }
      else if(ecapbytes[6] == 25 || ecapbytes[5] == 25 || ecapbytes[11] == 51 || ITEinputnum == 4){
        if(RT5Xir == 1)sendIR("5x",4,2); // RT5X profile 4
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",4,3); // OSSC profile 4

        if(SVS==0)sendRBP(4);
        else sendSVS(4);
      }
      else if(ecapbytes[6] == 26 || ecapbytes[5] == 26 || ecapbytes[11] == 52 || ITEinputnum == 5){
        if(RT5Xir == 1)sendIR("5x",5,2); // RT5X profile 5
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",5,3); // OSSC profile 5

        if(SVS==0)sendRBP(5);
        else sendSVS(5);
      }
      else if(ecapbytes[6] == 27 || ecapbytes[5] == 27 || ecapbytes[11] == 53 || ITEinputnum == 6){
        if(RT5Xir == 1)sendIR("5x",6,2); // RT5X profile 6
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",6,3); // OSSC profile 6

        if(SVS==0)sendRBP(6);
        else sendSVS(6);
      }
      else if(ecapbytes[6] == 28 || ecapbytes[5] == 28 || ecapbytes[11] == 54 || ITEinputnum == 7){
        if(RT5Xir == 1)sendIR("5x",7,2); // RT5X profile 7
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",7,3); // OSSC profile 7

        if(SVS==0)sendRBP(7);
        else sendSVS(7);
      }
      else if(ecapbytes[6] == 29 || ecapbytes[5] == 29 || ecapbytes[11] == 55 || ITEinputnum == 8){
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

      ITEinputnum = 0;                     // Resets ITEinputnum to 0 so sendSVS will not repeat after this cycle through the void loop
      listenITE = 0;                       // Sets listenITE to 0 so the ITE mux data will be ignored while an autoswitch command is detected.
      ITEtimer = millis();                 // resets ITEtimer to millis() 
    }

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
        if(S0 && //otakuoff[1] &&  
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

      // set ecapbytes to 0 for next read
  memset(ecapbytes,0,sizeof(ecapbytes)); // ecapbytes is local variable, but superstitious clearing regardless :)

} // end of readExtron1()

void readExtron2(){
    

    byte ecapbytes[40]; // used to store first 13 captured bytes / messages for Extron                
    String ecap = "0000000000000000000000000000000000000000"; // used to store Extron status messages for Extron in String format
    String einput = "00000000000000000000000000000000"; // used to store Extron input

    if(automatrixSW2){ // if automatrixSW2 is set "true" in options, then "0LS" is sent every 250ms to see if an input has changed
      LS0time2(250);
    }

    // listens to the Extron sw2 Port for changes
    if(extronSerial2.available() > 0){ // if there is data available for reading, read
    extronSerial2.readBytes(ecapbytes,40); // read in and store only the first 13 bytes for every status message received from 2nd Extron port
      if(debugE2CAP){
        Serial.print(F("ecap2 HEX: "));
        for(uint8_t i=0;i<40;i++){
          Serial.print(ecapbytes[i],HEX);Serial.print(F(" "));
        }
        Serial.println(F("\r"));
        ecap = String((char *)ecapbytes);
        Serial.print(F("ecap2 ASCII: "));Serial.println(ecap);
      }
    }
    ecap = String((char *)ecapbytes);

    if(ecap.substring(0,3) == "Out" && !automatrixSW2){ // store only the input and output states, some Extron devices report output first instead of input
      einput = ecap.substring(6,10);
      eoutput[1] = ecap.substring(3,5).toInt();
    }
    else if(ecap.substring(0,1) == "F"){ // detect if switch has changed auto/manual states
      einput = ecap.substring(4,8);
      eoutput[1] = 33;
    }
    else if(ecap.substring(0,3) == "Rpr"){ // detect if a Preset has been used
      einput = ecap.substring(0,5);
      eoutput[1] = 33;
    }
    else if(ecap.substring(amSizeSW2 + 6,amSizeSW2 + 9) == "Rpr"){ // detect if a Preset has been used 
      einput = ecap.substring(amSizeSW2 + 6,amSizeSW2 + 11);
      eoutput[1] = 33;
    }
    else if(ecap.substring(amSizeSW2 + 7,amSizeSW2 + 10) == "Rpr"){ // detect if a Preset has been used 
      einput = ecap.substring(amSizeSW2 + 7,amSizeSW2 + 12);
      eoutput[1] = 33;
    }
    else if(ecap.substring(0,3) == "In0" && ecap.substring(4,7) != "All" && ecap.substring(5,8) != "All" && automatrixSW2){ // start of automatrix
      if(ecap.substring(0,4) == "In00"){
        einput = ecap.substring(5,amSizeSW2 + 5);
      }else 
        einput = ecap.substring(4,amSizeSW2 + 4);
      for(uint8_t i=0;i<amSizeSW2;i++){
        if(einput[i] != stack2[i] || einput[currentInputSW2 - 1] == '0'){ // check to see if anything changed
          stack2[i] = einput[i];
          if(einput[i] != '0'){
            currentInputSW2 = i+1;
            if(vinMatrix[0] == 2 || vinMatrix[0] == 3)
            {
              recallPreset(2,vinMatrix[currentInputSW2 + 32]);
            }
            else{
              setTie(2,currentInputSW2);
              sendSVS(currentInputSW2 + 100);
            }
          }
        }
      } //end of for loop
      if(einput.substring(0,amSizeSW2) == sstack.substring(0,amSizeSW2) 
        && stack2.substring(0,amSizeSW2) == sstack.substring(0,amSizeSW2) && currentInputSW2 != 0){ // check for all inputs being off

        currentInputSW2 = 0;
        previnput[1] = "0";
        setTie(2,currentInputSW2);

        if(S0 && (!automatrixSW1 && (previnput[0] == "0" || previnput[0] == "In0 " || previnput[0] == "In00" || previnput[0] == "discon"))
              && otakuoff[0] && otakuoff[1]
              && (!automatrixSW1 && (previnput[0] == "discon" || voutMatrix[eoutput[0]]))){

          sendSVS(currentInputSW2);
        }

      }
    } // end of automatrix
    else{                              // less complex switches only report input status, no output status
      einput = ecap.substring(0,4);
      eoutput[1] = 33;
    }


    // For Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references voutMaxtrix
    if((einput.substring(0,2) == "In" && voutMatrix[eoutput[1]+32] && !automatrixSW2) || (einput.substring(0,3) == "Rpr")){
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
        (currentInputSW1 <= 0) && ((einput == "In0 " || einput == "In00") && 
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


    // VIKI Manual Switch Detection (created by: https://github.com/Arthrimus)
    // ** hdmi output must be connected when powering on switch for ITE messages to appear, thus manual button detection working **

    if((millis() - ITEtimer2 > 1200) && (MTVb > 1)){  // Timer that disables sending SVS serial commands using the ITE mux data when there has recently been an autoswitch command (prevents duplicate commands)
      listenITE2 = 1;  // Sets listenITE2 to 1 so the ITE mux data can be used to send SVS serial commands again
      ITErecv2 = 0; // Turns off ITErecv2 so the SVS serial commands are not repeated if an autoswitch command preceeded the ITE commands
      ITEtimer2 = millis(); // Resets timer to current millis() count to disable this function once the variables hav been updated
    }


    if(ecap.startsWith("=") && listenITE2 && (MTVb > 1)){   // checks if the serial command from the VIKI starts with "=" This indicates that the command is an ITE mux status message
      if(ecap.substring(10,11) == "P"){       // checks the last value of the IT6635 mux. P3 points to inputs 1,2,3 / P2 points to inputs 4,5,6 / P1 input 7 / P0 input 8
        ITEstatus2[0] = ecap.substring(11,12).toInt();
      }
      if(ecap.substring(18,20) == ">0"){       // checks the value of the IT66535 IC that points to Dev->0. P2 is input 1 / P1 is input 2 / P0 is input 3
        ITEstatus2[1] = ecap.substring(12,13).toInt();
      }
      if(ecap.substring(18,20) == ">1"){       // checks the value of the IT66535 IC that points to Dev->1. P2 is input 4 / P1 is input 5 / P0 is input 6
        ITEstatus2[2] = ecap.substring(12,13).toInt();
      }
      ITErecv2 = 1;                             // sets ITErecv2 to 1 indicating that an ITE message has been received and an SVS command can be sent once the sendtimer elapses
      sendtimer2 = millis();                    // resets sendtimer2 to millis()
      ITEtimer2 = millis();                    // resets ITEtimer2 to millis()
    }


    if((millis() - sendtimer2 > 300) && ITErecv && (MTVb > 1)){ // wait 300ms to make sure all ITE messages are received in order to complete ITEstatus
      if(ITEstatus2[0] == 3){                   // Checks if port 3 of the IT6635 chip is currently selected
        if(ITEstatus2[1] == 2) ITEinputnum2 = 1;   // Checks if port 2 of the IT66353 DEV0 chip is selected, Sets ITEinputnum to input 1
        else if(ITEstatus2[1] == 1) ITEinputnum2 = 2;   // Checks if port 1 of the IT66353 DEV0 chip is selected, Sets ITEinputnum to input 2
        else if(ITEstatus2[1] == 0) ITEinputnum2 = 3;   // Checks ITEstatus array position` 1 to determine if port 0 of the IT66353 DEV0 chip is selected, Sets ITEinputnum to input 3
      }
      else if(ITEstatus2[0] == 2){                 // Checks if port 2 of the IT6635 chip is currently selected
        if(ITEstatus2[2] == 2) ITEinputnum2 = 4;   // Checks if port 2 of the IT66353 DEV1 chip is selected, Sets ITEinputnum to input 4
        else if(ITEstatus2[2] == 1) ITEinputnum2 = 5;   // Checks if port 1 of the IT66353 DEV1 chip is selected, Sets ITEinputnum to input 5
        else if(ITEstatus2[2] == 0) ITEinputnum2 = 6;   // Checks if port 0 of the IT66353 DEV1 chip is selected, Sets ITEinputnum to input 6
      }
      else if(ITEstatus2[0] == 1) ITEinputnum2 = 7;   // Checks if port 1 of the IT6635 chip is currently selected, Sets ITEinputnum to input 7
      else if(ITEstatus2[0] == 0) ITEinputnum2 = 8;   // Checks if port 0 of the IT6635 chip is currently selected, Sets ITEinputnum to input 8

      ITErecv2 = 0;                              // sets ITErecv2 to 0 to prevent the message from being resent
      sendtimer2 = millis();                     // resets sendtimer2 to millis()
    }

    // for TESmart 4K60 / TESmart 4K30 / MT-VIKI HDMI switch on Extron alt sw2 Port
    if(ecapbytes[4] == 17 || ecapbytes[3] == 17 || ecapbytes[4] == 95 || ITEinputnum2 > 0){
      if(ecapbytes[6] == 22 || ecapbytes[5] == 22 || ecapbytes[11] == 48 || ITEinputnum2 == 1){
        if(RT5Xir == 3)sendIR("5x",1,2); // RT5X profile 1 
        sendSVS(101);
      }
      else if(ecapbytes[6] == 23 || ecapbytes[5] == 23 || ecapbytes[11] == 49 || ITEinputnum2 == 2){
        if(RT5Xir == 3)sendIR("5x",2,2); // RT5X profile 2
        sendSVS(102);
      }
      else if(ecapbytes[6] == 24 || ecapbytes[5] == 24 || ecapbytes[11] == 50 || ITEinputnum2 == 3){
        if(RT5Xir == 3)sendIR("5x",3,2); // RT5X profile 3
        sendSVS(103);
      }
      else if(ecapbytes[6] == 25 || ecapbytes[5] == 25 || ecapbytes[11] == 51 || ITEinputnum2 == 4){
        if(RT5Xir == 3)sendIR("5x",4,2); // RT5X profile 4
        sendSVS(104);
      }
      else if(ecapbytes[6] == 26 || ecapbytes[5] == 26 || ecapbytes[11] == 52 || ITEinputnum2 == 5){
        if(RT5Xir == 3)sendIR("5x",5,2); // RT5X profile 5
        sendSVS(105);
      }
      else if(ecapbytes[6] == 27 || ecapbytes[5] == 27 || ecapbytes[11] == 53 || ITEinputnum2 == 6){
        if(RT5Xir == 3)sendIR("5x",6,2); // RT5X profile 6
        sendSVS(106);
      }
      else if(ecapbytes[6] == 28 || ecapbytes[5] == 28 || ecapbytes[11] == 54 || ITEinputnum2 == 7){
        if(RT5Xir == 3)sendIR("5x",7,2); // RT5X profile 7
        sendSVS(107);
      }
      else if(ecapbytes[6] == 29 || ecapbytes[5] == 29 || ecapbytes[11] == 55 || ITEinputnum2 == 8){
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

      ITEinputnum2 = 0;                     // Resets ITEinputnum2 to 0 so sendSVS will not repeat after this cycle through the void loop
      listenITE2 = 0;                       // Sets listenITE2 to 0 so the ITE mux data will be ignored while an autoswitch command is detected.
      ITEtimer2 = millis();                 // resets ITEtimer2 to millis()
    }

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

    // set ecapbytes to 0 for next read
    memset(ecapbytes,0,sizeof(ecapbytes)); // ecapbytes is local variable, but superstitious clearing regardless :)

} // end of readExtron2()

void readIR(){

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
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[0]);                                                               // Can be changed to "ANY" SVS profile in the OPTIONS section
        else if(MTVir == 1){extronSerial.write(viki1,4);sendSVS(auxprof[0]);}
        else if(MTVir == 2){extronSerial2.write(viki1,4);sendSVS(auxprof[0]);}
        if(TESmartir > 1){extronSerial2.write(tesmart1,6);sendSVS(101);}                                                                   
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[1]);
        else if(MTVir == 1){extronSerial.write(viki2,4);sendSVS(auxprof[1]);}
        else if(MTVir == 2){extronSerial2.write(viki2,4);sendSVS(auxprof[1]);}
        if(TESmartir > 1){extronSerial2.write(tesmart2,6);sendSVS(102);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[2]);
        else if(MTVir == 1){extronSerial.write(viki3,4);sendSVS(auxprof[2]);}
        else if(MTVir == 2){extronSerial2.write(viki3,4);sendSVS(auxprof[2]);}
        if(TESmartir > 1){extronSerial2.write(tesmart3,6);sendSVS(103);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[3]);
        else if(MTVir == 1){extronSerial.write(viki4,4);sendSVS(auxprof[3]);}
        else if(MTVir == 2){extronSerial2.write(viki4,4);sendSVS(auxprof[3]);}
        if(TESmartir > 1){extronSerial2.write(tesmart4,6);sendSVS(104);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[4]);
        else if(MTVir == 1){extronSerial.write(viki5,4);sendSVS(auxprof[4]);}
        else if(MTVir == 2){extronSerial2.write(viki5,4);sendSVS(auxprof[4]);}
        if(TESmartir > 1){extronSerial2.write(tesmart5,6);sendSVS(105);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[5]);
        else if(MTVir == 1){extronSerial.write(viki6,4);sendSVS(auxprof[5]);}
        else if(MTVir == 2){extronSerial2.write(viki6,4);sendSVS(auxprof[5]);}
        if(TESmartir > 1){extronSerial2.write(tesmart6,6);sendSVS(106);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[6]);
        else if(MTVir == 1){extronSerial.write(viki7,4);sendSVS(auxprof[6]);}
        else if(MTVir == 2){extronSerial2.write(viki7,4);sendSVS(auxprof[6]);}
        if(TESmartir > 1){extronSerial2.write(tesmart7,6);sendSVS(107);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[7]);
        else if(MTVir == 1){extronSerial.write(viki8,4);sendSVS(auxprof[7]);}
        else if(MTVir == 2){extronSerial2.write(viki8,4);sendSVS(auxprof[7]);}
        if(TESmartir > 1){extronSerial2.write(tesmart8,6);sendSVS(108);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 1){ // profile button 9
        if(TESmartir < 2)sendSVS(auxprof[8]);
        if(TESmartir > 1){extronSerial2.write(tesmart9,6);sendSVS(109);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 37){ // profile button 10
        if(TESmartir < 2)sendSVS(auxprof[9]);
        if(TESmartir > 1){extronSerial2.write(tesmart10,6);sendSVS(110);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 38){ // profile button 11
        if(TESmartir < 2)sendSVS(auxprof[10]);
        if(TESmartir > 1){extronSerial2.write(tesmart11,6);sendSVS(111);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 39){ // profile button 12
        if(TESmartir < 2)sendSVS(auxprof[11]);
        if(TESmartir > 1){extronSerial2.write(tesmart12,6);sendSVS(112);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 56){ // aux1 button
        if(TESmartir > 1){extronSerial2.write(tesmart13,6);sendSVS(113);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 57){ // aux2 button
        if(TESmartir > 1){extronSerial2.write(tesmart14,6);sendSVS(114);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 58){ // aux3 button
        if(TESmartir > 1){extronSerial2.write(tesmart15,6);sendSVS(115);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 59){ // aux4 button
        if(TESmartir > 1){extronSerial2.write(tesmart16,6);sendSVS(116);}
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
        Serial.println(F("\rremote aux6\r"));
      }
      else if(ir_recv_command == 60){
        Serial.println(F("\rremote aux5\r"));
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
  else if(type == "LG"){ // LG TV
      irsend.sendNEC(0xFB04,0xF708,3); // Power button
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
      if((currWakeProf[0] == 1 && currWakeProf[1] != 0) || (currWakeProf[0] == 0 && currWakeProf[1] != 12)){
        if(currWakeProf[0])
          sendSVS(currWakeProf[1]);
        else
          sendRBP(currWakeProf[1]);
      }
    }
    if(currentTime - prevBlinkTime >= 300){
      prevBlinkTime = currentTime;
      if(digitalRead(LED_BUILTIN) == LOW)digitalWrite(LED_BUILTIN,HIGH);
      else digitalWrite(LED_BUILTIN,LOW);
    }
} // end of sendRTwake()

void LS0time1(unsigned long eTime){
  LScurrentTime = millis();  // Init timer
  if(LSprevTime == 0)       // If previous timer not initialized, do so now.
    LSprevTime = millis();
  if((LScurrentTime - LSprevTime) >= eTime){ // If it's been longer than eTime, send "0LS" and reset the timer.
    LScurrentTime = 0;
    LSprevTime = 0;
    extronSerial.print(F("0LS"));
    delay(20);
 }
} // end of LS0time1()

void LS0time2(unsigned long eTime){
  LScurrentTime2 = millis();  // Init timer
  if(LSprevTime2 == 0)       // If previous timer not initialized, do so now.
    LSprevTime2 = millis();
  if((LScurrentTime2 - LSprevTime2) >= eTime){ // If it's been longer than eTime, send "0LS" and reset the timer.
    LScurrentTime2 = 0;
    LSprevTime2 = 0;
    extronSerial2.print(F("0LS"));
    delay(20);
 }
} // end of LS0time2()

void setTie(uint8_t sw, uint8_t num){
  if(sw == 1){
    if(voutMatrix[0]){
      extronSerial.print(num);
      extronSerial.print(F("*"));
      extronSerial.print(F("!"));
    }
    else{
      for(uint8_t i=1;i<(amSizeSW1 + 1);i++){
        if(voutMatrix[i]){
          extronSerial.print(num);
          extronSerial.print(F("*"));
          extronSerial.print(i);
          extronSerial.print(F("!"));
        }
      }
    }
  }
  else if(sw == 2){
    if(voutMatrix[0]){
      extronSerial2.print(num);
      extronSerial2.print(F("*"));
      extronSerial2.print(F("!"));
    }
    else{
      for(uint8_t i=17;i<(amSizeSW2 + 17);i++){
        if(voutMatrix[i]){
          extronSerial2.print(num);
          extronSerial2.print(F("*"));
          extronSerial2.print(i - 16);
          extronSerial2.print(F("!"));
        }
      }
    }
  }
} // end of setTie()

void recallPreset(uint8_t sw, uint8_t num){
  if(sw == 1){
    extronSerial.print(num);
    extronSerial.print(F("."));
  }
  else if(sw == 2){
    extronSerial2.print(num);
    extronSerial2.print(F("."));
  }
} // end of recallPreset()

void sendSVS(uint16_t num){
  analogWrite(LED_RED,255);
  analogWrite(LED_BLUE,255);
  analogWrite(LED_GREEN,222);
  usbHost.cprof = String(num);

  Serial.print(F("\rSVS NEW INPUT="));
  if(num != 0)Serial.print(num + offset);
  else Serial.print(num);;
  Serial.println(F("\r"));
  delay(1000);
  Serial.print(F("\rSVS CURRENT INPUT="));
  if(num != 0)Serial.print(num + offset);
  else Serial.print(num);
  Serial.println(F("\r"));

  currWakeProf[0] = 1; // 1 is SVS profile
  if(num != 0)currWakeProf[1] = num + offset;
  else currWakeProf[1] = num;
  analogWrite(LED_GREEN,255);
} // end of sendSVS()

void sendRBP(uint8_t prof){ // send Remote Button Profile
  analogWrite(LED_RED,255);
  analogWrite(LED_BLUE,255);
  analogWrite(LED_GREEN,222);
  usbHost.cprof = String((-1) * prof);

  Serial.print(F("\rremote prof"));
  Serial.print(prof);
  Serial.println(F("\r"));

  currWakeProf[0] = 0; // 0 is Remote Button Profile
  currWakeProf[1] = prof;
  delay(1000);
  analogWrite(LED_GREEN,255);
} // end of sendRBP()
