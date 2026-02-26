/*
* Donut Dongle beta v1.7p
* Copyright (C) 2026 @Donutswdad
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

#define IR_SEND_PIN 11  // Optional IR LED Emitter for RT5X / OSSC compatibility. Sends IR data out Arduino pin D11
#define IR_RECEIVE_PIN 2 // Optional IR Receiver on pin D2
#define RAW_BUFFER_LENGTH 50 // IR Receive buffer length for NEC commands
#define MAX_BYTES 44
#define MAX_EINPUT 36

#define EXTRON1 0
#define EXTRON2 1
#define GSCART1 2
#define GSCART2 3

#include <TinyIRReceiver.hpp> // these next 3 can be found in the built-in Library Manager
#include <IRremote.hpp>
#include <SoftwareSerial.h>
#include <AltSoftSerial.h>  // https://github.com/PaulStoffregen/AltSoftSerial in order to have a 3rd Serial port for 2nd Extron Switch / alt sw2
                            // Step 1 - Goto the github link above. Click the GREEN "<> Code" box and "Download ZIP"
                            // Step 2 - In Arudino IDE; goto "Sketch" -> "Include Library" -> "Add .ZIP Library"
struct profileorder {
  int Prof;
  uint8_t On;
  uint8_t King;
  uint8_t Order;
};

profileorder mswitch[4] = {{0,0,0,0},{0,0,0,1},{0,0,0,2},{0,0,0,3}};
uint8_t mswitchSize = 4;

/*
////////////////////
//    OPTIONS    //
//////////////////
*/

uint8_t const debugE1CAP = 0; // line ~505
uint8_t const debugE2CAP = 0; // line ~771
uint8_t const debugState = 0; // line ~432


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


bool const S0 = false;  // (Profile 0) default is false
                         //
                         //  ** Recommended to remove any /profile/SVS/S0_<user defined>.rt4 profiles and leave this option set false if using in tandem with the Scalable Video Switch. **
                         //  ** Does not work with TESmart HDMI switches **
                         //
                         // For SVS=0; set true to load "Remote Profile 12" when all ports are in-active on SW1 (and SW2 if connected). 
                         // You can assign it to a generic HDMI profile for example.
                         // If your device has a 12th input, SVS will be used instead. "If" you also have an active 2nd Extron Switch, Remote Profile 12
                         // will only load if "BOTH" switches have all in-active ports.
                         // 
                         // For SVS=1, /profile/SVS/ "S0_<user defined>.rt4" will load instead of Remote Profile 12
                         //
                         // For SVS=2, Remote Profile 12 will be used instead of "S0_<user defined>.rt4"
                         //


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

// For Extron Matrix switches that support DSVP. RGBS and HDMI/DVI video types.

#define automatrixSW1 false // set true for auto matrix switching on "SW1" port
#define automatrixSW2 false // set true for auto matrix switching on "SW2" port

///////////////////////////////

uint8_t const ExtronVideoOutputPortSW1 = 1; // For certain Extron Matrix models, must specify the video output port that connects to RT4K
uint8_t const ExtronVideoOutputPortSW2 = 1; // can also be used for auto matrix mode as shown in next option

///////////////////////////////

uint8_t const vinMatrix[65] = {0,  // MATRIX switchers  // When auto matrix mode is enabled: (automatrixSW1 / SW2 defined above)
                                                        // set to 0 for the auto switched input to tie to all outputs
                                                        // set to 1 for the auto switched input to trigger a Preset
                                                        // set to 2 for the auto switched input to tie to "ExtronVideoOutputPortSW1" / "ExtronVideoOutputPortSW2"
                                                        //
                                                        // For option 1, set the following inputs to the desired Preset #
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

uint8_t const gctl = 0; // 1 = Enables gscart/gcomp manual input selection
                        // 0 = Disable (Default)
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

char const auxpower[] = "LG"; // AUX8 + Power button sends power off/on via IR Emitter. "LG" OLEX CX is the only one implemented atm. 

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// gscart / gcomp adjustment variables for port detection
float const highsamvolt[2] = {1.6,1.6}; // for gscart sw1,sw2 rise above this voltage for a high sample
byte const apin[2][3] = {{A0,A1,A2},{A3,A4,A5}}; // defines analog pins used to read bit0, bit1, bit2
uint8_t const dch = 15; // (duty cycle high) at least this many high samples per "samsize" for a high bit (~75% duty cycle)
uint8_t const dcl = 5; // (duty cycle low) at least this many high samples and less than "dch" per "samsize" indicate all inputs are in-active (~50% duty cycle)
uint8_t const samsize = 20; // total number of ADC samples required to capture at least 1 period
uint8_t const fpdccountmax = 3; // number of periods required when in the 50% duty cycle state before a Profile 0 is triggered.

////////////////////////////////////////////////////////////////////////

// Misc
int minFreeRam = 2048; // used by recordMem(), on RT4K Diag screen, press AUX8 + Enter button to show Max used mem% and Uptime. Best if below 73%

// automatrix variables
#if automatrixSW1 || automatrixSW2
uint8_t AMstate[32];
uint32_t prevAMstate = 0;
int AMstateTop = -1;
uint8_t amSizeSW1 = 8; // 8 by default, updates if a different size is discovered
uint8_t amSizeSW2 = 8; // ...
#endif

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
uint8_t auxTESmart = 0; // used to keep track if aux7 was pressed to change inputs on TESmart 16x1 HDMI switch via RT4K remote.
uint8_t extrabuttonprof = 0;  // Used to keep track of AUX8 button presses for addtional button profiles

// Extron sw1 / alt sw1 software serial port -> MAX3232 TTL IC (when jumpers set to "H")
SoftwareSerial extronSerial(3,4); // setup a software serial port for listening to Extron sw1 / alt sw1. rxPin = 3 / txPin = 4

// Extron sw2 / alt sw2 software serial port -> MAX3232 TTL IC (when jumpers set to "H")
AltSoftSerial extronSerial2; // setup yet another serial port for listening to Extron sw2 / alt sw2. hardcoded to pins D8 / D9

// Extron Global variables
uint8_t eoutput[2]; // used to store Extron output
int currentInputSW1 = -1;
int currentInputSW2 = -1;
int currentProf = 0; // negative numbers for Remote Button profiles, positive for SVS profiles
char ecap[MAX_BYTES] = {};
char einput[MAX_EINPUT] = {};
byte ecapbytes[MAX_BYTES] = {0}; // used to store first MAX_BYTES bytes / messages for Extron capture

// Serial commands
byte viki[4] = {0xA5,0x5A,0x07,0xCC};
byte tesmart[6] = {0xAA,0xBB,0x03,0x01,0x01,0xEE};
byte const VERB[5] = {0x57,0x33,0x43,0x56,0x7C}; // sets matrix switch to verbose level 3

// LS timer variables
unsigned long LScurrentTime = 0; 
unsigned long LScurrentTime2 = 0;
unsigned long LSprevTime = 0;
unsigned long LSprevTime2 = 0;

// IR Global variables
uint8_t repeatcount = 0; // used to help emulate the repeat nature of directional button presses
char svsbutton[3] = {}; // used to store 3 digit SVS profile when AUX8 is double pressed
uint8_t nument = 0; // used to keep track of how many digits have been entered for 3 digit SVS profile

// sendRTwake global variables
bool RTwake = false;
unsigned long currentTime = 0;
unsigned long prevTime = 0;
unsigned long prevBlinkTime = 0;

#if !automatrixSW1
// MT-VIKI Time variables
unsigned long MTVcurrentTime = 0; 
unsigned long MTVprevTime = 0;
unsigned long sendtimer = 0;
unsigned long ITEtimer = 0;
#endif

#if !automatrixSW2
// MT-VIKI Time variables
unsigned long MTVcurrentTime2 = 0;
unsigned long MTVprevTime2 = 0;
unsigned long sendtimer2 = 0;
unsigned long ITEtimer2 = 0;
#endif

// MT-VIKI Manual Switch variables
uint8_t ITEstatus[] = {3,0,0};
uint8_t ITEstatus2[] = {3,0,0};
bool ITErecv[2] = {0,0};
bool listenITE[2] = {1,1};
uint8_t ITEinputnum[2] = {0,0};
uint8_t currentMTVinput[2] = {0,0};
bool MTVdiscon[2] = {false,false};
bool MTVddSW1 = false;
bool MTVddSW2 = false;

void setup(){

    pinMode(A2,INPUT);pinMode(A1,INPUT);pinMode(A0,INPUT); // set gscart1 port as input
    pinMode(A5,INPUT);pinMode(A4,INPUT);pinMode(A3,INPUT); // set gscart2 port as input
    initPCIInterruptForTinyReceiver(); // for IR Receiver
    Serial.begin(9600); // set the baud rate for the RT4K Serial Connection
    while(!Serial){;}   // allow connection to establish before continuing
    Serial.print(F("\r")); // clear RT4K Serial buffer
    extronSerial.begin(9600); // set the baud rate for the Extron sw1 Connection
    extronSerial.setTimeout(50); // sets the timeout for reading / saving into a string
    extronSerial2.begin(9600); // set the baud rate for Extron sw2 Connection
    extronSerial2.setTimeout(50); // sets the timeout for reading / saving into a string for the Extron sw2 Connection
    pinMode(LED_BUILTIN, OUTPUT); // initialize builtin led for RTwake
    memset(ecap,0,sizeof(ecap)); // set to all 0s
    memset(einput,0,sizeof(einput)); // set to all 0s

} // end of setup

void loop(){

  // below are a list of functions that loop over and over to read in port changes and other misc tasks. you can disable them by commenting them out

  readIR(); // intercepts the remote's button presses and relays them through the Serial interface giving a much more responsive experience and new functionality

  readGscart1(); // reads A0,A1,A2 pins to see which port, if any, are active

  readGscart2(); // reads A3,A4,A5 pins to see which port, if any, are active

  readExtron1(); // also reads TESmart HDMI and Otaku Games Scart switch on "alt sw1" port

  readExtron2(); // also reads TESmart HDMI and Otaku Games Scart switch on "alt sw2" port

  if(RTwake)sendRTwake(8000); // 8000 is 8 seconds. After waking the RT4K, wait this amount of time before re-sending the latest profile change.

  if(debugState){
    Serial.print(F("GSCART1 "));Serial.print(F(" On: "));Serial.print(mswitch[GSCART1].On);Serial.print(F(" King: "));
    Serial.print(mswitch[GSCART1].King);Serial.print(F(" Prof: "));Serial.print(mswitch[GSCART1].Prof);
    Serial.print(F(" -- GSCART2 "));Serial.print(F(" On: "));Serial.print(mswitch[GSCART2].On);Serial.print(F(" King: "));
    Serial.print(mswitch[GSCART2].King);Serial.print(F(" Prof: "));Serial.print(mswitch[GSCART2].Prof);  
    Serial.print(F(" -- EXTRON1 "));Serial.print(F(" On: "));Serial.print(mswitch[EXTRON1].On);Serial.print(F(" King: "));
    Serial.print(mswitch[EXTRON1].King);Serial.print(F(" Prof: "));Serial.print(mswitch[EXTRON1].Prof);
    Serial.print(F(" -- EXTRON2 "));Serial.print(F(" On: "));Serial.print(mswitch[EXTRON2].On);Serial.print(F(" King: "));
    Serial.print(mswitch[EXTRON2].King);Serial.print(F(" Prof: "));Serial.println(mswitch[EXTRON2].Prof);
  }

  recordMem();
} // end of loop()

bool substringEquals(const char* buffer, int start, int end, const char* compareTo){
  size_t len = end - start;
  for(size_t i=0;i < len;i++){
    if(tolower(buffer[start + i]) != tolower(compareTo[i])) return false;
  }
  return true;
} // end of substringEquals()

void copySnippet(const char* src, int start, int end, char* dest){
  size_t len = end - start;
  for(size_t i=0;i < len;i++){
    dest[i] = src[start + i];
  }
} // end of copySnippet()

int sliceToInt(const char* buffer, int start, int end){
  int value = 0;
  for(int i=start;i < end;i++){
    value = value * 10 + (buffer[i] - '0');
  }
  return value;
} // end of sliceToInt()

uint8_t lengthUpToLineEnding(const char* buffer, uint8_t bufLen){
  for(uint8_t i = 0;i < bufLen;i++){
    if(buffer[i] == '\r' || buffer[i] == '\n'){
      return i;  // stop at first line ending
    }
  }
  return bufLen;  // no line ending found, full length
} // end of lengthUpToLineEnding()

void recordMem(){
  extern char __heap_start;
  extern char* __brkval;
  char top;
  int current = &top - (__brkval ? __brkval : &__heap_start); // calc free mem
  if(current < minFreeRam){
    minFreeRam = current;
  }
} // end of recordMem();


void readExtron1(){

  #if automatrixSW1 // if automatrixSW1 is set "true" in options, then "0LS" is sent every 500ms to see if an input has changed
      LS0time1(500);
  #endif

  #if !automatrixSW1
    if(MTVddSW1){            // if a MT-VIKI switch has been detected on SW1, then the currently active MT-VIKI hdmi port is checked for disconnection
      MTVtime1(2000);
    }
  #endif

    // listens to the Extron sw1 Port for changes
    // SIS Command Responses reference - Page 77 https://media.extron.com/public/download/files/userman/XP300_Matrix_B.pdf
    if(extronSerial.available() > 0){ // if there is data available for reading, read
      extronSerial.readBytes(ecapbytes,MAX_BYTES); // read in and store only the first MAX_BYTES bytes for every status message received from 1st Extron SW port
      if(debugE1CAP){
        Serial.print(F("ecap HEX: "));
        for(uint8_t i=0;i<MAX_BYTES;i++){
          Serial.print(ecapbytes[i],HEX);Serial.print(F(" "));
        }
        Serial.println(F("\r"));
        memcpy(ecap,ecapbytes,MAX_BYTES);
        Serial.print(F("ecap ASCII: "));Serial.println(ecap);
      }
    }
    if(!debugE1CAP) memcpy(ecap,ecapbytes,MAX_BYTES);

    if(substringEquals(ecap,0,3,"Out") && !automatrixSW1){ // store only the input and output states, some Extron devices report output first instead of input
      if(substringEquals(ecap,4,5," ")){
        copySnippet(ecap,5,9,einput); // einput = ecap.substring(5,9);
        if(sliceToInt(ecap,3,4) == ExtronVideoOutputPortSW1) eoutput[0] = 1;
        else eoutput[0] = 0;
      }
      else{
        copySnippet(ecap,6,10,einput); // einput = ecap.substring(6,10);
        if(sliceToInt(ecap,3,5) == ExtronVideoOutputPortSW1) eoutput[0] = 1;
        else eoutput[0] = 0;
      }
    }
    else if(substringEquals(ecap,0,1,"F")){ // detect if switch has changed auto/manual states
      copySnippet(ecap,4,8,einput); // einput = ecap.substring(4,8);
      eoutput[0] = 1;
    }
    else if(substringEquals(ecap,0,3,"Rpr")){ // detect if a Preset has been used
      copySnippet(ecap,0,5,einput); // einput = ecap.substring(0,5);
      eoutput[0] = 1;
    }
    else if(substringEquals(ecap,0,8,"RECONFIG")){     // This is received everytime a change is made on older Extron Crosspoints
      ExtronOutputQuery(ExtronVideoOutputPortSW1,1); // Finds current input for "ExtronVideoOutputPortSW1" that is connected to port 1 of the DD
    }
#if automatrixSW1
    else if(substringEquals(ecap,amSizeSW1 + 6,amSizeSW1 + 9,"Rpr")){ // detect if a Preset has been used 
      copySnippet(ecap,amSizeSW1 + 6,amSizeSW1 + 11,einput); // einput = ecap.substring(amSizeSW1 + 6,amSizeSW1 + 11);
      eoutput[0] = 1;
    }
    else if(substringEquals(ecap,amSizeSW1 + 7,amSizeSW1 + 10,"Rpr")){ // detect if a Preset has been used 
      copySnippet(ecap,amSizeSW1 + 7,amSizeSW1 + 12,einput); // einput = ecap.substring(amSizeSW1 + 7,amSizeSW1 + 12);
      eoutput[0] = 1;
    }
    else if(substringEquals(ecap,0,3,"In0") && !substringEquals(ecap,4,7,"All") && !substringEquals(ecap,5,8,"All")){ // start of automatrix
      if(substringEquals(ecap,0,4,"In00")){
        amSizeSW1 = lengthUpToLineEnding(ecap,MAX_BYTES) - 5;
        copySnippet(ecap,5,amSizeSW1 + 5,einput); // einput = ecap.substring(5,amSizeSW1 + 5);
      }
      else{
        amSizeSW1 = lengthUpToLineEnding(ecap,MAX_BYTES) - 4;
        copySnippet(ecap,4,amSizeSW1 + 4,einput); // einput = ecap.substring(4,amSizeSW1 + 4);
      }
      uint8_t check = readAMstate(einput,amSizeSW1);
      if(check != currentInputSW1){
        currentInputSW1 = check;
        if(currentInputSW1 == 0){
          setTie(currentInputSW1,1);
          sendProfile(0,EXTRON1,1);
        }
        else if(vinMatrix[0] == 1){
          recallPreset(vinMatrix[currentInputSW1],1);
        }
        else if(vinMatrix[0] == 0 || vinMatrix[0] == 2){
          setTie(currentInputSW1,1);          
          sendProfile(currentInputSW1,EXTRON1,1);
        }
      }
    }
    else if(substringEquals(ecap,0,10,"00000000\r\n") || substringEquals(ecap,0,14,"000000000000\r\n")
            || substringEquals(ecap,0,18,"0000000000000000\r\n")
            || substringEquals(ecap,0,26,"000000000000000000000000\r\n")
            || substringEquals(ecap,0,34,"00000000000000000000000000000000\r\n")){
      extronSerial.write(VERB,5); // sets extron matrix switch to Verbose level 3
    } // end of automatrix
#endif
    else{                             // less complex switches only report input status, no output status
      copySnippet(ecap,0,4,einput); // einput = ecap.substring(0,4);
      eoutput[0] = 1;
    }

    // For older Extron Crosspoints, where "RECONFIG" is sent when changes are made, the profile is only changed when a different input is selected for the defined output. (ExtronVideoOutputPortSW1)
    // Without this, the profile would be resent when changes to other outputs are selected.
    if(substringEquals(einput,0,2,"IN")){
      int temp = sliceToInt(einput,2,4); // einput.substring(2,4).toInt();
      if(SVS == 0 && !S0 && temp > 0 && temp < 13 && temp == -1*currentProf) copySnippet("XX00",0,4,einput); // einput = "XX00";
      else if(SVS == 0 && S0 && temp > 0 && temp < 12 && temp == -1*currentProf) copySnippet("XX00",0,4,einput); // einput = "XX00";
      else if(temp == currentProf) copySnippet("XX00",0,4,einput); // einput = "XX00"; 
    }

    // for Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references eoutput[0]
    if((substringEquals(einput,0,2,"IN") && eoutput[0] && !automatrixSW1) || (substringEquals(einput,0,3,"Rpr"))){
      if(substringEquals(einput,0,3,"Rpr")){
        sendProfile(sliceToInt(einput,3,5),EXTRON1,1);
      }
      else if(!substringEquals(einput,0,4,"IN0 ") && !substringEquals(einput,0,4,"In0 ") && !substringEquals(einput,0,4,"In00")){ // for inputs 13-99 (SVS only)
        sendProfile(sliceToInt(einput,2,4),EXTRON1,1);
      }
      else if(substringEquals(einput,0,4,"IN0 ") || substringEquals(einput,0,4,"In0 ") || substringEquals(einput,0,4,"In00")){
        sendProfile(0,EXTRON1,1);
      }
    }

#if !automatrixSW1
    // VIKI Manual Switch Detection (created by: https://github.com/Arthrimus)
    // ** hdmi output must be connected when powering on switch for ITE messages to appear, thus manual button detection working **

    if(millis() - ITEtimer > 1200){  // Timer that disables sending SVS serial commands using the ITE mux data when there has recently been an autoswitch command (prevents duplicate commands)
      listenITE[0] = 1;  // Sets listenITE[0] to 1 so the ITE mux data can be used to send SVS serial commands again
      ITErecv[0] = 0; // Turns off ITErecv[0] so the SVS serial commands are not repeated if an autoswitch command preceeded the ITE commands
      ITEtimer = millis(); // Resets timer to current millis() count to disable this function once the variables have been updated
    }


    if((substringEquals(ecap,0,3,"==>") || substringEquals(ecap,15,18,"==>")) && listenITE[0]){   // checks if the serial command from the VIKI starts with "=" This indicates that the command is an ITE mux status message
      if(substringEquals(ecap,10,11,"P")){        // checks the last value of the IT6635 mux. P3 points to inputs 1,2,3 / P2 points to inputs 4,5,6 / P1 input 7 / P0 input 8
        ITEstatus[0] = sliceToInt(ecap,11,12); // ecap.substring(11,12).toInt();
      }
      if(substringEquals(ecap,25,26,"P")){        // checks the last value of the IT6635 mux. P3 points to inputs 1,2,3 / P2 points to inputs 4,5,6 / P1 input 7 / P0 input 8
        ITEstatus[0] = sliceToInt(ecap,26,27); // ecap.substring(26,27).toInt();
      }
      if(substringEquals(ecap,18,20,">0")){       // checks the value of the IT66535 IC that points to Dev->0. P2 is input 1 / P1 is input 2 / P0 is input 3
        ITEstatus[1] = sliceToInt(ecap,12,13); // ecap.substring(12,13).toInt();
      }
      if(substringEquals(ecap,33,35,">0")){       // checks the value of the IT66535 IC that points to Dev->0. P2 is input 1 / P1 is input 2 / P0 is input 3
        ITEstatus[1] = sliceToInt(ecap,27,28); // ecap.substring(27,28).toInt();
      }
      if(substringEquals(ecap,18,20,">1")){       // checks the value of the IT66535 IC that points to Dev->1. P2 is input 4 / P1 is input 5 / P0 is input 6
        ITEstatus[2] = sliceToInt(ecap,12,13); // ecap.substring(12,13).toInt();
      }
      if(substringEquals(ecap,33,35,">1")){       // checks the value of the IT66535 IC that points to Dev->1. P2 is input 4 / P1 is input 5 / P0 is input 6
        ITEstatus[2] = sliceToInt(ecap,27,28); // ecap.substring(27,28).toInt();
      }

      ITErecv[0] = 1;                             // sets ITErecv[0] to 1 indicating that an ITE message has been received and an SVS command can be sent once the sendtimer elapses
      sendtimer = millis();                    // resets sendtimer to millis()
      ITEtimer = millis();                    // resets ITEtimer to millis()
      MTVprevTime = millis();                 // delays disconnection detection timer so it wont interrupt
    }


    if((millis() - sendtimer > 300) && ITErecv[0]){ // wait 300ms to make sure all ITE messages are received in order to complete ITEstatus
      if(ITEstatus[0] == 3){                   // Checks if port 3 of the IT6635 chip is currently selected
        if(ITEstatus[1] == 2) ITEinputnum[0] = 1;   // Checks if port 2 of the IT66353 DEV0 chip is selected, Sets ITEinputnum[0] to input 1
        else if(ITEstatus[1] == 1) ITEinputnum[0] = 2;   // Checks if port 1 of the IT66353 DEV0 chip is selected, Sets ITEinputnum[0] to input 2
        else if(ITEstatus[1] == 0) ITEinputnum[0] = 3;   // Checks if port 0 of the IT66353 DEV0 chip is selected, Sets ITEinputnum[0] to input 3
      }
      else if(ITEstatus[0] == 2){                 // Checks if port 2 of the IT6635 chip is currently selected
        if(ITEstatus[2] == 2) ITEinputnum[0] = 4;   // Checks if port 2 of the IT66353 DEV1 chip is selected, Sets ITEinputnum[0] to input 4
        else if(ITEstatus[2] == 1) ITEinputnum[0] = 5;   // Checks if port 1 of the IT66353 DEV1 chip is selected, Sets ITEinputnum[0] to input 5
        else if(ITEstatus[2] == 0) ITEinputnum[0] = 6;   // Checks if port 0 of the IT66353 DEV1 chip is selected, Sets ITEinputnum[0] to input 6
      }
      else if(ITEstatus[0] == 1) ITEinputnum[0] = 7;   // Checks if port 1 of the IT6635 chip is currently selected, Sets ITEinputnum[0] to input 7
      else if(ITEstatus[0] == 0) ITEinputnum[0] = 8;   // Checks if port 0 of the IT6635 chip is currently selected, Sets ITEinputnum[0] to input 8

      ITErecv[0] = 0;                              // sets ITErecv[0] to 0 to prevent the message from being resent
      sendtimer = millis();                     // resets sendtimer to millis()
    }

    if(substringEquals(ecap,0,5,"Auto_") || substringEquals(ecap,15,20,"Auto_") || ITEinputnum[0] > 0) MTVddSW1 = true; // enable MT-VIKI disconnection detection if MT-VIKI switch is present

    // for TESmart 4K60 / TESmart 4K30 / MT-VIKI HDMI switch on SW1
    if(ecapbytes[4] == 17 || ecapbytes[3] == 17 || substringEquals(ecap,0,5,"Auto_") || substringEquals(ecap,15,20,"Auto_") || ITEinputnum[0] > 0){
      if(ecapbytes[6] == 22 || ecapbytes[5] == 22 || ecapbytes[11] == 48 || ecapbytes[26] == 48 || ITEinputnum[0] == 1){
        sendProfile(1,EXTRON1,1);
        currentMTVinput[0] = 1;
        MTVdiscon[0] = false;
      }
      else if(ecapbytes[6] == 23 || ecapbytes[5] == 23 || ecapbytes[11] == 49 || ecapbytes[26] == 49 || ITEinputnum[0] == 2){
        sendProfile(2,EXTRON1,1);
        currentMTVinput[0] = 2;
        MTVdiscon[0] = false;
      }
      else if(ecapbytes[6] == 24 || ecapbytes[5] == 24 || ecapbytes[11] == 50 || ecapbytes[26] == 50 || ITEinputnum[0] == 3){
        sendProfile(3,EXTRON1,1);
        currentMTVinput[0] = 3;
        MTVdiscon[0] = false;
      }
      else if(ecapbytes[6] == 25 || ecapbytes[5] == 25 || ecapbytes[11] == 51 || ecapbytes[26] == 51 || ITEinputnum[0] == 4){
        sendProfile(4,EXTRON1,1);
        currentMTVinput[0] = 4;
        MTVdiscon[0] = false;
      }
      else if(ecapbytes[6] == 26 || ecapbytes[5] == 26 || ecapbytes[11] == 52 || ecapbytes[26] == 52 || ITEinputnum[0] == 5){
        sendProfile(5,EXTRON1,1);
        currentMTVinput[0] = 5;
        MTVdiscon[0] = false;
      }
      else if(ecapbytes[6] == 27 || ecapbytes[5] == 27 || ecapbytes[11] == 53 || ecapbytes[26] == 53 || ITEinputnum[0] == 6){
        sendProfile(6,EXTRON1,1);
        currentMTVinput[0] = 6;
        MTVdiscon[0] = false;
      }
      else if(ecapbytes[6] == 28 || ecapbytes[5] == 28 || ecapbytes[11] == 54 || ecapbytes[26] == 54 || ITEinputnum[0] == 7){
        sendProfile(7,EXTRON1,1);
        currentMTVinput[0] = 7;
        MTVdiscon[0] = false;
      }
      else if(ecapbytes[6] == 29 || ecapbytes[5] == 29 || ecapbytes[11] == 55 || ecapbytes[26] == 55 || ITEinputnum[0] == 8){
        sendProfile(8,EXTRON1,1);
        currentMTVinput[0] = 8;
        MTVdiscon[0] = false;
      }
      else if(ecapbytes[6] > 29 && ecapbytes[6] < 38){
        sendProfile(ecapbytes[6] - 21,EXTRON1,1);
      }
      else if(ecapbytes[5] > 29 && ecapbytes[5] < 38){
        sendProfile(ecapbytes[5] - 21,EXTRON1,1);
      }

      if(substringEquals(ecap,0,5,"Auto_") || substringEquals(ecap,15,20,"Auto_")) listenITE[0] = 0; // Sets listenITE[0] to 0 so the ITE mux data will be ignored while an autoswitch command is detected.
      ITEinputnum[0] = 0;                     // Resets ITEinputnum[0] to 0 so sendSVS will not repeat after this cycle through the void loop
      ITEtimer = millis();                 // resets ITEtimer to millis()
      MTVprevTime = millis();              // delays disconnection detection timer so it wont interrupt
    }
   
    // if a MT-VIKI active port disconnection is detected, and then later a reconnection, resend the profile.
    if(substringEquals(ecap,24,41,"IS_NON_INPUT_PORT")){
      if(!MTVdiscon[0]) sendProfile(0,EXTRON1,0);
      MTVdiscon[0] = true;
    }
    else if(!substringEquals(ecap,24,41,"IS_NON_INPUT_PORT") && substringEquals(ecap,0,11,"Uart_RxData") && MTVdiscon[0]){
      MTVdiscon[0] = false;
      sendProfile(currentMTVinput[0],EXTRON1,1);
    }


    // for Otaku Games Scart Switch 1
    if(substringEquals(ecap,0,11,"remote prof")){
      if(substringEquals(ecap,0,13,"remote prof10")){
        sendProfile(10,EXTRON1,1);
      }
      else if(substringEquals(ecap,0,13,"remote prof12")){
        sendProfile(0,EXTRON1,1);
      }
      else if(substringEquals(ecap,12,13,"\r")){
        sendProfile(sliceToInt(ecap,11,12),EXTRON1,1);
      }
      else{
        sendProfile(sliceToInt(ecap,11,13),EXTRON1,1);
      }
    }
#endif

  recordMem();
  memset(ecapbytes,0,sizeof(ecapbytes)); // reset capture to all 0s
  memset(ecap,0,sizeof(ecap));
  memset(einput,0,sizeof(einput));
  
} // end of readExtron1()

void readExtron2(){
    
#if automatrixSW2 // if automatrixSW2 is set "true" in options, then "0LS" is sent every 500ms to see if an input has changed
      LS0time2(500);
#endif

#if !automatrixSW2
    if(MTVddSW2){ // if a MT-VIKI switch has been detected on SW2, then the currently active MT-VIKI hdmi port is checked for disconnection
      MTVtime2(2000);
    }
#endif

    // listens to the Extron sw2 Port for changes
    if(extronSerial2.available() > 0){ // if there is data available for reading, read
    extronSerial2.readBytes(ecapbytes,MAX_BYTES); // read in and store only the first MAX_BYTES bytes for every status message received from 2nd Extron port
      if(debugE2CAP){
        Serial.print(F("ecap2 HEX: "));
        for(uint8_t i=0;i<MAX_BYTES;i++){
          Serial.print(ecapbytes[i],HEX);Serial.print(F(" "));
        }
        Serial.println(F("\r"));
        memcpy(ecap,ecapbytes,MAX_BYTES);
        Serial.print(F("ecap2 ASCII: "));Serial.println(ecap);
      }
    }
    if(!debugE2CAP) memcpy(ecap,ecapbytes,MAX_BYTES);

    if(substringEquals(ecap,0,3,"Out") && !automatrixSW2){ // store only the input and output states, some Extron devices report output first instead of input
      if(substringEquals(ecap,4,5," ")){
        copySnippet(ecap,5,9,einput); // einput = ecap.substring(5,9);
        if(sliceToInt(ecap,3,4) == ExtronVideoOutputPortSW2) eoutput[1] = 1;
        else eoutput[1] = 0;
      }
      else{
        copySnippet(ecap,6,10,einput); // einput = ecap.substring(6,10);
        if(sliceToInt(ecap,3,5) == ExtronVideoOutputPortSW2) eoutput[1] = 1;
        else eoutput[1] = 0;
      }
    }
    else if(substringEquals(ecap,0,1,"F")){ // detect if switch has changed auto/manual states
      copySnippet(ecap,4,8,einput); // einput = ecap.substring(4,8);
      eoutput[1] = 1;
    }
    else if(substringEquals(ecap,0,3,"Rpr")){ // detect if a Preset has been used
      copySnippet(ecap,0,5,einput); // einput = ecap.substring(0,5);
      eoutput[1] = 1;
    }
    else if(substringEquals(ecap,0,8,"RECONFIG")){     // This is received everytime a change is made on older Extron Crosspoints.
      ExtronOutputQuery(ExtronVideoOutputPortSW2,2); // Finds current input for "ExtronVideoOutputPortSW2" that is connected to port 2 of the DD
    }
#if automatrixSW2
    else if(substringEquals(ecap,amSizeSW2 + 6,amSizeSW2 + 9,"Rpr")){ // detect if a Preset has been used 
      copySnippet(ecap,amSizeSW2 + 6,amSizeSW2 + 11,einput); // einput = ecap.substring(amSizeSW2 + 6,amSizeSW2 + 11);
      eoutput[1] = 1;
    }
    else if(substringEquals(ecap,amSizeSW2 + 7,amSizeSW2 + 10,"Rpr")){ // detect if a Preset has been used 
      copySnippet(ecap,amSizeSW2 + 7,amSizeSW2 + 12,einput); // einput = ecap.substring(amSizeSW2 + 7,amSizeSW2 + 12);
      eoutput[1] = 1;
    }
    else if(substringEquals(ecap,0,3,"In0") && !substringEquals(ecap,4,7,"All") && !substringEquals(ecap,5,8,"All")){ // start of automatrix
      if(substringEquals(ecap,0,4,"In00")){
        amSizeSW2 = lengthUpToLineEnding(ecap,MAX_BYTES) - 5;
        copySnippet(ecap,5,amSizeSW2 + 5,einput); // einput = ecap.substring(5,amSizeSW2 + 5);
      }
      else{
        amSizeSW2 = lengthUpToLineEnding(ecap,MAX_BYTES) - 4;
        copySnippet(ecap,4,amSizeSW2 + 4,einput); // einput = ecap.substring(4,amSizeSW2 + 4);
      }
      uint8_t check2 = readAMstate(einput,amSizeSW2);
      if(check2 != currentInputSW2){
        currentInputSW2 = check2;
        if(currentInputSW2 == 0){
          setTie(currentInputSW2,2);
          sendProfile(0,EXTRON2,1);
        }
        else if(vinMatrix[0] == 1){
          recallPreset(vinMatrix[currentInputSW2 + 32],2);
        }
        else if(vinMatrix[0] == 0 || vinMatrix[0] == 2){
          setTie(currentInputSW2,2);          
          sendProfile(currentInputSW2 + 100,EXTRON2,1);
        }
      }
    }
    else if(substringEquals(ecap,0,10,"00000000\r\n") || substringEquals(ecap,0,14,"000000000000\r\n")
            || substringEquals(ecap,0,18,"0000000000000000\r\n") 
            || substringEquals(ecap,0,26,"000000000000000000000000\r\n")
            || substringEquals(ecap,0,34,"00000000000000000000000000000000\r\n")){
      extronSerial2.write(VERB,5); // sets extron matrix switch to Verbose level 3
    } // end of automatrix
#endif
    else{                              // less complex switches only report input status, no output status
      copySnippet(ecap,0,4,einput); // einput = ecap.substring(0,4);
      eoutput[1] = 1;
    }

    // For older Extron Crosspoints, where "RECONFIG" is sent when changes are made, the profile is only changed when a different input is selected for the defined output. (ExtronVideoOutputPortSW2)
    // Without this, the profile would be resent when changes to other outputs are selected.
    if(substringEquals(einput,0,2,"IN") && sliceToInt(einput,2,4)+100 == currentProf) copySnippet("XX00",0,4,einput); // einput = "XX00";

    // For Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references eoutput[1]
    if((substringEquals(einput,0,2,"IN") && eoutput[1] && !automatrixSW2) || substringEquals(einput,0,3,"Rpr")){
      if(substringEquals(einput,0,3,"Rpr")){
        sendProfile(sliceToInt(einput,3,5)+100,EXTRON2,1);
      }
      else if(!substringEquals(einput,0,4,"IN0 ") && !substringEquals(einput,0,4,"In0 ") && !substringEquals(einput,0,4,"In00")){ // much easier method for switch 2 since ALL inputs will respond with SVS commands regardless of SVS option above
        sendProfile(sliceToInt(einput,2,4)+100,EXTRON2,1);
      }
      else if(substringEquals(einput,0,4,"IN0 ") || substringEquals(einput,0,4,"In0 ") || substringEquals(einput,0,4,"In00")){
        sendProfile(0,EXTRON2,1);
      }

    }


  #if !automatrixSW2
    // VIKI Manual Switch Detection (created by: https://github.com/Arthrimus)
    // ** hdmi output must be connected when powering on switch for ITE messages to appear, thus manual button detection working **

    if(millis() - ITEtimer2 > 1200){  // Timer that disables sending SVS serial commands using the ITE mux data when there has recently been an autoswitch command (prevents duplicate commands)
      listenITE[1] = 1;  // Sets listenITE[1] to 1 so the ITE mux data can be used to send SVS serial commands again
      ITErecv[1] = 0; // Turns off ITErecv[1] so the SVS serial commands are not repeated if an autoswitch command preceeded the ITE commands
      ITEtimer2 = millis(); // Resets timer to current millis() count to disable this function once the variables hav been updated
    }


    if((substringEquals(ecap,0,3,"==>") || substringEquals(ecap,15,18,"==>")) && listenITE[1]){   // checks if the serial command from the VIKI starts with "=" This indicates that the command is an ITE mux status message
      if(substringEquals(ecap,10,11,"P")){       // checks the last value of the IT6635 mux. P3 points to inputs 1,2,3 / P2 points to inputs 4,5,6 / P1 input 7 / P0 input 8
        ITEstatus2[0] = sliceToInt(ecap,11,12); // ecap.substring(11,12).toInt();
      }
      if(substringEquals(ecap,25,26,"P")){       // checks the last value of the IT6635 mux. P3 points to inputs 1,2,3 / P2 points to inputs 4,5,6 / P1 input 7 / P0 input 8
        ITEstatus2[0] = sliceToInt(ecap,26,27); // ecap.substring(26,27).toInt();
      }
      if(substringEquals(ecap,18,20,">0")){       // checks the value of the IT66535 IC that points to Dev->0. P2 is input 1 / P1 is input 2 / P0 is input 3
        ITEstatus2[1] = sliceToInt(ecap,12,13); // ecap.substring(12,13).toInt();
      }
      if(substringEquals(ecap,33,35,">0")){       // checks the value of the IT66535 IC that points to Dev->0. P2 is input 1 / P1 is input 2 / P0 is input 3
        ITEstatus2[1] = sliceToInt(ecap,27,28); // ecap.substring(27,28).toInt();
      }
      if(substringEquals(ecap,18,20,">1")){       // checks the value of the IT66535 IC that points to Dev->1. P2 is input 4 / P1 is input 5 / P0 is input 6
        ITEstatus2[2] = sliceToInt(ecap,12,13); // ecap.substring(12,13).toInt();
      }
      if(substringEquals(ecap,33,35,">1")){       // checks the value of the IT66535 IC that points to Dev->1. P2 is input 4 / P1 is input 5 / P0 is input 6
        ITEstatus2[2] = sliceToInt(ecap,27,28); // ecap.substring(27,28).toInt();
      }
      ITErecv[1] = 1;                             // sets ITErecv[1] to 1 indicating that an ITE message has been received and an SVS command can be sent once the sendtimer elapses
      sendtimer2 = millis();                    // resets sendtimer2 to millis()
      ITEtimer2 = millis();                    // resets ITEtimer2 to millis()
      MTVprevTime2 = millis();                 // delays disconnection detection timer so it wont interrupt
    }


    if((millis() - sendtimer2 > 300) && ITErecv[1]){ // wait 300ms to make sure all ITE messages are received in order to complete ITEstatus
      if(ITEstatus2[0] == 3){                   // Checks if port 3 of the IT6635 chip is currently selected
        if(ITEstatus2[1] == 2) ITEinputnum[1] = 1;   // Checks if port 2 of the IT66353 DEV0 chip is selected, Sets ITEinputnum to input 1
        else if(ITEstatus2[1] == 1) ITEinputnum[1] = 2;   // Checks if port 1 of the IT66353 DEV0 chip is selected, Sets ITEinputnum to input 2
        else if(ITEstatus2[1] == 0) ITEinputnum[1] = 3;   // Checks ITEstatus array position` 1 to determine if port 0 of the IT66353 DEV0 chip is selected, Sets ITEinputnum to input 3
      }
      else if(ITEstatus2[0] == 2){                 // Checks if port 2 of the IT6635 chip is currently selected
        if(ITEstatus2[2] == 2) ITEinputnum[1] = 4;   // Checks if port 2 of the IT66353 DEV1 chip is selected, Sets ITEinputnum to input 4
        else if(ITEstatus2[2] == 1) ITEinputnum[1] = 5;   // Checks if port 1 of the IT66353 DEV1 chip is selected, Sets ITEinputnum to input 5
        else if(ITEstatus2[2] == 0) ITEinputnum[1] = 6;   // Checks if port 0 of the IT66353 DEV1 chip is selected, Sets ITEinputnum to input 6
      }
      else if(ITEstatus2[0] == 1) ITEinputnum[1] = 7;   // Checks if port 1 of the IT6635 chip is currently selected, Sets ITEinputnum to input 7
      else if(ITEstatus2[0] == 0) ITEinputnum[1] = 8;   // Checks if port 0 of the IT6635 chip is currently selected, Sets ITEinputnum to input 8

      ITErecv[1] = 0;                              // sets ITErecv[1] to 0 to prevent the message from being resent
      sendtimer2 = millis();                     // resets sendtimer2 to millis()
    }

    if(substringEquals(ecap,0,5,"Auto_") || substringEquals(ecap,15,20,"Auto_") || ITEinputnum[1] > 0) MTVddSW2 = true; // enable MT-VIKI disconnection detection if MT-VIKI switch is present


    // for TESmart 4K60 / TESmart 4K30 / MT-VIKI HDMI switch on SW2
    if(ecapbytes[4] == 17 || ecapbytes[3] == 17 || substringEquals(ecap,0,5,"Auto_") || substringEquals(ecap,15,20,"Auto_") || ITEinputnum[1] > 0){
      if(ecapbytes[6] == 22 || ecapbytes[5] == 22 || ecapbytes[11] == 48 || ecapbytes[26] == 48 || ITEinputnum[1] == 1){
        sendProfile(101,EXTRON2,1);
        currentMTVinput[1] = 101;
        MTVdiscon[1] = false;
      }
      else if(ecapbytes[6] == 23 || ecapbytes[5] == 23 || ecapbytes[11] == 49 || ecapbytes[26] == 49 || ITEinputnum[1] == 2){
        sendProfile(102,EXTRON2,1);
        currentMTVinput[1] = 102;
        MTVdiscon[1] = false;
      }
      else if(ecapbytes[6] == 24 || ecapbytes[5] == 24 || ecapbytes[11] == 50 || ecapbytes[26] == 50 || ITEinputnum[1] == 3){
        sendProfile(103,EXTRON2,1);
        currentMTVinput[1] = 103;
        MTVdiscon[1] = false;
      }
      else if(ecapbytes[6] == 25 || ecapbytes[5] == 25 || ecapbytes[11] == 51 || ecapbytes[26] == 51 || ITEinputnum[1] == 4){
        sendProfile(104,EXTRON2,1);
        currentMTVinput[1] = 104;
        MTVdiscon[1] = false;
      }
      else if(ecapbytes[6] == 26 || ecapbytes[5] == 26 || ecapbytes[11] == 52 || ecapbytes[26] == 52 || ITEinputnum[1] == 5){
        sendProfile(105,EXTRON2,1);
        currentMTVinput[1] = 105;
        MTVdiscon[1] = false;
      }
      else if(ecapbytes[6] == 27 || ecapbytes[5] == 27 || ecapbytes[11] == 53 || ecapbytes[26] == 53 || ITEinputnum[1] == 6){
        sendProfile(106,EXTRON2,1);
        currentMTVinput[1] = 106;
        MTVdiscon[1] = false;
      }
      else if(ecapbytes[6] == 28 || ecapbytes[5] == 28 || ecapbytes[11] == 54 || ecapbytes[26] == 54 || ITEinputnum[1] == 7){
        sendProfile(107,EXTRON2,1);
        currentMTVinput[1] = 107;
        MTVdiscon[1] = false;
      }
      else if(ecapbytes[6] == 29 || ecapbytes[5] == 29 || ecapbytes[11] == 55 || ecapbytes[26] == 55 || ITEinputnum[1] == 8){
        sendProfile(108,EXTRON2,1);
        currentMTVinput[1] = 108;
        MTVdiscon[1] = false;
      }
      else if(ecapbytes[6] > 29 && ecapbytes[6] < 38){
        sendProfile(ecapbytes[6] + 79,EXTRON2,1);
      }
      else if(ecapbytes[5] > 29 && ecapbytes[5] < 38){
        sendProfile(ecapbytes[5] + 79,EXTRON2,1);
      }

      if(substringEquals(ecap,0,5,"Auto_") || substringEquals(ecap,15,20,"Auto_")) listenITE[1] = 0; // Sets listenITE[1] to 0 so the ITE mux data will be ignored while an autoswitch command is detected.
      ITEinputnum[1] = 0;                     // Resets ITEinputnum[1] to 0 so sendSVS will not repeat after this cycle through the void loop
      ITEtimer2 = millis();                 // resets ITEtimer2 to millis()
      MTVprevTime2 = millis();              // delays disconnection detection timer so it wont interrupt
    }

    // if a MT-VIKI active port disconnection is detected, and then later a reconnection, resend the profile.
    if(substringEquals(ecap,24,41,"IS_NON_INPUT_PORT")){
      if(!MTVdiscon[1]) sendProfile(0,EXTRON2,0);
      MTVdiscon[1] = true;
    }
    else if(!substringEquals(ecap,24,41,"IS_NON_INPUT_PORT") && substringEquals(ecap,0,11,"Uart_RxData") && MTVdiscon[1]){
      MTVdiscon[1] = false;
      sendProfile(currentMTVinput[1],EXTRON2,1);
    }

    
    // for Otaku Games Scart Switch 2
    if(substringEquals(ecap,0,11,"remote prof")){
      if(substringEquals(ecap,0,13,"remote prof10")){
        sendProfile(110,EXTRON2,1);
      }
      else if(substringEquals(ecap,0,13,"remote prof12")){
        sendProfile(0,EXTRON2,1);
      }
      else if(substringEquals(ecap,12,13,"\r")){
        sendProfile(sliceToInt(ecap,11,12)+100,EXTRON2,1);
      }
      else{
        sendProfile(sliceToInt(ecap,11,13)+100,EXTRON2,1);
      }
    }
  #endif

  recordMem();
  memset(ecapbytes,0,sizeof(ecapbytes)); // reset capture to 0s
  memset(ecap,0,sizeof(ecap));
  memset(einput,0,sizeof(einput));

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
    if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 0))sendProfile(201,GSCART1,0); // 0 0 0
    else if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 1))sendProfile(202,GSCART1,0); // 0 0 1
    else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 0))sendProfile(203,GSCART1,0); // 0 1 0
    else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 1))sendProfile(204,GSCART1,0); // 0 1 1
    else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 0))sendProfile(205,GSCART1,0); // 1 0 0
    else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 1))sendProfile(206,GSCART1,0); // 1 0 1
    else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 0))sendProfile(207,GSCART1,0); // 1 1 0
    else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 1))sendProfile(208,GSCART1,0); // 1 1 1
    
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
    sendProfile(0,GSCART1,0);
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

  if(samcc[0] < samsize) samcc[0]++;// increment counter until "samsize" has been reached then reset counter and "highcount[0]"
  else{
    samcc[0] = 1;
    memset(highcount[0],0,sizeof(highcount[0]));
  }

  recordMem();
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
    if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 0))sendProfile(209,GSCART2,0); // 0 0 0
    else if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 1))sendProfile(210,GSCART2,0); // 0 0 1
    else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 0))sendProfile(211,GSCART2,0); // 0 1 0
    else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 1))sendProfile(212,GSCART2,0); // 0 1 1
    else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 0))sendProfile(213,GSCART2,0); // 1 0 0
    else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 1))sendProfile(214,GSCART2,0); // 1 0 1
    else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 0))sendProfile(215,GSCART2,0); // 1 1 0
    else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 1))sendProfile(216,GSCART2,0); // 1 1 1

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
    sendProfile(0,GSCART2,0);
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

  if(samcc[1] < samsize) samcc[1]++;
  else{
    samcc[1] = 1;
    memset(highcount[1],0,sizeof(highcount[1]));
  }

  recordMem();
} // end readGscart2()

void readIR(){

  uint8_t ir_recv_command = 0;
  uint8_t ir_recv_address = 0;

  if(TinyReceiverDecode()){

    ir_recv_command = TinyIRReceiverData.Command;
    ir_recv_address = TinyIRReceiverData.Address;


    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && extrabuttonprof == 2){
      if(ir_recv_command == 11){ // profile button 1
        svsbutton[nument] = '1';
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        svsbutton[nument] = '2';
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
        svsbutton[nument] = '3';
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        svsbutton[nument] = '4';
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        svsbutton[nument] = '5';
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        svsbutton[nument] = '6';
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        svsbutton[nument] = '7';
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        svsbutton[nument] = '8';
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 1){ // profile button 9
        svsbutton[nument] = '9';
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 37 || ir_recv_command == 38 || ir_recv_command == 39){ // profile buttons 10,11,12
        svsbutton[nument] = '0';
        nument++;
        ir_recv_command = 0;
      }
      else if(ir_recv_command == 26){ // if you accidentally hit the aux8 button 2x + power, still power toggle tv
        sendIR(auxpower,0,1); 
        ir_recv_command = 0;
        memset(svsbutton,0,sizeof(svsbutton));
        nument = 0;
      }
      else{
        extrabuttonprof = 0;
        memset(svsbutton,0,sizeof(svsbutton));
        nument = 0;
      }

      
    } // end of extrabutton == 2

    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && extrabuttonprof == 1){ // if AUX8 was pressed and a profile button is pressed next,
      if(ir_recv_command == 11){ // profile button 1                                                         // load "SVS" profiles 1 - 12 (profile button 1 - 12).
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[0]);                                                               // Can be changed to "ANY" SVS profile in the OPTIONS section
        if(MTVir == 1){extronSerialEwrite("viki",1,1);}
        if(MTVir == 2){extronSerialEwrite("viki",1,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",1,2);sendSVS(101);}                                                                   
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[1]);
        if(MTVir == 1){extronSerialEwrite("viki",2,1);}
        if(MTVir == 2){extronSerialEwrite("viki",2,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",2,2);sendSVS(102);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[2]);
        if(MTVir == 1){extronSerialEwrite("viki",3,1);}
        if(MTVir == 2){extronSerialEwrite("viki",3,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",3,2);sendSVS(103);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[3]);
        if(MTVir == 1){extronSerialEwrite("viki",4,1);}
        if(MTVir == 2){extronSerialEwrite("viki",4,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",4,2);sendSVS(104);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[4]);
        if(MTVir == 1){extronSerialEwrite("viki",5,1);}
        if(MTVir == 2){extronSerialEwrite("viki",5,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",5,2);sendSVS(105);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[5]);
        if(MTVir == 1){extronSerialEwrite("viki",6,1);}
        if(MTVir == 2){extronSerialEwrite("viki",6,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",6,2);sendSVS(106);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[6]);
        if(MTVir == 1){extronSerialEwrite("viki",7,1);}
        if(MTVir == 2){extronSerialEwrite("viki",7,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",7,2);sendSVS(107);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[7]);
        if(MTVir == 1){extronSerialEwrite("viki",8,1);}
        if(MTVir == 2){extronSerialEwrite("viki",8,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",8,2);sendSVS(108);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 1){ // profile button 9
        if(TESmartir < 2)sendSVS(auxprof[8]);
        if(TESmartir > 1){extronSerialEwrite("tesmart",9,2);sendSVS(109);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 37){ // profile button 10
        if(TESmartir < 2)sendSVS(auxprof[9]);
        if(TESmartir > 1){extronSerialEwrite("tesmart",10,2);sendSVS(110);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 38){ // profile button 11
        if(TESmartir < 2)sendSVS(auxprof[10]);
        if(TESmartir > 1){extronSerialEwrite("tesmart",11,2);sendSVS(111);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 39){ // profile button 12
        if(TESmartir < 2)sendSVS(auxprof[11]);
        if(TESmartir > 1){extronSerialEwrite("tesmart",12,2);sendSVS(112);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 56){ // aux1 button
        if(TESmartir > 1){extronSerialEwrite("tesmart",13,2);sendSVS(113);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 57){ // aux2 button
        if(TESmartir > 1){extronSerialEwrite("tesmart",14,2);sendSVS(114);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 58){ // aux3 button
        if(TESmartir > 1){extronSerialEwrite("tesmart",15,2);sendSVS(115);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 59){ // aux4 button
        if(TESmartir > 1){extronSerialEwrite("tesmart",16,2);sendSVS(116);}
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 26){ // Power button
        sendIR(auxpower,0,0);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 83){ // ok button
        Serial.print(F("Max mem used: "));
        Serial.print((long)100 * (2048 - minFreeRam) / 2048);
        Serial.println(F("%"));
        unsigned long totalMinutes = millis() / 60000UL;
        unsigned int days = totalMinutes / 1440UL;
        unsigned int hours = (totalMinutes / 60UL) % 24;
        byte minutes = totalMinutes % 60;
        Serial.print(F("Uptime: "));
        Serial.print(days);
        Serial.print(F("d "));
        Serial.print(hours);
        Serial.print(F("h "));
        Serial.print(minutes);
        Serial.println(F("m"));
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 63){
        ir_recv_command = 0;
        extrabuttonprof++;
      }
      else{
        extrabuttonprof = 0;
        memset(svsbutton,0,sizeof(svsbutton));
        nument = 0;
      }
      
    } // end extrabuttonprof == 1


    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && auxTESmart == 1){ // if AUX7 was pressed and a profile button is pressed next
      if(ir_recv_command == 11){ // profile button 1
        extronSerialEwrite("tesmart",1,1);sendSVS(1);                                                                    
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        extronSerialEwrite("tesmart",2,1);sendSVS(2); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
        extronSerialEwrite("tesmart",3,1);sendSVS(3); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        extronSerialEwrite("tesmart",4,1);sendSVS(4); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        extronSerialEwrite("tesmart",5,1);sendSVS(5); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        extronSerialEwrite("tesmart",6,1);sendSVS(6); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        extronSerialEwrite("tesmart",7,1);sendSVS(7); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        extronSerialEwrite("tesmart",8,1);sendSVS(8); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 1){ // profile button 9
        extronSerialEwrite("tesmart",9,1);sendSVS(9); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 37){ // profile button 10
        extronSerialEwrite("tesmart",10,1);sendSVS(10); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 38){ // profile button 11
        extronSerialEwrite("tesmart",11,1);sendSVS(11); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 39){ // profile button 12
        extronSerialEwrite("tesmart",12,1);sendSVS(12); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 56){ // aux1 button
        extronSerialEwrite("tesmart",13,1);sendSVS(13); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 57){ // aux2 button
        extronSerialEwrite("tesmart",14,1);sendSVS(14); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 58){ // aux3 button
        extronSerialEwrite("tesmart",15,1);sendSVS(15); 
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }
      else if(ir_recv_command == 59){ // aux4 button
        extronSerialEwrite("tesmart",16,1);sendSVS(16);
        ir_recv_command = 0;
        extrabuttonprof = 0;
      }

      auxTESmart = 0;
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
      else if(ir_recv_command == 60){ // button aux5
        if(auxgsw[0])Serial.println(F("\rremote aud\r"));
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
      sendSVS(sliceToInt(svsbutton,0,3));
      nument = 0;
      memset(svsbutton,0,sizeof(svsbutton));
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
        if(TESmartir == 1 || TESmartir == 3)auxTESmart = 1;
        else Serial.println(F("\rremote aux7\r"));
      }
      else if(ir_recv_command == 61){
        if(gctl)auxgsw[1] = 1;
        else Serial.println(F("\rremote aux6\r"));
      }
      else if(ir_recv_command == 60){
        if(gctl)auxgsw[0] = 1;
        else Serial.println(F("\rremote aud\r")); // remote aux5
      }
      else if(ir_recv_command == 59){
        Serial.println(F("\rremote col\r")); // remote aux4
      }
      else if(ir_recv_command == 58){
        Serial.println(F("\rremote aux3\r")); // remote aux3
      }
      else if(ir_recv_command == 57){
        Serial.println(F("\rremote aux2\r")); // remote aux2
      }
      else if(ir_recv_command == 56){
        Serial.println(F("\rremote aux1\r")); // remote aux1
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


    // if(ir_recv_address == 124 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT){ // OSSC remote, nothing defined atm. (124 dec is 7C hex)
    //   if(ir_recv_command == 148){} // 1 button
    //   else if(ir_recv_command == 149){} // 2 button
    //   else if(ir_recv_command == 150){} // 3 button
    //   else if(ir_recv_command == 151){} // 4 button
    //   else if(ir_recv_command == 152){} // 5 button
    //   else if(ir_recv_command == 153){} // 6 button
    //   else if(ir_recv_command == 154){} // 7 button
    //   else if(ir_recv_command == 155){} // 8 button
    //   else if(ir_recv_command == 156){} // 9 button
    //   else if(ir_recv_command == 147){} // 0 button
    //   else if(ir_recv_command == 157){} // 10+ --/- button
    //   else if(ir_recv_command == 158){} // return <__S--> button
    //   else if(ir_recv_command == 180){} // up button
    //   else if(ir_recv_command == 179){} // down button
    //   else if(ir_recv_command == 181){} // left button
    //   else if(ir_recv_command == 182){} // right button
    //   else if(ir_recv_command == 184){} // ok button
    //   else if(ir_recv_command == 178){} // menu button
    //   else if(ir_recv_command == 173){} // L/R button
    //   else if(ir_recv_command == 139){} // cancel / PIC & clock & zoom button
    //   else if(ir_recv_command == 183){} // exit button
    //   else if(ir_recv_command == 166){} // info button
    //   else if(ir_recv_command == 131){} // red pause button
    //   else if(ir_recv_command == 130){} // green play button
    //   else if(ir_recv_command == 133){} // yellow stop button
    //   else if(ir_recv_command == 177){} // blue eject button
    //   else if(ir_recv_command == 186){} // >>| next button
    //   else if(ir_recv_command == 185){} // |<< previous button
    //   else if(ir_recv_command == 134){} // << button
    //   else if(ir_recv_command == 135){} // >> button
    //   else if(ir_recv_command == 128){} // Power button
    // }
    // else if(ir_recv_address == 122 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT){ // 122 is 7A hex
    //   if(ir_recv_command == 27){} // tone- button
    //   else if(ir_recv_command == 26){} // tone+ button
    // }
    // else if(ir_recv_address == 56 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT){ // 56 is 38 hex
    //   if(ir_recv_command == 14){} // vol+ button
    //   else if(ir_recv_command == 15){} // vol- button
    //   else if(ir_recv_command == 10){} // ch+ button
    //   else if(ir_recv_command == 11){} // ch- button
    //   else if(ir_recv_command == 18){} // p.n.s. button
    //   else if(ir_recv_command == 19){} // tv/av button
    //   else if(ir_recv_command == 24){} // mute speaker button
    //   //else if(ir_recv_command == ){} // TV button , I dont believe these are NEC based
    //   //else if(ir_recv_command == ){} // SAT / DVB button ... ^^^
    //   //else if(ir_recv_command == ){} // DVD / HIFI button ... ^^^
      
    // } // end of OSSC remote
    
  } // end of TinyReceiverDecode()
  recordMem();
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
      sendProfile(200 + port,GSCART1,1);
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
        sendProfile(200 + port,GSCART2,1);
      }
      lastginput = port;
    }
    else if(port == 10){
      digitalWrite(A5,LOW);digitalWrite(A4,LOW);digitalWrite(A3,HIGH); // 001
      if(lastginput == port){
        sendProfile(200 + port,GSCART2,1);
      }
      lastginput = port;
    }
    else if(port == 11){
      digitalWrite(A5,LOW);digitalWrite(A4,HIGH);digitalWrite(A3,LOW); // 010
      if(lastginput == port){
        sendProfile(200 + port,GSCART2,1);
      }
      lastginput = port;
    }
    else if(port == 12){
      digitalWrite(A5,LOW);digitalWrite(A4,HIGH);digitalWrite(A3,HIGH); // 011
      if(lastginput == port){
        sendProfile(200 + port,GSCART2,1);
      }
      lastginput = port;
    }
    else if(port == 13){
      digitalWrite(A5,HIGH);digitalWrite(A4,LOW);digitalWrite(A3,LOW); // 100
      if(lastginput == port)sendProfile(200 + port,GSCART2,1);
      lastginput = port;
    }
    else if(port == 14){
      digitalWrite(A5,HIGH);digitalWrite(A4,LOW);digitalWrite(A3,HIGH); // 101
      if(lastginput == port)sendProfile(200 + port,GSCART2,1);
      lastginput = port;
    }
    else if(port == 15){
      digitalWrite(A5,HIGH);digitalWrite(A4,HIGH);digitalWrite(A3,LOW); // 110
      if(lastginput == port)sendProfile(200 + port,GSCART2,1);
      lastginput = port;
    }
    else if(port == 16){
      digitalWrite(A5,HIGH);digitalWrite(A4,HIGH);digitalWrite(A3,HIGH); // 111
      if(lastginput == port)sendProfile(200 + port,GSCART2,1);
      lastginput = port;
    }
  }
  recordMem();
} // end of overrideGscart()

void sendSVS(uint16_t num){
  Serial.print(F("\rSVS NEW INPUT="));
  if(num != 0)Serial.print(num + offset);
  else Serial.print(num);
  Serial.println(F("\r"));
  delay(1000);
  Serial.print(F("\rSVS CURRENT INPUT="));
  if(num != 0)Serial.print(num + offset);
  else Serial.print(num);
  Serial.println(F("\r"));
  currentProf = num;
  recordMem();
}

void sendRBP(int prof){ // send Remote Button Profile
  Serial.print(F("\rremote prof"));
  Serial.print(prof);
  Serial.println(F("\r"));
  currentProf = -1*prof; // always store remote button profiles as negative numbers
  recordMem();
}

void sendIR(const char* type, uint8_t prof, uint8_t repeat){
  IRsend irsend;
  if(substringEquals(type,0,2,"5X")){ // RT5X
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
  else if(substringEquals(type,0,2,"4K")){ // RT4K
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
  else if(substringEquals(type,0,4,"OSSC")){ // OSSC
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
  else if(substringEquals(type,0,2,"LG")){ // LG CX OLED TV
    irsend.sendNEC(0x04,0x08,0); // Power button
    irsend.sendNEC(0x00,0x00,0);
    irsend.sendNEC(0x00,0x00,0);
    irsend.sendNEC(0x00,0x00,0);
    delay(50);
    irsend.sendNEC(0x04,0x08,0); // send once more
    irsend.sendNEC(0x00,0x00,0);
    irsend.sendNEC(0x00,0x00,0);
    irsend.sendNEC(0x00,0x00,0);
  }
  recordMem();
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
    if((SVS == 1 && currentProf != 0) || ((SVS == 0 || SVS == 2) && currentProf != -12)){
      if(currentProf > 0)
        sendSVS(currentProf);
      else
        sendRBP(-1*currentProf);
    }
  }
  if(currentTime - prevBlinkTime >= 300){
    prevBlinkTime = currentTime;
    if(digitalRead(LED_BUILTIN) == LOW)digitalWrite(LED_BUILTIN,HIGH);
    else digitalWrite(LED_BUILTIN,LOW);
  }
  recordMem();
} // end of sendRTwake()

void LS0time1(unsigned long eTime){
  LScurrentTime = millis();  // Init timer
  if(LSprevTime == 0)       // If previous timer not initialized, do so now.
    LSprevTime = millis();
  if((LScurrentTime - LSprevTime) >= eTime){ // If it's been longer than eTime, send "0LS" and reset the timer.
    LScurrentTime = 0;
    LSprevTime = 0;
    extronSerial.print(F("0LS"));
  }
  recordMem();
}  // end of LS0time1()

void LS0time2(unsigned long eTime){
  LScurrentTime2 = millis();  // Init timer
  if(LSprevTime2 == 0)       // If previous timer not initialized, do so now.
    LSprevTime2 = millis();
  if((LScurrentTime2 - LSprevTime2) >= eTime){ // If it's been longer than eTime, send "0LS" and reset the timer.
    LScurrentTime2 = 0;
    LSprevTime2 = 0;
    extronSerial2.print(F("0LS"));
  }
  recordMem();
}  // end of LS0time2()

void setTie(uint8_t num, uint8_t sw){
  if(sw == 1){
    if(vinMatrix[0] == 0){
      extronSerial.print(num);
      extronSerial.print(F("*"));
      extronSerial.print(F("!"));
    }
    else if(vinMatrix[0] == 2){
      extronSerial.print(num);
      extronSerial.print(F("*"));
      extronSerial.print(ExtronVideoOutputPortSW1);
      extronSerial.print(F("!"));
    }
  }
  else if(sw == 2){
    if(vinMatrix[0] == 0){
      extronSerial2.print(num);
      extronSerial2.print(F("*"));
      extronSerial2.print(F("!"));
    }
    else if(vinMatrix[0] == 2){
      extronSerial2.print(num);
      extronSerial2.print(F("*"));
      extronSerial2.print(ExtronVideoOutputPortSW2);
      extronSerial2.print(F("!"));
    }
  }
  recordMem();
} // end of setTie()

void recallPreset(uint8_t num, uint8_t sw){
  if(sw == 1){
    extronSerial.print(num);
    extronSerial.print(F("."));
  }
  else if(sw == 2){
    extronSerial2.print(num);
    extronSerial2.print(F("."));
  }
  recordMem();
} // end of recallPreset()

#if !automatrixSW1
void MTVtime1(unsigned long eTime){
  MTVcurrentTime = millis();  // Init timer
  if(MTVprevTime == 0)       // If previous timer not initialized, do so now.
    MTVprevTime = millis();
  if((MTVcurrentTime - MTVprevTime) >= eTime){ // If it's been longer than eTime, send MT-VIKI serial command for current input, see if it responds with disconnected, and reset the timer.
    MTVcurrentTime = 0;
    MTVprevTime = 0;
    extronSerialEwrite("viki",currentMTVinput[0],1);
  }
  recordMem();
}  // end of MTVtime1()
#endif

#if !automatrixSW2
void MTVtime2(unsigned long eTime){
  MTVcurrentTime2 = millis();  // Init timer
  if(MTVprevTime2 == 0)       // If previous timer not initialized, do so now.
    MTVprevTime2 = millis();
  if((MTVcurrentTime2 - MTVprevTime2) >= eTime){ // If it's been longer than eTime, send MT-VIKI serial command for current input, see if it responds with disconnected, and reset the timer.
    MTVcurrentTime2 = 0;
    MTVprevTime2 = 0;
    extronSerialEwrite("viki",currentMTVinput[1] - 100,2);
  }
  recordMem();
} // end of MTVtime2()
#endif

void ExtronOutputQuery(uint8_t outputNum, uint8_t sw){
  char cmd[6]; 
  uint8_t len = 0;
  cmd[len++] = 'v';
  char buff[4];
  itoa(outputNum,buff,10);
  for(char* p = buff; *p; p++){
    cmd[len++] = *p;
  }
  cmd[len++] = '%';
  if(sw == 1)
    extronSerial.write((uint8_t *)cmd,len);
  else if(sw == 2)
    extronSerial2.write((uint8_t *)cmd,len);
  recordMem();
} // end of ExtronOutputQuery()

void extronSerialEwrite(const char* type, uint8_t value, uint8_t sw){
  if(substringEquals(type,0,4,"viki")){
    viki[2] = byte(value - 1);
    if(sw == 1)
      extronSerial.write(viki,4);
    else if(sw == 2)
      extronSerial2.write(viki,4);
  }
  else if(substringEquals(type,0,7,"tesmart")){
    tesmart[4] = byte(value);
    if(sw == 1)
      extronSerial.write(tesmart,6);
    else if(sw == 2)
      extronSerial2.write(tesmart,6);
  }
  recordMem();
} // end of extronSerialEwrite()

void sendProfile(int sprof, uint8_t sname, uint8_t soverride){
  if(sprof != 0){
    mswitch[sname].On = 1;
    if(SVS == 0 && sname == EXTRON1 && sprof > 0 && sprof < 12){ // save RBP as negative number
      mswitch[sname].Prof = -1*sprof;
    }
    else if(SVS == 2 && (sname == GSCART1 || sname == GSCART2) && sprof > 200 && sprof < 213){
      mswitch[sname].Prof = -1*(sprof - 200);
    }
    else{
      mswitch[sname].Prof = sprof;
    }
    if(mswitch[sname].Prof == currentProf && !soverride) return;
    uint8_t prevOrder = mswitch[sname].Order;
    for(uint8_t i = 0;i < mswitchSize;i++){
      if(i == sname){
        mswitch[i].Order = 0;
        mswitch[i].King = 1;
      }
      else{
        mswitch[i].King = 0;
        if(mswitch[i].Order < prevOrder) mswitch[i].Order++;
      }
    }

    if(sname == EXTRON1){ // sendIR to RT5X / OSSC
      if(sprof < 11){
        if(RT5Xir == 1)sendIR("5x",sprof,2); // RT5X profile
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1)sendIR("ossc",sprof,3); // OSSC profile
      }
      else if(OSSCir == 1 && sprof > 11 && sprof < 15){
        sendIR("ossc",sprof,3); // OSSC profile
      }
      else if(RT5Xir == 4){
        sendIR("5x",sprof - 12,2); // RT5X profile
      }
    }
    else if(sname == EXTRON2 && RT5Xir == 3 && sprof < 11){
      sendIR("5x",sprof,2); // RT5X profile 1
    }
    else if(sname == GSCART1 || sname == GSCART2){
      if(sprof < 211){
        if(RT5Xir == 2)sendIR("5x",sprof - 200,2); // RT5X profile 
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 2)sendIR("ossc",sprof - 200,3); // OSSC profile
      }
      else if(sprof < 215){
        if(OSSCir == 2)sendIR("ossc",sprof - 200,3); // OSSC profile
      }
    }

    if(SVS == 0 && !S0 && sname == EXTRON1 && sprof > 0 && sprof < 13){ sendRBP(sprof); }
    else if(SVS == 0 && S0 && sname == EXTRON1 && sprof > 0 && sprof < 12){ sendRBP(sprof); }
    else if(SVS == 2 && !S0 && (sname == GSCART1 || sname == GSCART2) && sprof > 200 && sprof < 213){ sendRBP(sprof - 200); }
    else if(SVS == 2 && S0 && (sname == GSCART1 || sname == GSCART2) && sprof > 200 && sprof < 212){ sendRBP(sprof - 200); }
    else{ sendSVS(sprof); }
  }
  else if(sprof == 0){ // all inputs are off, set attributes to 0, find a console that is On starting at the top of the list, set as King, send profile
    mswitch[sname].On = 0;
    mswitch[sname].Prof = 0;
    if(mswitch[sname].King == 1){
      int bestIdx = -1;
      uint8_t bestO = mswitchSize;
      for(uint8_t i=0;i < mswitchSize;i++){
        if(mswitch[i].On && mswitch[i].Order < bestO){
          bestO = mswitch[i].Order;
          bestIdx = i;
        }
      }
      for(uint8_t i=0;i < mswitchSize;i++){
        mswitch[i].King = 0;
      }
      if(bestIdx != -1){ 
        mswitch[bestIdx].King = 1;
        if(mswitch[bestIdx].Prof < 0) sendRBP(-1*mswitch[bestIdx].Prof);
        else sendSVS(mswitch[bestIdx].Prof);
        return;
      }
    } // end of if King == 1    
    uint8_t count = 0;
    for(uint8_t m=0;m < mswitchSize;m++){
      if(mswitch[m].On == 0) count++;
    }
    if(S0 && (SVS == 0 || SVS == 2) && (count == mswitchSize) && currentProf != -12){ sendRBP(12); } // of S0 is true, send S0 or "remote prof12" when all consoles are off
    else if(S0 && SVS == 1 && (count == mswitchSize) && currentProf != 0){ sendSVS(0); } 
  
  } // end of else if prof == 0
  recordMem();
} // end of sendProfile()

#if automatrixSW1 || automatrixSW2
uint8_t readAMstate(const char* cinput, uint8_t size){

  uint32_t newAMstate = 0;
  for(uint8_t i=0;i < size;i++){
    char c = cinput[i];
    if(c >= '1' && c <= '9'){
      newAMstate |= (1UL << (size - 1 - i));
    }
  }

  uint32_t changed = newAMstate ^ prevAMstate;

  for(uint8_t bitPos = 0;bitPos < size;bitPos++){
    uint32_t bit = 1UL << (size - 1 - bitPos);
    uint8_t input = bitPos + 1;

    if(changed & bit){
      if(newAMstate & bit){ // input on
        bool exists = false;
        for(int j=0;j <= AMstateTop;j++){
          if(AMstate[j] == input){
            exists = true;
            break;
          }
        }
        if(!exists && AMstateTop < (size - 1)) AMstate[++AMstateTop] = input;
      } // end of input on
      else{ // input off
        for(int j=0;j <= AMstateTop;j++){
          if(AMstate[j] == input){
            for(int k = j;k < AMstateTop;k++){
              AMstate[k] = AMstate[k + 1];
            }
            AMstateTop--;
            break;
          }
        }
      } // end of input off
    } // end of changed ?
  } // end of for

  prevAMstate = newAMstate;
  recordMem();
  return AMstate[AMstateTop];
} // end of readAMstate()
#endif
