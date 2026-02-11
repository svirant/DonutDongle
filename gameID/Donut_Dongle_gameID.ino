/*
* Donut Dongle gameID v0.4a (Arduino Nano ESP32 only)
* Copyright(C) 2026 @Donutswdad
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
#define Serial Serial0 // ** COMMENT OUT THIS LINE ** to see output in Serial Monitor. Disables Serial output to RT4K. usbMode must also be set to "false"


#include <TinyIRReceiver.hpp> // all can be found in the built-in Library Manager
#include <IRremote.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

//////////////////////////////////////////  WIFI & Setup //  WIFI & Setup //  WIFI & Setup // WIFI & Setup //////////////////////////////////////////

/**--- WIFI & Setup ---**/
const char* donuthostname = "donutshop";      // hostname, by default: http://donutshop.local include quotes ""
const char* ssid = "SSID";                    // Wifi Network goes here in quotes ""  Note: MUST be a 2.4GHz WiFi AP. 5GHz is NOT supported by the Nano ESP32.
const char* password = "password";            // replace "password" with your Wifi password including quotes ""
const char* updatepassword = "update123";     // password for updating firmware over Wifi via Arduino IDE 2.x. include quotes ""

#define usbMode false              // USB Serial Command mode. set true to enable. requires OTG adapter. normal Serial mode will be active regardless of setting.
                                   // https://github.com/wakwak-koba/EspUsbHost will also need to be installed in order to have FTDI support for the RT4K usb serial port.
                                   // This is the easiest method...
                                   // Step 1 - Goto the github link above. Click the GREEN "<> Code" box and "Download ZIP"
                                   // Step 2 - In Arudino IDE; goto "Sketch" -> "Include Library" -> "Add .ZIP Library"


/*
////////////////////
//    OPTIONS    //
//////////////////
*/

uint8_t const debugE1CAP = 0; // line ~700
uint8_t const debugE2CAP = 0; // line ~961
uint8_t const debugState = 0; // line ~555


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

bool const S0 = false;  // (Profile 0) default is false 
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
                              //
                              // 4 = enabled for Otaku Games Scart Switch that is connected to Extron sw1 / alt sw1 with another Otaku via headphone splitter.
                              //      this 2nd Otaku has been flashed to repsond with "remote prof13" - "remote prof22", and "remote prof24" when all ports are off.

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


String const auxpower = "LG"; // AUX8 + Power button sends power off/on via IR Emitter. "LG" OLEX CX is the only one implemented atm.

#define GAMEID1 0
#define EXTRON1 1
#define EXTRON2 2

/*
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//    GAMEID CONFIG     //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
*/

struct Console {
  String Desc;
  String Address;
  int DefaultProf;
  int Prof;
  int On;
  int King;
  bool Enabled;
};

struct profileorder {
  int Prof;
  uint8_t On;
  uint8_t King;
  uint8_t Order;
};

profileorder mswitch[3] = {{0,0,0,0},{0,0,0,1},{0,0,0,2}};
uint8_t mswitchSize = 3;

                   // format as so: {Description, console address, Default Profile for console, current profile state (leave 0), power state (leave 0), active state (leave 0), Enabled}
                   //
                   // If using a "remote button profile" for the "Default Profile" which are valued 1 - 12, place a "-" before the profile number. 
                   // Example: -1 means "remote button profile 1"
                   //          -12 means "remote button profile 12"
                   //            0 means SVS profile 0
                   //            1 means SVS profile 1
                   //           12 means SVS profile 12
                   //           etc...
Console consoles[10] = {{"PS1 Digital","http://ps1digital.local/gameid",-9,0,0,0,1}, // you can add more, but stay in this format
                      {"MemCardPro","http://10.0.1.52/api/currentState",-5,0,0,0,1},
                      {"N64 Digital","http://n64digital.local/gameid",-7,0,0,0,1} // the last one in the list has no "," at the end
                      };

int consolesSize = 3; // Can hold 10 entries, but only set for 3 so the UI doesnt show 7 blank entries :)


                   // If using a "remote button profile" for the "PROFILE" which are valued 1 - 12, place a "-" before the profile number. 
                   // Example: -1 means "remote button profile 1"
                   //          -12 means "remote button profile 12"
                   //            0 means SVS profile 0
                   //            1 means SVS profile 1
                   //           12 means SVS profile 12
                   //           etc...
                   //                      
                                 // {"Description","<GAMEID>","PROFILE #"},
String gameDB[1000][3] = {{"N64 EverDrive","00000000-00000000---00","7"}, // 7 is the "SVS PROFILE", would translate to a S7_<USER_DEFINED>.rt4 named profile under RT4K-SDcard/profile/SVS/
                      {"xstation","XSTATION","8"},               // XSTATION is the <GAMEID>
                      {"GameCube","GM4E0100","505"},             // GameCube is the Description
                      {"N64 MarioKart 64","3E5055B6-2E92DA52-N-45","501"},
                      {"N64 Mario 64","635A2BFF-8B022326-N-45","502"},
                      {"N64 GoldenEye 007","DCBC50D1-09FD1AA3-N-45","503"},
                      {"N64 Wave Race 64","492F4B61-04E5146A-N-45","504"},
                      {"PS1 Ridge Racer Revolution","SLUS-00214","10"},
                      {"PS1 Ridge Racer","SCUS-94300","9"},
                      {"MegaDrive","MegaDrive","506"},
                      {"SoR2","Streets of Rage 2 (USA)","507"}};

uint16_t gameDBSize = 11; // array can hold 1000 entries, but only set to current size so the UI doesnt show 989 blank entries :)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// automatrix variables
#if automatrixSW1 || automatrixSW2
uint8_t AMstate[32];
uint32_t prevAMstate = 0;
int  AMstateTop = -1;
uint8_t amSizeSW1 = 8; // 8 by default, but updates if a different size is discovered
uint8_t amSizeSW2 = 8; // ...
#endif

// GAMEID variables
bool S0_gameID = true;    // When a gameID match is not found for a powered on console, DefaultProf for that console will load
String payload = ""; 
unsigned long currentGameTime = 0;
unsigned long prevGameTime = 0;

// Extron Global variables
uint8_t eoutput[2]; // used to store Extron output
int currentInputSW1 = -1;
int currentInputSW2 = -1;
String ecap = "00000000000000000000000000000000000000000000"; // used to store Extron status messages for Extron in String format
String einput = "000000000000000000000000000000000000"; // used to store Extron input
byte ecapbytes[44] = {0}; // used to store first 44 bytes / messages for Extron capture

// Serial commands
byte viki[4] = {0xA5,0x5A,0x00,0xCC};
byte tesmart[6] = {0xAA,0xBB,0x03,0x01,0x01,0xEE};
byte const VERB[5] = {0x57,0x33,0x43,0x56,0x7C}; // sets matrix switch to verbose level 3

// LS timer variables
unsigned long LScurrentTime = 0; 
unsigned long LScurrentTime2 = 0;
unsigned long LSprevTime = 0;
unsigned long LSprevTime2 = 0;

// Delay Send variables
unsigned long DScurrentTime = 0; 
unsigned long DScurrentTime2 = 0;
unsigned long DSprevTime = 0;
unsigned long DSprevTime2 = 0;
bool delaySend = false;
int delayProf = 33333;

// IR Global variables
uint8_t repeatcount = 0; // used to help emulate the repeat nature of directional button presses
String svsbutton; // used to store 3 digit SVS profile when AUX8 is double pressed
uint8_t nument = 0; // used to keep track of how many digits have been entered for 3 digit SVS profile

// sendRTwake global variables
int currentProf = 0; // negative numbers for Remote Button profiles, positive for SVS profiles
bool RTwake = false;
unsigned long currentTime = 0;
unsigned long prevTime = 0;
unsigned long prevBlinkTime = 0;
bool wakesleepcmd = false; // specifically for USB mode, needed for the "pwr on" / "remote pwr" command combo so a single button can sleep/wake

// TESmart remote control
uint8_t aux7button = 0; // Used to keep track if AUX7 was pressed to change inputs on TESmart 16x1 HDMI switch via RT4K remote.
uint8_t aux8button = 0;  // Used to keep track of AUX8 button presses for addtional remote commands

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

WebServer server(80);

void DDloop(void *pvParameters);
void GIDloop(void *pvParameters);
uint16_t gTime = 2000;
uint8_t RMTuse = 0;

#if usbMode
#include <EspUsbHostSerial_FTDI.h> // https://github.com/wakwak-koba/EspUsbHost in order to have FTDI support for the RT4K usb serial port, this is the easiest method.
                                   // Step 1 - Goto the github link above. Click the GREEN "<> Code" box and "Download ZIP"
                                   // Step 2 - In Arudino IDE; goto "Sketch" -> "Include Library" -> "Add .ZIP Library"

class SerialFTDI : public EspUsbHostSerial_FTDI {
  public:
  String cprof = "null";
  String tcprof = "null";
  String tcmd = "null";
  int tp = 0;
  virtual void task(void) override {
    EspUsbHost::task();
    if(this->isReady()){
      usb_host_transfer_submit(this->usbTransfer_recv);
      if(cprof != "null"){
        tp = cprof.toInt();
        analogWrite(LED_GREEN,222);
        if(tp >= 0){
          if(offset > 0) cprof = String(tp + offset);
          tcprof = "\rSVS NEW INPUT=" + cprof + "\r";
          submit((uint8_t *)reinterpret_cast<const uint8_t*>(&tcprof[0]), tcprof.length()); // usb response
          Serial.print(F("\rSVS NEW INPUT=")); // serial response
          if(tp != 0)Serial.print(tp + offset);
          else Serial.print(tp);;
          Serial.println(F("\r"));
          delay(1000);
          tcprof = "\rSVS CURRENT INPUT=" + cprof + "\r"; // serial response
          Serial.print(F("\rSVS CURRENT INPUT="));
          if(tp != 0)Serial.print(tp + offset);
          else Serial.print(tp);
          Serial.println(F("\r"));      
        }
        if(tp < 0){
          tcprof = "\rremote prof" + String((-1)*tp) + "\r";
        }
        submit((uint8_t *)reinterpret_cast<const uint8_t*>(&tcprof[0]), tcprof.length()); // usb response
        if(tp < 0) delay(1000); // only added so the green led stays lit for 1 second for "remote prof" commands
        analogWrite(LED_GREEN,255);
        tcprof,cprof = "null";
      }
      else if(tcmd == "wakesleep"){ // this sets up the "pwr on", "remote pwr" command combo so a single button can sleep/wake
        tcmd = "\r\npwr on\r\n";
        submit((uint8_t *)reinterpret_cast<const uint8_t*>(&tcmd[0]), tcmd.length());
        tcmd = "null";
        wakesleepcmd = true; // set true so the following will execute next cycle
      }
      else if(wakesleepcmd){
        tcmd = "\r\nremote pwr\r\n";
        submit((uint8_t *)reinterpret_cast<const uint8_t*>(&tcmd[0]), tcmd.length());
        tcmd = "null";
        wakesleepcmd = false;
      }
      else if(tcmd != "null"){
        submit((uint8_t *)reinterpret_cast<const uint8_t*>(&tcmd[0]), tcmd.length());
        tcmd = "null";
      }
    }
  }
};

SerialFTDI usbHost;
#endif

void setup(){

  initPCIInterruptForTinyReceiver(); // for IR Receiver
  WiFi.begin(ssid,password); // WiFi creds are defined around line 40 at the top
  #if usbMode
  usbHost.begin(115200); // leave at 115200 for RT4K usb connection
  #endif
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT); // GREEN led lights up for 1 second when a SVS profile is sent
  pinMode(LED_BLUE, OUTPUT); // BLUE led is a WiFi activity. Long periods of blue means one of the gameID servers is not connecting.
  analogWrite(LED_GREEN,255);
  analogWrite(LED_BLUE,255);

  Serial.begin(9600);                           // set the baud rate for the RT4K VGA serial connection
  extronSerial.begin(9600,SERIAL_8N1,3,4);   // set the baud rate for the Extron sw1 Connection
  extronSerial.setTimeout(150);                 // sets the timeout for reading / saving into a string
  if(automatrixSW1)extronSerial.write(VERB,5); // sets extron matrix switch to Verbose level 3
  extronSerial2.begin(9600,SERIAL_8N1,8,9);  // set the baud rate for Extron sw2 Connection
  extronSerial2.setTimeout(150);                // sets the timeout for reading / saving into a string for the Extron sw2 Connection
  if(automatrixSW2)extronSerial2.write(VERB,5); // sets extron matrix switch to Verbose level 3
  MDNS.begin(donuthostname); // defined around line 40 at the top
  if(!LittleFS.begin(true)){ // format if mount fails
    Serial.println(F("LittleFS mount failed!"));
    return;
  }
  OTAsetup();

  loadGameDB();
  loadConsoles();
  loadS0Vars();

  server.on("/",HTTP_GET,handleRoot);
  server.on("/getConsoles",HTTP_GET,handleGetConsoles);
  server.on("/updateConsoles",HTTP_POST,handleUpdateConsoles);
  server.on("/getGameDB",HTTP_GET,handleGetGameDB);
  server.on("/updateGameDB",HTTP_POST,handleUpdateGameDB);
  server.on("/getS0Vars", HTTP_GET, handleGetS0Vars);
  server.on("/updateS0Vars", HTTP_POST, handleUpdateS0Vars);
  server.on("/getPayload", HTTP_GET, handleGetPayload);
  server.on("/exportAll", HTTP_GET, handleExportAll);
  server.on("/importAll", HTTP_POST, handleImportAll);

  server.begin();

  xTaskCreate(DDloop,"DDloop",4096,NULL,1,NULL);
  xTaskCreate(GIDloop,"GIDloop",4096,NULL,1,NULL);
  
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
    if(RTwake)sendRTwake(8000); // 8000 is 8 seconds. After waking the RT4K, wait this amount of time before re-sending the latest profile change.
    if(delaySend)DStime(500);
    server.handleClient();
    if(debugState){
      delay(100);
      Serial.print(F("GAMEID1 "));Serial.print(F(" On: "));Serial.print(mswitch[GAMEID1].On);Serial.print(F(" King: "));Serial.print(mswitch[GAMEID1].King);Serial.print(F(" Prof: "));Serial.println(mswitch[GAMEID1].Prof);
      Serial.print(F("EXTRON1 "));Serial.print(F(" On: "));Serial.print(mswitch[EXTRON1].On);Serial.print(F(" King: "));Serial.print(mswitch[EXTRON1].King);Serial.print(F(" Prof: "));Serial.println(mswitch[EXTRON1].Prof);
      Serial.print(F("EXTRON2 "));Serial.print(F(" On: "));Serial.print(mswitch[EXTRON2].On);Serial.print(F(" King: "));Serial.print(mswitch[EXTRON2].King);Serial.print(F(" Prof: "));Serial.println(mswitch[EXTRON2].Prof);
    }
    ArduinoOTA.handle();
    #if usbMode
    usbHost.task();  // used for RT4K usb serial communications
    #endif
  }
} // end of DDloop

void GIDloop(void *pvParameters){
  (void)pvParameters;

  for(;;){
    readGameID();
  }
} // end of GIDloop

void OTAsetup(){
  ArduinoOTA.setHostname(donuthostname);
  ArduinoOTA.setPassword(updatepassword);

  ArduinoOTA
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("OTA Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("OTA Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("OTA Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("OTA Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("OTA End Failed");
    });
  ArduinoOTA.begin();
} // end of OTAsetup()

void readGameID(){ // queries addresses in "consoles" array for gameIDs
  currentGameTime = millis();  // Init timer
  if(prevGameTime == 0)       // If previous timer not initialized, do so now.
    prevGameTime = millis();
  if((currentGameTime - prevGameTime) >= gTime){ // make sure at least gTime has passed before continuing
    int result = 0;
    for(int i = 0; i < consolesSize; i++){
      if(WiFi.status() == WL_CONNECTED && consoles[i].Enabled){ // wait for WiFi connection
        HTTPClient http;
        http.setConnectTimeout(2000); // give only 2 seconds per console to check gameID, is only honored for IP-based addresses
        http.begin(consoles[i].Address);
        analogWrite(LED_BLUE,222);
        int httpCode = http.GET();             // start connection and send HTTP header
        if(httpCode > 0 || httpCode == -11){   // httpCode will be negative on error, let the read error slide...
          if(httpCode == HTTP_CODE_OK){        // console is healthy // HTTP header has been sent and Server response header has been handled
            consoles[i].Address = replaceDomainWithIP(consoles[i].Address); // replace Domain with IP in consoles array. this allows setConnectTimeout to be honored
            payload = http.getString();        
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            if(!error){ // If the response is JSON, continue
              if(!doc["gameID"].isNull()) {  // If payload contains gameID, 
                payload = doc["gameID"].as<String>(); // reset payload to it's value
              }
            }
            result = fetchGameIDProf(payload,consoles[i].DefaultProf);
            consoles[i].On = 1;
            if(consoles[i].Prof != result && result != -1){ // gameID found for console, set as King, unset previous King, send profile change 
              consoles[i].Prof = result;
              consoles[i].King = 1;
              for(int j=0;j < consolesSize;j++){ // set previous King to 0
                if(i != j && consoles[j].King == 1)
                  consoles[j].King = 0;
              }
              sendProfile(consoles[i].Prof,GAMEID1,1);
            }
          } 
        } // end of if(httpCode > 0 || httpCode == -11)
        else{ // console is off, set attributes to 0, find a console that is On starting at the top of the gameID list, set as King, send profile
          consoles[i].On = 0;
          consoles[i].Prof = 0;
          if(consoles[i].King == 1){
            for(int k=0;k < consolesSize;k++){
              if(i == k){
                consoles[k].King = 0;
                for(int l=0;l < consolesSize;l++){ // find next Console that is on
                  if(consoles[l].On == 1){
                    consoles[l].King = 1;
                    sendProfile(consoles[l].Prof,GAMEID1,1);
                    break;
                  }
                }
              }
    
            } // end of for()
          } // end of if()
          int count = 0;
          for(int m=0;m < consolesSize;m++){
            if(consoles[m].On == 0) count++;
          }
          if(count == consolesSize){
            sendProfile(0,GAMEID1,1);
          }
        } // end of else()
      http.end();
      analogWrite(LED_BLUE, 255);
      }  // end of WiFi connection
      else if(!consoles[i].Enabled){ // console is disabled in web ui, set attributes to 0, find a console that is On starting at the top of the gameID list, set as King, send profile
          consoles[i].On = 0;
          consoles[i].Prof = 0;
          if(consoles[i].King == 1){
            for(int k=0;k < consolesSize;k++){
              if(i == k){
                consoles[k].King = 0;
                for(int l=0;l < consolesSize;l++){ // find next Console that is on
                  if(consoles[l].On == 1){
                    consoles[l].King = 1;
                    sendProfile(consoles[l].Prof,GAMEID1,1);
                    break;
                  }
                }
              }
    
            } // end of for()
          } // end of if()
      } // end of if else()
    }
    currentGameTime = 0;
    prevGameTime = 0;
  }
}  // end of readGameID()


void readExtron1(){

#if automatrixSW1 // if automatrixSW1 is set "true" in options, then "0LS" is sent every 500ms to see if an input has changed
    LS0time1(500);
#endif

#if !automatrixSW1
    if(MTVddSW1){            // if a MT-VIKI switch has been detected on SW1, then the currently active MT-VIKI hdmi port is checked for disconnection
      MTVtime1(1500);
    }
#endif

    // listens to the Extron sw1 Port for changes
    // SIS Command Responses reference - Page 77 https://media.extron.com/public/download/files/userman/XP300_Matrix_B.pdf
    if(extronSerial.available() > 0){ // if there is data available for reading, read
      extronSerial.readBytes(ecapbytes,44); // read in and store only the first 44 bytes for every status message received from 1st Extron SW port
      if(debugE1CAP){
        Serial.print(F("ecap HEX: "));
        for(uint8_t i=0;i<44;i++){
          Serial.print(ecapbytes[i],HEX);Serial.print(F(" "));
        }
        Serial.println(F("\r"));
        ecap = String((char *)ecapbytes);
        Serial.print(F("ecap ASCII: "));Serial.println(ecap);
      }
    }
    if(!debugE1CAP) ecap = String((char *)ecapbytes); // convert bytes to String for Extron switches


    if((ecap.substring(0,3) == "Out" || ecap.substring(0,3) == "OUT") && !automatrixSW1){ // store only the input and output states, some Extron devices report output first instead of input
      if(ecap.substring(4,5) == " "){
        einput = ecap.substring(5,9);
        if(ecap.substring(3,4).toInt() == ExtronVideoOutputPortSW1) eoutput[0] = 1;
        else eoutput[0] = 0;
      }
      else{
        einput = ecap.substring(6,10);
        if(ecap.substring(3,5).toInt() == ExtronVideoOutputPortSW1) eoutput[0] = 1;
        else eoutput[0] = 0;
      }
    }
    else if(ecap.substring(0,1) == "F"){ // detect if switch has changed auto/manual states
      einput = ecap.substring(4,8);
      eoutput[0] = 1;
    }
    else if(ecap.substring(0,3) == "Rpr"){ // detect if a Preset has been used
      einput = ecap.substring(0,5);
      eoutput[0] = 1;
    }
    else if(ecap.substring(0,8) == "RECONFIG"){     // This is received everytime a change is made on older Extron Crosspoints
      ExtronOutputQuery(ExtronVideoOutputPortSW1,1); // Finds current input for "ExtronVideoOutputPortSW1" that is connected to port 1 of the DD
    }
#if automatrixSW1 
    else if(ecap.substring(amSizeSW1 + 6,amSizeSW1 + 9) == "Rpr"){ // detect if a Preset has been used 
      einput = ecap.substring(amSizeSW1 + 6,amSizeSW1 + 11);
      eoutput[0] = 1;
    }
    else if(ecap.substring(amSizeSW1 + 7,amSizeSW1 + 10) == "Rpr"){ // detect if a Preset has been used 
      einput = ecap.substring(amSizeSW1 + 7,amSizeSW1 + 12);
      eoutput[0] = 1;
    }
    else if(ecap.substring(0,3) == "In0" && ecap.substring(4,7) != "All" && ecap.substring(5,8) != "All"){ // start of automatrix
      if(ecap.substring(0,4) == "In00"){
        amSizeSW1 = ecap.length() - 7;
        einput = ecap.substring(5,amSizeSW1 + 5);
      }
      else{
        amSizeSW1 = ecap.length() - 6;
        einput = ecap.substring(4,amSizeSW1 + 4);
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
    } // end of automatrix
#endif
    else{                             // less complex switches only report input status, no output status
      einput = ecap.substring(0,4);
      eoutput[0] = 1;
    }


    // For older Extron Crosspoints, where "RECONFIG" is sent when changes are made, the profile is only changed when a different input is selected for the defined output. (ExtronVideoOutputPortSW1)
    // Without this, the profile would be resent when changes to other outputs are selected.
    if(einput.substring(0,2) == "IN"){
      int temp = einput.substring(2,4).toInt();
      if(SVS == 0 && !S0 && temp > 0 && temp < 13 && temp == -1*currentProf) einput = "XX00";
      else if(SVS == 0 && S0 && temp > 0 && temp < 12 && temp == -1*currentProf) einput = "XX00";
      else if(temp == currentProf) einput = "XX00"; 
    }

    // for Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references eoutput[0]
    if(((einput.substring(0,2) == "IN" || einput.substring(0,2) == "In") && eoutput[0] && !automatrixSW1) || (einput.substring(0,3) == "Rpr")){
      if(einput.substring(0,3) == "Rpr"){
        sendProfile(einput.substring(3,5).toInt(),EXTRON1,1);
      }
      else if(einput != "IN0 " && einput != "In0 " && einput != "In00"){ // for inputs 13-99 (SVS only)
        sendProfile(einput.substring(2,4).toInt(),EXTRON1,1);
      }
      else if(einput == "IN0" || einput == "In0 " || einput == "In00"){
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


    if((ecap.substring(0,3) == "==>" || ecap.substring(15,18) == "==>") && listenITE[0]){   // checks if the serial command from the VIKI starts with "=" This indicates that the command is an ITE mux status message
      if(ecap.substring(10,11) == "P"){        // checks the last value of the IT6635 mux. P3 points to inputs 1,2,3 / P2 points to inputs 4,5,6 / P1 input 7 / P0 input 8
        ITEstatus[0] = ecap.substring(11,12).toInt();
      }
      if(ecap.substring(25,26) == "P"){        // checks the last value of the IT6635 mux. P3 points to inputs 1,2,3 / P2 points to inputs 4,5,6 / P1 input 7 / P0 input 8
        ITEstatus[0] = ecap.substring(26,27).toInt();
      }
      if(ecap.substring(18,20) == ">0"){       // checks the value of the IT66535 IC that points to Dev->0. P2 is input 1 / P1 is input 2 / P0 is input 3
        ITEstatus[1] = ecap.substring(12,13).toInt();
      }
      if(ecap.substring(33,35) == ">0"){       // checks the value of the IT66535 IC that points to Dev->0. P2 is input 1 / P1 is input 2 / P0 is input 3
        ITEstatus[1] = ecap.substring(27,28).toInt();
      }
      if(ecap.substring(18,20) == ">1"){       // checks the value of the IT66535 IC that points to Dev->1. P2 is input 4 / P1 is input 5 / P0 is input 6
        ITEstatus[2] = ecap.substring(12,13).toInt();
      }
      if(ecap.substring(33,35) == ">1"){       // checks the value of the IT66535 IC that points to Dev->1. P2 is input 4 / P1 is input 5 / P0 is input 6
        ITEstatus[2] = ecap.substring(27,28).toInt();
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

    if(ecap.substring(0,5) == "Auto_" || ecap.substring(15,20) == "Auto_" || ITEinputnum[0] > 0) MTVddSW1 = true; // enable MT-VIKI disconnection detection if MT-VIKI switch is present

    // for TESmart 4K60 / TESmart 4K30 / MT-VIKI HDMI switch on SW1
    if(ecapbytes[4] == 17 || ecapbytes[3] == 17 || ecap.substring(0,5) == "Auto_" || ecap.substring(15,20) == "Auto_" || ITEinputnum[0] > 0){
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

      if(ecap.substring(0,5) == "Auto_" || ecap.substring(15,20) == "Auto_") listenITE[0] = 0; // Sets listenITE[0] to 0 so the ITE mux data will be ignored while an autoswitch command is detected.
      ITEinputnum[0] = 0;                     // Resets ITEinputnum[0] to 0 so sendSVS will not repeat after this cycle through the void loop
      ITEtimer = millis();                 // resets ITEtimer to millis()
      MTVprevTime = millis();              // delays disconnection detection timer so it wont interrupt
    }
   
    // if a MT-VIKI active port disconnection is detected, and then later a reconnection, resend the profile.
    if(ecap.substring(24,41) == "IS_NON_INPUT_PORT"){
      MTVdiscon[0] = true;
      sendProfile(0,EXTRON1,0);
    }
    else if(ecap.substring(24,41) != "IS_NON_INPUT_PORT" && ecap.substring(0,11) == "Uart_RxData" && MTVdiscon[0]){
      MTVdiscon[0] = false;
      sendProfile(currentMTVinput[0],EXTRON1,1);
    }


    // for Otaku Games Scart Switch 1
    if(ecap.substring(0,11) == "remote prof"){
      if(ecap.substring(0,13) == "remote prof10"){
        sendProfile(10,EXTRON1,1);
      }
      else if(ecap.substring(0,13) == "remote prof12"){
        sendProfile(0,EXTRON1,1);
      }
      else if(ecap.substring(11,13).toInt() > 12){
        sendProfile(ecap.substring(11,13).toInt(),EXTRON1,1);
      }
      else{
        sendProfile(ecap.substring(11,12).toInt(),EXTRON1,1);
      }
    }
#endif

  memset(ecapbytes,0,sizeof(ecapbytes)); // reset capture to all 0s
  ecap = "00000000000000000000000000000000000000000000";
  einput = "000000000000000000000000000000000000";

} // end of readExtron1()

void readExtron2(){

#if automatrixSW2// if automatrixSW2 is set "true" in options, then "0LS" is sent every 500ms to see if an input has changed
      LS0time2(500);
#endif

#if !automatrixSW2
    if(MTVddSW2){            // if a MT-VIKI switch has been detected on SW2, then the currently active MT-VIKI hdmi port is checked for disconnection
      MTVtime2(1500);
    }
#endif

    // listens to the Extron sw2 Port for changes
    if(extronSerial2.available() > 0){ // if there is data available for reading, read
    extronSerial2.readBytes(ecapbytes,44); // read in and store only the first 44 bytes for every status message received from 2nd Extron port
      if(debugE2CAP){
        Serial.print(F("ecap2 HEX: "));
        for(uint8_t i=0;i<44;i++){
          Serial.print(ecapbytes[i],HEX);Serial.print(F(" "));
        }
        Serial.println(F("\r"));
        ecap = String((char *)ecapbytes);
        Serial.print(F("ecap2 ASCII: "));Serial.println(ecap);
      }
    }
    if(!debugE2CAP) ecap = String((char *)ecapbytes);

    if((ecap.substring(0,3) == "Out" || ecap.substring(0,3) == "OUT") && !automatrixSW2){ // store only the input and output states, some Extron devices report output first instead of input
      if(ecap.substring(4,5) == " "){
        einput = ecap.substring(5,9);
        if(ecap.substring(3,4).toInt() == ExtronVideoOutputPortSW2) eoutput[1] = 1;
        else eoutput[1] = 0;
      }
      else{
        einput = ecap.substring(6,10);
        if(ecap.substring(3,5).toInt() == ExtronVideoOutputPortSW2) eoutput[1] = 1;
        else eoutput[1] = 0;
      }
    }
    else if(ecap.substring(0,1) == "F"){ // detect if switch has changed auto/manual states
      einput = ecap.substring(4,8);
      eoutput[1] = 1;
    }
    else if(ecap.substring(0,3) == "Rpr"){ // detect if a Preset has been used
      einput = ecap.substring(0,5);
      eoutput[1] = 1;
    }
    else if(ecap.substring(0,8) == "RECONFIG"){     // This is received everytime a change is made on older Extron Crosspoints.
      ExtronOutputQuery(ExtronVideoOutputPortSW2,2); // Finds current input for "ExtronVideoOutputPortSW2" that is connected to port 2 of the DD
    }
#if automatrixSW2    
    else if(ecap.substring(amSizeSW2 + 6,amSizeSW2 + 9) == "Rpr"){ // detect if a Preset has been used 
      einput = ecap.substring(amSizeSW2 + 6,amSizeSW2 + 11);
      eoutput[1] = 1;
    }
    else if(ecap.substring(amSizeSW2 + 7,amSizeSW2 + 10) == "Rpr"){ // detect if a Preset has been used 
      einput = ecap.substring(amSizeSW2 + 7,amSizeSW2 + 12);
      eoutput[1] = 1;
    }
    else if(ecap.substring(0,3) == "In0" && ecap.substring(4,7) != "All" && ecap.substring(5,8) != "All"){ // start of automatrix
      if(ecap.substring(0,4) == "In00"){
        amSizeSW2 = ecap.length() - 7;
        einput = ecap.substring(5,amSizeSW2 + 5);
      }
      else{
        amSizeSW2 = ecap.length() - 6;
        einput = ecap.substring(4,amSizeSW2 + 4);
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
    } // end of automatrix
#endif
    else{                              // less complex switches only report input status, no output status
      einput = ecap.substring(0,4);
      eoutput[1] = 1;
    }

    // For older Extron Crosspoints, where "RECONFIG" is sent when changes are made, the profile is only changed when a different input is selected for the defined output. (ExtronVideoOutputPortSW2)
    // Without this, the profile would be resent when changes to other outputs are selected.
    if(einput.substring(0,2) == "IN" && einput.substring(2,4).toInt()+100 == currentProf) einput = "XX00";

    // For Extron devices, use remaining results to see which input is now active and change profile accordingly, cross-references eoutput[1]
    if(((einput.substring(0,2) == "IN" || einput.substring(0,2) == "In") && eoutput[1] && !automatrixSW2) || (einput.substring(0,3) == "Rpr")){
      if(einput.substring(0,3) == "Rpr"){
        sendProfile(einput.substring(3,5).toInt()+100,EXTRON2,1);
      }
      else if(einput != "IN0 " && einput != "In0 " && einput != "In00"){ // much easier method for switch 2 since ALL inputs will respond with SVS commands regardless of SVS option above
        sendProfile(einput.substring(2,4).toInt()+100,EXTRON2,1);
      }
      else if(einput == "IN0" || einput == "In0 " || einput == "In00"){
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


    if((ecap.substring(0,3) == "==>" || ecap.substring(15,18) == "==>") && listenITE[1]){   // checks if the serial command from the VIKI starts with "=" This indicates that the command is an ITE mux status message
      if(ecap.substring(10,11) == "P"){       // checks the last value of the IT6635 mux. P3 points to inputs 1,2,3 / P2 points to inputs 4,5,6 / P1 input 7 / P0 input 8
        ITEstatus2[0] = ecap.substring(11,12).toInt();
      }
      if(ecap.substring(25,26) == "P"){       // checks the last value of the IT6635 mux. P3 points to inputs 1,2,3 / P2 points to inputs 4,5,6 / P1 input 7 / P0 input 8
        ITEstatus2[0] = ecap.substring(26,27).toInt();
      }
      if(ecap.substring(18,20) == ">0"){       // checks the value of the IT66535 IC that points to Dev->0. P2 is input 1 / P1 is input 2 / P0 is input 3
        ITEstatus2[1] = ecap.substring(12,13).toInt();
      }
      if(ecap.substring(33,35) == ">0"){       // checks the value of the IT66535 IC that points to Dev->0. P2 is input 1 / P1 is input 2 / P0 is input 3
        ITEstatus2[1] = ecap.substring(27,28).toInt();
      }
      if(ecap.substring(18,20) == ">1"){       // checks the value of the IT66535 IC that points to Dev->1. P2 is input 4 / P1 is input 5 / P0 is input 6
        ITEstatus2[2] = ecap.substring(12,13).toInt();
      }
      if(ecap.substring(33,35) == ">1"){       // checks the value of the IT66535 IC that points to Dev->1. P2 is input 4 / P1 is input 5 / P0 is input 6
        ITEstatus2[2] = ecap.substring(27,28).toInt();
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

    if(ecap.substring(0,5) == "Auto_" || ecap.substring(15,20) == "Auto_" || ITEinputnum[1] > 0) MTVddSW2 = true; // enable MT-VIKI disconnection detection if MT-VIKI switch is present


    // for TESmart 4K60 / TESmart 4K30 / MT-VIKI HDMI switch on SW2
    if(ecapbytes[4] == 17 || ecapbytes[3] == 17 || ecap.substring(0,5) == "Auto_" || ecap.substring(15,20) == "Auto_" || ITEinputnum[1] > 0){
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

      if(ecap.substring(0,5) == "Auto_" || ecap.substring(15,20) == "Auto_") listenITE[1] = 0; // Sets listenITE[1] to 0 so the ITE mux data will be ignored while an autoswitch command is detected.
      ITEinputnum[1] = 0;                     // Resets ITEinputnum[1] to 0 so sendSVS will not repeat after this cycle through the void loop
      ITEtimer2 = millis();                 // resets ITEtimer2 to millis()
      MTVprevTime2 = millis();              // delays disconnection detection timer so it wont interrupt
    }

    // if a MT-VIKI active port disconnection is detected, and then later a reconnection, resend the profile.
    if(ecap.substring(24,41) == "IS_NON_INPUT_PORT"){
      MTVdiscon[1] = true;
      sendProfile(0,EXTRON2,0);
    }
    else if(ecap.substring(24,41) != "IS_NON_INPUT_PORT" && ecap.substring(0,11) == "Uart_RxData" && MTVdiscon[1]){
      MTVdiscon[1] = false;
      sendProfile(currentMTVinput[1],EXTRON2,1);

    }    
    
    // for Otaku Games Scart Switch 2
    if(ecap.substring(0,11) == "remote prof"){
      if(ecap.substring(0,13) == "remote prof10"){
        sendProfile(110,EXTRON2,1);
      }
      else if(ecap.substring(0,13) == "remote prof12"){
        sendProfile(0,EXTRON2,1);
      }
      else{
        sendProfile(ecap.substring(11,12).toInt()+100,EXTRON2,1);
      }
    }
  #endif

  memset(ecapbytes,0,sizeof(ecapbytes)); // reset capture to 0s
  ecap = "00000000000000000000000000000000000000000000";
  einput = "000000000000000000000000000000000000";

}// end of readExtron2()

void readIR(){

  uint8_t ir_recv_command = 0;
  uint8_t ir_recv_address = 0;

  if(TinyReceiverDecode()){

    ir_recv_command = TinyIRReceiverData.Command;
    ir_recv_address = TinyIRReceiverData.Address;

    if(ir_recv_command != 0) RMTuse = 1;
        
    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && aux8button == 2){
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
        aux8button = 0;
        svsbutton = "";
        nument = 0;
      }

      
    } // end of extrabutton == 2

    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && aux8button == 1){ // if AUX8 was pressed and a profile button is pressed next,
      if(ir_recv_command == 11){ // profile button 1                                                         // load "SVS" profiles 1 - 12 (profile button 1 - 12).
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[0]);                                                               // Can be changed to "ANY" SVS profile in the OPTIONS section
        if(MTVir == 1){extronSerialEwrite("viki",1,1);}
        if(MTVir == 2){extronSerialEwrite("viki",1,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",1,2);sendSVS(101);}                                                                   
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 7){ // profile button 2
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[1]);
        if(MTVir == 1){extronSerialEwrite("viki",2,1);}
        if(MTVir == 2){extronSerialEwrite("viki",2,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",2,2);sendSVS(102);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 3){ // profile button 3
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[2]);
        if(MTVir == 1){extronSerialEwrite("viki",3,1);}
        if(MTVir == 2){extronSerialEwrite("viki",3,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",3,2);sendSVS(103);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 10){ // profile button 4
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[3]);
        if(MTVir == 1){extronSerialEwrite("viki",4,2);}
        if(MTVir == 2){extronSerialEwrite("viki",4,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",4,2);sendSVS(104);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 6){ // profile button 5
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[4]);
        if(MTVir == 1){extronSerialEwrite("viki",5,1);}
        if(MTVir == 2){extronSerialEwrite("viki",5,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",5,2);sendSVS(105);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 2){ // profile button 6
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[5]);
        if(MTVir == 1){extronSerialEwrite("viki",6,1);}
        if(MTVir == 2){extronSerialEwrite("viki",6,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",6,2);sendSVS(106);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 9){ // profile button 7
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[6]);
        if(MTVir == 1){extronSerialEwrite("viki",7,1);}
        if(MTVir == 2){extronSerialEwrite("viki",7,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",7,2);sendSVS(107);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 5){ // profile button 8
        if(MTVir == 0 && TESmartir < 2)sendSVS(auxprof[7]);
        if(MTVir == 1){extronSerialEwrite("viki",8,1);}
        if(MTVir == 2){extronSerialEwrite("viki",8,2);}
        if(TESmartir > 1){extronSerialEwrite("tesmart",8,2);sendSVS(108);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 1){ // profile button 9
        if(TESmartir < 2)sendSVS(auxprof[8]);
        if(TESmartir > 1){extronSerialEwrite("tesmart",9,2);sendSVS(109);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 37){ // profile button 10
        if(TESmartir < 2)sendSVS(auxprof[9]);
        if(TESmartir > 1){extronSerialEwrite("tesmart",10,2);sendSVS(110);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 38){ // profile button 11
        if(TESmartir < 2)sendSVS(auxprof[10]);
        if(TESmartir > 1){extronSerialEwrite("tesmart",11,2);sendSVS(111);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 39){ // profile button 12
        if(TESmartir < 2)sendSVS(auxprof[11]);
        if(TESmartir > 1){extronSerialEwrite("tesmart",12,2);sendSVS(112);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 56){ // aux1 button
        if(TESmartir > 1){extronSerialEwrite("tesmart",13,2);sendSVS(113);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 57){ // aux2 button
        if(TESmartir > 1){extronSerialEwrite("tesmart",14,2);sendSVS(114);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 58){ // aux3 button
        if(TESmartir > 1){extronSerialEwrite("tesmart",15,2);sendSVS(115);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 59){ // aux4 button
        if(TESmartir > 1){extronSerialEwrite("tesmart",16,2);sendSVS(116);}
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 26){ // (aux8 +) Power button
        sendIR(auxpower,0,1);
        ir_recv_command = 0;
        aux8button = 0;
      }
      else if(ir_recv_command == 63){
        ir_recv_command = 0;
        aux8button++;
      }
      else{
        aux8button = 0;
        svsbutton = "";
        nument = 0;
      }
      
    } // end aux8button == 1


    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT && aux7button == 1){ // if AUX7 was pressed and a profile button is pressed next
      if(TESmartir == 1 || TESmartir == 3){
        if(ir_recv_command == 11){ // profile button 1
          extronSerialEwrite("tesmart",1,1);sendSVS(1);                                                                    
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 7){ // profile button 2
          extronSerialEwrite("tesmart",2,1);sendSVS(2); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 3){ // profile button 3
          extronSerialEwrite("tesmart",3,1);sendSVS(3); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 10){ // profile button 4
          extronSerialEwrite("tesmart",4,1);sendSVS(4); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 6){ // profile button 5
          extronSerialEwrite("tesmart",5,1);sendSVS(5); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 2){ // profile button 6
          extronSerialEwrite("tesmart",6,1);sendSVS(6); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 9){ // profile button 7
          extronSerialEwrite("tesmart",7,1);sendSVS(7); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 5){ // profile button 8
          extronSerialEwrite("tesmart",8,1);sendSVS(8); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 1){ // profile button 9
          extronSerialEwrite("tesmart",9,1);sendSVS(9); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 37){ // profile button 10
          extronSerialEwrite("tesmart",10,1);sendSVS(10); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 38){ // profile button 11
          extronSerialEwrite("tesmart",11,1);sendSVS(11); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 39){ // profile button 12
          extronSerialEwrite("tesmart",12,1);sendSVS(12); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 56){ // aux1 button
          extronSerialEwrite("tesmart",13,1);sendSVS(13); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 57){ // aux2 button
          extronSerialEwrite("tesmart",14,1);sendSVS(14); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 58){ // aux3 button
          extronSerialEwrite("tesmart",15,1);sendSVS(15); 
          ir_recv_command = 0;
          aux8button = 0;
        }
        else if(ir_recv_command == 59){ // aux4 button
          extronSerialEwrite("tesmart",16,1);sendSVS(16);
          ir_recv_command = 0;
          aux8button = 0;
        }
      }

      aux7button = 0;
    }

    if(nument == 3){
      sendSVS(svsbutton.toInt());
      nument = 0;
      svsbutton = "";
      aux8button = 0;
      ir_recv_command = 0;
    }
    
    if(TinyIRReceiverData.Flags == IRDATA_FLAGS_IS_REPEAT){repeatcount++;} // directional buttons have to be held down for just a bit before repeating

    if(ir_recv_address == 73 && TinyIRReceiverData.Flags != IRDATA_FLAGS_IS_REPEAT){ // block most buttons from being repeated when held
      repeatcount = 0;
      if(ir_recv_command == 63){
        if(aux8button < 3)aux8button++;
        else{ 
          dualSerialPrintln("remote aux8");
        }
      }
      else if(ir_recv_command == 62){
        if(aux7button < 1)aux7button++;
        else dualSerialPrintln("remote aux7");
      }
      else if(ir_recv_command == 61){
        dualSerialPrintln("remote aux6");
      }
      else if(ir_recv_command == 60){
        dualSerialPrintln("remote aud"); // remote aux5
      }
      else if(ir_recv_command == 59){
        dualSerialPrintln("remote col"); // remote aux4
      }
      else if(ir_recv_command == 58){
        dualSerialPrintln("remote aux3");
      }
      else if(ir_recv_command == 57){
        dualSerialPrintln("remote aux2");
      }
      else if(ir_recv_command == 56){
        dualSerialPrintln("remote aux1");
      }
      else if(ir_recv_command == 52){
        dualSerialPrintln("remote res1");
      }
      else if(ir_recv_command == 53){
        dualSerialPrintln("remote res2");
      }
      else if(ir_recv_command == 54){
        dualSerialPrintln("remote res3");
      }
      else if(ir_recv_command == 55){
        dualSerialPrintln("remote res4");
      }
      else if(ir_recv_command == 51){
        dualSerialPrintln("remote res480p");
      }
      else if(ir_recv_command == 50){
        dualSerialPrintln("remote res1440p");
      }
      else if(ir_recv_command == 49){
        dualSerialPrintln("remote res1080p");
      }
      else if(ir_recv_command == 48){
        dualSerialPrintln("remote res4k");
      }
      else if(ir_recv_command == 47){
        dualSerialPrintln("remote buffer");
      }
      else if(ir_recv_command == 44){
        dualSerialPrintln("remote genlock");
      }
      else if(ir_recv_command == 46){
        dualSerialPrintln("remote safe");
      }
      else if(ir_recv_command == 86){
        dualSerialPrintln("remote pause");
      }
      else if(ir_recv_command == 45){
        dualSerialPrintln("remote phase");
      }
      else if(ir_recv_command == 43){
        dualSerialPrintln("remote gain");
      }
      else if(ir_recv_command == 36){
        dualSerialPrintln("remote prof");
      }
      else if(ir_recv_command == 11){
        sendRBP(1);
        if(RT5Xir >= 1){sendIR("5x",1,1);delay(30);sendIR("5x",1,1);} // RT5X profile 1
        if(RT5Xir && OSSCir)delay(500);
        if(OSSCir == 1){sendIR("ossc",1,3);} // OSSC profile 1
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
        sendRBP(11);
        if(OSSCir == 1){sendIR("ossc",11,3);} // OSSC profile 11
      }
      else if(ir_recv_command == 39){
        sendRBP(12);
        if(OSSCir == 1){sendIR("ossc",12,3);} // OSSC profile 12
      }
      else if(ir_recv_command == 35){
        dualSerialPrintln("remote adc");
      }
      else if(ir_recv_command == 34){
        dualSerialPrintln("remote sfx");
      }
      else if(ir_recv_command == 33){
        dualSerialPrintln("remote scaler");
      }
      else if(ir_recv_command == 32){
        dualSerialPrintln("remote output");
      }
      else if(ir_recv_command == 17){
        dualSerialPrintln("remote input");
      }
      else if(ir_recv_command == 41){
        dualSerialPrintln("remote stat");
      }
      else if(ir_recv_command == 40){
        dualSerialPrintln("remote diag");
      }
      else if(ir_recv_command == 66){
        dualSerialPrintln("remote back");
      }
      else if(ir_recv_command == 83){
        dualSerialPrintln("remote ok");
      }
      else if(ir_recv_command == 79){
        dualSerialPrintln("remote right");
      }
      else if(ir_recv_command == 16){
        dualSerialPrintln("remote down");
      }
      else if(ir_recv_command == 87){
        dualSerialPrintln("remote left");
      }
      else if(ir_recv_command == 24){
        dualSerialPrintln("remote up");
      }
      else if(ir_recv_command == 92){
        dualSerialPrintln("remote menu");
      }
      else if(ir_recv_command == 26){ // power button
        Serial.println(F("\rpwr on\r")); // wake
        Serial.println(F("\rremote pwr\r")); // sleep
        #if usbMode
        usbHost.tcmd = "wakesleep";
        #endif
        RTwake = true;
      }
    }
    else if(ir_recv_address == 73 && repeatcount > 4){ // directional buttons have to be held down for just a bit before repeating
      if(ir_recv_command == 24){
        dualSerialPrintln("remote up");
      }
      else if(ir_recv_command == 16){
        dualSerialPrintln("remote down");
      }
      else if(ir_recv_command == 87){
        dualSerialPrintln("remote left");
      }
      else if(ir_recv_command == 79){
        dualSerialPrintln("remote right");
      }
    } // end of if(ir_recv_address
    
    if(ir_recv_address == 73 && repeatcount > 15){ // when directional buttons are held down for even longer... turbo directional mode
      if(ir_recv_command == 87){
        for(uint8_t i=0;i<4;i++){
          dualSerialPrintln("remote left");
        }
      }
      else if(ir_recv_command == 79){
        for(uint8_t i=0;i<4;i++){
          dualSerialPrintln("remote right");
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
      irsend.sendNEC(0x04,0x08,repeat); // Power button
      irsend.sendNEC(0x00,0x00,3);
      delay(50);
      irsend.sendNEC(0x04,0x08,repeat); // send once more
      irsend.sendNEC(0x00,0x00,3);
  }
  
} // end of sendIR()

int fetchGameIDProf(String gameID,int dp){ // looks at gameDB for a gameID -> profile match
  for(int i = 0; i < gameDBSize; i++){      // returns "DefaultProf" for console if nothing found and S0_gameID = true
    if(gameDB[i][1] == gameID){            // returns "-1" (meaning dont change anything) if nothing found and S0_gameID = false
      return gameDB[i][2].toInt();
      break;
   }
  }  

  if(S0_gameID) return dp;
  else return -1;
}  // end of fetchGameIDProf()

String replaceDomainWithIP(String input){
  String result = input;
  int startIndex = 0;
  while(startIndex < result.length()){
    int httpPos = result.indexOf("http://",startIndex); // Look for "http://"
    if(httpPos == -1) break;  // No "http://" found
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
      if(((SVS == 1 || !S0) && currentProf != 0) || (SVS != 1 && S0 && (currentProf != -12 && currentProf != 0))){
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

void DStime(unsigned long eTime){
  DScurrentTime = millis();  // Init timer
  if(DSprevTime == 0)       // If previous timer not initialized, do so now.
    DSprevTime = millis();
  if((DScurrentTime - DSprevTime) >= eTime){ // If it's been longer than eTime, sendProfile() and reset the timer.
    DScurrentTime = 0;
    DSprevTime = 0;
    delaySend = false;
    sendProfile(delayProf,GAMEID1,1);
    delayProf = 0;
 }
} // end of DStime()

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
} // end of recallPreset()

void sendSVS(uint16_t num){
  #if usbMode 
  usbHost.cprof = String(num); 
  #endif
  #if !usbMode
  analogWrite(LED_RED,255);
  analogWrite(LED_BLUE,255);
  analogWrite(LED_GREEN,222);

  Serial.print(F("\rSVS NEW INPUT="));
  if(num != 0)Serial.print(num + offset);
  else Serial.print(num);;
  Serial.println(F("\r"));
  delay(1000);
  Serial.print(F("\rSVS CURRENT INPUT="));
  if(num != 0)Serial.print(num + offset);
  else Serial.print(num);
  Serial.println(F("\r"));

  analogWrite(LED_GREEN,255);
  #endif
  currentProf = num;
} // end of sendSVS()

void sendRBP(int prof){ // send Remote Button Profile
  #if usbMode
  usbHost.cprof = String(-1*prof);
  #endif
  Serial.print(F("\rremote prof"));
  Serial.print(prof);
  Serial.println(F("\r"));
  currentProf = -1*prof; // always store remote button profiles as negative numbers
  #if !usbMode
  analogWrite(LED_RED,255);
  analogWrite(LED_BLUE,255);
  analogWrite(LED_GREEN,222);
  delay(1000);
  analogWrite(LED_GREEN,255);
  #endif
} // end of sendRBP()

void dualSerialPrintln(String str){
  str = "\r" + str + "\r";
  Serial.println(str);
  #if usbMode
  usbHost.tcmd = str;
  #endif
} // end of dualSerialPrintln()

#if !automatrixSW1
void MTVtime1(unsigned long eTime){
  MTVcurrentTime = millis();  // Init timer
  if(MTVprevTime == 0)       // If previous timer not initialized, do so now.
    MTVprevTime = millis();
  if((MTVcurrentTime - MTVprevTime) >= eTime){ // If it's been longer than eTime, send MT-VIKI serial command for current input, see if it responds with disconnected, and reset the timer.
    MTVcurrentTime = 0;
    MTVprevTime = 0;
    extronSerialEwrite("viki",currentMTVinput[0],1);
    delay(50);
 }
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
    delay(50);
 }
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

  delay(50);
} // end of ExtronOutputQuery()

void extronSerialEwrite(String type, uint8_t value, uint8_t sw){
  if(type == "viki"){
    viki[2] = byte(value - 1);
    if(sw == 1)
      extronSerial.write(viki,4);
    else if(sw == 2)
      extronSerial2.write(viki,4);
  }
  else if(type == "tesmart"){
    tesmart[4] = byte(value);
    if(sw == 1)
      extronSerial.write(tesmart,6);
    else if(sw == 2)
      extronSerial2.write(tesmart,6);
  }
  delay(50);
}  // end of extronSerialEwrite()


void sendProfile(int sprof, uint8_t sname, uint8_t soverride){

  // make sure the "No Match Profile" in DonutShop matches it's switch's auto-profile for fallback to work properly
  if(mswitch[GAMEID1].On){
    int gprof = 0;
    int gdprof = 0;
    for(uint8_t i=0;i < consolesSize;i++){
      if(consoles[i].King == 1){
        if(SVS == 0 && sprof > 0 && sprof < 12){
            gdprof = (-1)*consoles[i].DefaultProf;
            gprof = consoles[i].Prof;
        }
        else{
          gdprof = consoles[i].DefaultProf;
          gprof = consoles[i].Prof;
        }
      }
    }
    if(sname != GAMEID1 && sprof == gdprof){
      for(uint8_t i=0;i < mswitchSize;i++){
        if(i != GAMEID1){
          mswitch[i].King = 0;
        }
      }
      delaySend = true; 
      delayProf = gprof;
      return;
    }
  }

  // if GAMEID1 profile is the same as current profile, do not resend profile
  for(uint8_t i=0;i < mswitchSize;i++){
    if(sname == GAMEID1 && i != GAMEID1 && sprof == mswitch[i].Prof && mswitch[i].King == 1){
      mswitch[i].King = 0;
      mswitch[GAMEID1].On = 1;
      mswitch[GAMEID1].King = 1;
      mswitch[GAMEID1].Prof = sprof;
      return;
    }
  }

  if(sprof != 0){
    mswitch[sname].On = 1;
    if(SVS == 0 && sname == EXTRON1 && sprof > 0 && sprof < 12){ // save RBP as negative number
      mswitch[sname].Prof = -1*sprof;
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
    else if(RT5Xir == 3 && sname == EXTRON2 && sprof < 11){
      sendIR("5x",sprof,2); // RT5X profile 1
    }

    if(SVS == 0 && !S0 && sname == EXTRON1 && sprof > 0 && sprof < 13){ sendRBP(sprof); }
    else if(SVS == 0 && S0 && sname == EXTRON1 && sprof > 0 && sprof < 12){ sendRBP(sprof); }
    else if(sprof < 0){ sendRBP(-1*sprof); } // only RBP from GAMEID
    else { sendSVS(sprof); } // everything else
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
    if(count < mswitchSize){ RMTuse = 0; } //This prevents the S0 / remote prof12 profile from constantly overriding any profile loaded with the remote when all consoles are off.
                                          
    if(S0 && !RMTuse && SVS == 0 && (count == mswitchSize) && currentProf != -12){ sendRBP(12); } // send S0 or "remote prof12" when all consoles are off
    else if(S0 && !RMTuse && SVS == 1 && (count == mswitchSize) && currentProf != 0){ sendSVS(0); }

  } // end of else if prof == 0
} // end of sendProfile()

// --------------------- WEB UI FUNCTIONS --------------------- //

void handleGetConsoles(){
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for(int i=0;i<consolesSize;i++){
        JsonObject obj = arr.add<JsonObject>();
        obj["Desc"] = consoles[i].Desc;
        obj["Address"] = consoles[i].Address;
        obj["Prof"] = consoles[i].Prof;
        obj["On"] = consoles[i].On;
        obj["King"] = consoles[i].King;
        obj["DefaultProf"] = consoles[i].DefaultProf;
        obj["Enabled"] = consoles[i].Enabled;
    }

    String out; 
    serializeJson(doc,out);
    server.send(200,"application/json",out);
} // end of handleGetConsoles()

void handleGetGameDB(){
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for(int i=0;i<gameDBSize;i++){
        JsonArray item = arr.add<JsonArray>();
        item.add(gameDB[i][0]);
        item.add(gameDB[i][1]);
        item.add(gameDB[i][2]);
    }
    String out; 
    serializeJson(doc,out);
    server.send(200,"application/json",out);
} // end of handleGetGameDB()

void saveGameDB(){
  File f = LittleFS.open("/gameDB.json", FILE_WRITE);

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  for(int i = 0; i < gameDBSize; i++){
    JsonArray item = arr.add<JsonArray>();
    item.add(gameDB[i][0]);
    item.add(gameDB[i][1]);
    item.add(gameDB[i][2]);
  }

  serializeJson(doc, f);
  f.close();
} // end of saveGameDB()

void handleUpdateGameDB(){
  if(!server.hasArg("plain")){
    server.send(400, "application/json", "{\"error\":\"No data\"}");
    return;
  }

  JsonDocument doc; deserializeJson(doc, server.arg("plain"));
  JsonArray arr = doc.as<JsonArray>();

  gameDBSize = 0;
  for(JsonArray item : arr){
    gameDB[gameDBSize][0] = item[0].as<String>();
    gameDB[gameDBSize][1] = item[1].as<String>();
    gameDB[gameDBSize][2] = item[2].as<String>();
    gameDBSize++;
  }

  saveGameDB();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
} // end of handleUpdateGameDB()

void loadGameDB(){
  if(!LittleFS.exists("/gameDB.json")) return; // if file does not exist yet, load from .ino
  File f = LittleFS.open("/gameDB.json", FILE_READ);

  JsonDocument doc; deserializeJson(doc, f);
  f.close();

  JsonArray arr = doc.as<JsonArray>();
  gameDBSize = 0;
  for(JsonArray item : arr){
    gameDB[gameDBSize][0] = item[0].as<String>();
    gameDB[gameDBSize][1] = item[1].as<String>();
    gameDB[gameDBSize][2] = item[2].as<String>();
    gameDBSize++;
  }
} // end of loadGameDB()

void saveConsoles(){
  File f = LittleFS.open("/consoles.tmp", "w");
  if(!f){
    Serial.println("Failed to open /consoles.tmp for writing");
    return;
  }

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  for(int i = 0; i < consolesSize; i++){
    JsonObject obj = arr.add<JsonObject>();
    obj["Desc"]        = consoles[i].Desc;
    obj["Address"]     = consoles[i].Address;
    obj["DefaultProf"] = consoles[i].DefaultProf;
    obj["Enabled"]      = consoles[i].Enabled;
  }

  serializeJson(doc, f);
  f.close();

  if(LittleFS.exists("/consoles.json")){
    LittleFS.remove("/consoles.json");
  }
  if(!LittleFS.rename("/consoles.tmp", "/consoles.json")){
    Serial.println("Failed to rename /consoles.tmp to /consoles.json");
  }
} // end of saveConsoles()

void handleUpdateConsoles(){
  if(!server.hasArg("plain")){
    server.send(400, "application/json", "{\"error\":\"No data\"}");
    return;
  }

  JsonDocument doc; deserializeJson(doc, server.arg("plain"));
  JsonArray arr = doc.as<JsonArray>();

  int newSize = 0;   // Update consoles array in RAM
  for(JsonObject obj : arr){
    consoles[newSize].Desc        = obj["Desc"].as<String>();
    consoles[newSize].Address     = obj["Address"].as<String>();
    consoles[newSize].DefaultProf = obj["DefaultProf"].as<int>();
    consoles[newSize].Enabled      = obj["Enabled"].as<bool>();
    newSize++;
  }
  consolesSize = newSize;

  saveConsoles();  // save to SPIFFS
  server.send(200, "application/json", "{\"status\":\"ok\"}");
} // end of handleUpdateConsoles()

String embedS0Vars(){
  JsonDocument doc;
  doc["S0_gameID"] = S0_gameID;

  String json;
  serializeJson(doc, json);

  return "let S0Vars = " + json + ";";
} // end of embedS0Vars()

void handleUpdateS0Vars(){
  if(!server.hasArg("plain")){
    server.send(400, "text/plain", "No body");
    return;
  }

  JsonDocument doc; deserializeJson(doc, server.arg("plain"));

  S0_gameID = doc["S0_gameID"].as<bool>();

  saveS0Vars();
  server.send(200, "text/plain", "OK");
} // end of handleUpdateS0Vars()

void saveS0Vars(){
  JsonDocument doc;

  doc["S0_gameID"] = S0_gameID;

  File f = LittleFS.open("/s0vars.json", FILE_WRITE);

  serializeJson(doc, f);
  f.close();
} // end of saveS0Vars()

void loadS0Vars(){
  if (!LittleFS.exists("/s0vars.json")) return; // if file does not exist yet, load from .ino
  File f = LittleFS.open("/s0vars.json", FILE_READ);

  JsonDocument doc; deserializeJson(doc, f);
  f.close();

  S0_gameID = doc["S0_gameID"].as<bool>();
} // end of loadS0Vars()

void handleGetS0Vars(){
  JsonDocument doc;

  doc["S0_gameID"] = S0_gameID;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
} // end of handleGetS0Vars()

void loadConsoles(){
  if(!LittleFS.exists("/consoles.json")) return; // if file does not exist yet, load from .ino
  File f = LittleFS.open("/consoles.json", FILE_READ);

  JsonDocument doc; deserializeJson(doc,f);

  JsonArray arr = doc.as<JsonArray>();
  consolesSize = 0;
  for(JsonObject obj: arr){
      consoles[consolesSize].Desc = obj["Desc"].as<String>();
      consoles[consolesSize].Address = obj["Address"].as<String>();
      consoles[consolesSize].DefaultProf = obj["DefaultProf"].as<int>();
      consoles[consolesSize].Enabled = obj["Enabled"].as<bool>();
      consolesSize++;
  }
  f.close();
} // end of loadConsoles()

void handleGetPayload(){
  server.send(200, "text/plain", payload);
} // end of handleGetPayload()

void handleImportAll() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data\"}");
    return;
  }

  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));

  // Restore consoles
  JsonArray consolesArr = doc["consoles"].as<JsonArray>();
  consolesSize = 0;
  for (JsonObject o : consolesArr) {
    consoles[consolesSize].Desc = o["Desc"].as<String>();
    consoles[consolesSize].Address = o["Address"].as<String>();
    consoles[consolesSize].DefaultProf = o["DefaultProf"].as<int>();
    consoles[consolesSize].Enabled = o["Enabled"].as<bool>();
    consolesSize++;
  }

  saveConsoles();
  delay(200);

  // Restore gameDB
  JsonArray gameArr = doc["gameDB"].as<JsonArray>();
  gameDBSize = 0;
  for (JsonArray item : gameArr) {
    gameDB[gameDBSize][0] = item[0].as<String>();
    gameDB[gameDBSize][1] = item[1].as<String>();
    gameDB[gameDBSize][2] = item[2].as<String>();
    gameDBSize++;
  }
  saveGameDB();

  server.send(200, "application/json", "{\"status\":\"ok\"}");
} // end of handleImportAll()

void handleExportAll() {
  JsonDocument doc;

  JsonArray consolesArr = doc["consoles"].to<JsonArray>();
  for (int i = 0; i < consolesSize; i++) {
    JsonObject o = consolesArr.add<JsonObject>();
    o["Desc"] = consoles[i].Desc;
    o["Address"] = consoles[i].Address;
    o["DefaultProf"] = consoles[i].DefaultProf;
    o["Enabled"] = consoles[i].Enabled;
  }

  JsonArray gameArr = doc["gameDB"].to<JsonArray>();
  for (int i = 0; i < gameDBSize; i++) {
    JsonArray item = gameArr.add<JsonArray>();
    item.add(gameDB[i][0]);
    item.add(gameDB[i][1]);
    item.add(gameDB[i][2]);
  }

  String out;
  serializeJsonPretty(doc, out);
  server.sendHeader("Content-Disposition", "attachment; filename=donutshop_config.json");
  server.send(200, "application/json", out);
} // end of handleExportAll()

void handleRoot(){
  String page = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="utf-8">
    <title>Donut Shop</title>
    <style>
      body { font-family: sans-serif; }
      table { border-collapse: collapse; width: 80%; margin: 20px auto; }
      th, td { border: 1px solid #999; padding: 6px 10px; }
      th { background: #eee; cursor: pointer; user-select: none; }
      input { width: 95%; }
      button { margin: 2px; }
      h2, h3 { text-align: center; }

      #consoleTable td:first-child {
        text-align: center;
        width: 90px;
      }

      #consoleTable td:last-child,
      #profileTable td:last-child {
        width: 90px;
        text-align: center;
      }

      #profileTable input {
        display: block;
        margin: 0 auto;
        width: 97%;
        text-align: left;
      }

      .controls { text-align: center; }
      .arrow { font-size: 0.8em; margin-left: 4px; color: #555; }
      .tooltip {
        position: relative;
        display: inline-flex;
        align-items: center;
        cursor: help;
      }

      .tooltip .tooltip-bubble {
        position: absolute;
        z-index: 20;
        min-width: 180px;
        max-width: 280px;

        background: #2f2f2f;
        color: #fff;
        padding: 6px 10px;
        border-radius: 6px;

        font-size: 0.85em;
        line-height: 1.3;
        text-align: center;

        opacity: 0;
        visibility: hidden;
        transform: translate(-50%, -4px);
        transition: opacity 0.2s ease, transform 0.2s ease;

        left: 50%;
        bottom: 125%;
        pointer-events: none;
      }

      .tooltip:hover .tooltip-bubble,
      .tooltip:focus-within .tooltip-bubble {
        opacity: 1;
        visibility: visible;
        transform: translate(-50%, -8px);
      }

      .tooltip .tooltip-bubble::after {
        content: "";
        position: absolute;
        top: 100%;
        left: 50%;
        transform: translateX(-50%);
        border-width: 6px;
        border-style: solid;
        border-color: #2f2f2f transparent transparent transparent;
      }

      .status-icon {
        display: inline-block;
        width: 10px;
        height: 10px;
        border-radius: 50%;
        margin-right: 6px;
      }

      .leader-icon {
        display: inline-block;
        width: 9px;
        height: 9px;
        border-radius: 0%;
        margin-right: 6px;
        transform: rotate(45deg);
      }

      .leader-icon.on { background-color: #4CAF50; }
      .status-icon.on { background-color: #4CAF50; }
      .status-icon.off { background-color: red; }
      .profile-match td {
        background-color: #4CAF50;
      }

      .profile-match td input {
        -webkit-appearance: none;
        appearance: none;
      }

      .console-prof-cell-match {
        background-color: #4CAF50;
      }

      .topbar {
        position: fixed;
        top: 0;
        right: 0;
        padding: 8px;
      }
      button {
        margin-left: 4px;
      }

      .switch {
        position: relative;
        display: inline-block;
        width: 42px;
        height: 22px;
        vertical-align: middle;
      }

      .switch input {
        opacity: 0;
        width: 0;
        height: 0;
      }

      .slider {
        position: absolute;
        cursor: pointer;
        inset: 0;
        background-color: #ccc;
        transition: 0.25s;
        border-radius: 22px;
      }

      .slider:before {
        position: absolute;
        content: "";
        height: 18px;
        width: 18px;
        left: 2px;
        bottom: 2px;
        background-color: white;
        transition: 0.25s;
        border-radius: 50%;
      }

      .switch input:checked + .slider {
        background-color: #4CAF50;
      }

      .switch input:checked + .slider:before {
        transform: translateX(20px);
      }

      .topbar .tooltip .tooltip-bubble {
        position: absolute;
        bottom: -32px;
        right: 0;
        left: auto;
        transform: translate(0, 0);
        white-space: nowrap;
        z-index: 50;
        min-width: 20px;
      }

      .topbar .tooltip .tooltip-bubble::after {
        content: "";
        position: absolute;
        top: -6px;
        right: 6px;
        left: auto;
        border-width: 6px;
        border-style: solid;
        border-color: transparent transparent #2f2f2f transparent;
      }

      .topbar .tooltip {
        cursor: default;
        position: relative;
      }


    </style>
  </head>

  <body>
  <div class="topbar" style="display:flex; justify-content:flex-end; gap:2px; background:white; padding:8px;">
    <input type="file" id="importJson" style="display:none" accept=".json" onchange="importData(event)">
    <button onclick="document.getElementById('importJson').click()">
      <span class="tooltip">📂<span class="tooltip-bubble">Import Config</span>
      </span>
    </button>
    <button onclick="exportData()">
      <span class="tooltip">💾<span class="tooltip-bubble">Export Config</span>
      </span>
    </button>
  </div>

  <center><h1>Donut Shop</h1></center>
  <div class="controls">
    <button onclick="addConsole()">Add Console</button>
    <span class="tooltip">
      <button onclick="addProfile()">
        Add Profile
      </button>
      <span class="tooltip-bubble">
      Will populate with last gameID found.
      </span>
    </span>
  </div>

  <h2>Consoles</h2>
  <table id="consoleTable">
    <thead>
      <tr>
        <th>
          <span class="tooltip" tabindex="0">
            Enabled
            <span class="tooltip-bubble">
            gameID is checked by cycling through "Enabled" Consoles. 
            There is a 2 second pause after each cycle.
            </span>
          </span>
        </th>
        <th>
          <span class="tooltip" tabindex="0">
            Description
            <span class="tooltip-bubble">
            Any name or description can be used.
            </span>
          </span>
        </th>
        <th>
          <span class="tooltip" tabindex="0">
            Address
            <span class="tooltip-bubble">
            Will automatically get converted to IP if Domain based, in order to timeout after a 2 second gameID query.
            </span>
          </span>
        </th>
        <th>
          <span class="tooltip" tabindex="0">
            No Match Profile
            <span class="tooltip-bubble">
            Profile used when a gameDB entry is not found.
            </span>
          </span>
          <label class="switch" style="margin-left:5px;">
            <input type="checkbox"
                  id="S0_gameID"
                  onchange="updateS0GameID(this)">
            <span class="slider"></span>
          </label>
        </th>
        <th>Action</th>
      </tr>
    </thead>
    <tbody></tbody>
  </table>

  <h2>gameDB</h2>
  <table id="profileTable">
    <thead>
      <tr>
        <th onclick="sortProfiles(0)">
          <span class="tooltip" tabindex="0">
            Name
            <span class="tooltip-bubble">
            Any name or description can be used
            </span>
          </span>
          <span class="arrow" id="arrow0">▲▼</span>
        </th>
        <th onclick="sortProfiles(1)">gameID <span class="arrow" id="arrow1">▲▼</span></th>
        <th onclick="sortProfiles(2)">
          <span class="tooltip" tabindex="0">
            Profile #
            <span class="tooltip-bubble">
            Negative numbers represent a Remote Button Profile. Positive are SVS.
            </span>
          </span>
          <span class="arrow" id="arrow2">▲▼</span>
        </th>

        <th>Action</th>
      </tr>
    </thead>
    <tbody></tbody>
  </table>

  <script>
  let refreshInterval = null;
  let activeProfileNumber = null;
  let updatingConsoles = false;
  let consoles = [];
  let gameProfiles = [];
  let currentSortCol = null;
  let currentSortDir = 'asc';
  

  // inject current S0 values
  )rawliteral";

  page += embedS0Vars();

  page += R"rawliteral(

  // ---------------- LOAD DATA ----------------
  async function loadData(){
    const c = await fetch('/getConsoles');
    consoles = await c.json();

    const g = await fetch('/getGameDB');
    gameProfiles = await g.json();

    renderConsoles();
    consoles.forEach((c, i) => updateStatusIcon(i));
    renderProfiles();
    updateArrows();

    // set initial state of S0_gameID checkbox
    const s0cb = document.getElementById('S0_gameID');
    if (s0cb && S0Vars['S0_gameID'] !== undefined) {
      s0cb.checked = S0Vars['S0_gameID'];
    }
  }

  // ---------------- CONSOLES ----------------
  function renderConsoles(){
    const tbody = document.querySelector('#consoleTable tbody');
    tbody.innerHTML = '';

    consoles.forEach((c, idx) => {
      const tr = document.createElement('tr');

      // --- Enable switch column ---
      const tdEnable = document.createElement('td');

      const label = document.createElement('label');
      label.className = 'switch';

      const enableCheckbox = document.createElement('input');
      enableCheckbox.type = 'checkbox';
      enableCheckbox.checked = !!c.Enabled;
      enableCheckbox.onchange = async () => {
        consoles[idx].Enabled = enableCheckbox.checked;
        await saveConsoles();
      };

      const slider = document.createElement('span');
      slider.className = 'slider';

      label.appendChild(enableCheckbox);
      label.appendChild(slider);
      tdEnable.appendChild(label);
      tr.appendChild(tdEnable);


      // --- Description ---
      const tdDesc = document.createElement('td');
      const descInput = document.createElement('input');
      descInput.type = 'text';
      descInput.value = c.Desc;
      descInput.onchange = async () => {
        consoles[idx].Desc = descInput.value;
        await saveConsoles();
      };
      tdDesc.appendChild(descInput);
      tr.appendChild(tdDesc);

      // --- Address ---
      const tdAddr = document.createElement('td');
      const addrInput = document.createElement('input');
      addrInput.type = 'text';
      addrInput.value = c.Address;
      addrInput.style.width = '87%';
      addrInput.onchange = async () => {
          consoles[idx].Address = addrInput.value;
          await saveConsoles();
      };
      tdAddr.appendChild(addrInput);
      tr.appendChild(tdAddr); 

      // --- DefaultProf ---
      const tdProf = document.createElement('td');
      tdProf.classList.add('console-default-prof-cell');
      const profInput = document.createElement('input');
      profInput.type = 'number';
      profInput.value = c.DefaultProf;
      profInput.classList.add('console-default-prof');
      profInput.onchange = async () => {
        consoles[idx].DefaultProf = parseInt(profInput.value, 10) || 0; 
        if (consoles[idx].King === 1) {
          activeProfileNumber = consoles[idx].DefaultProf;
          highlightProfileRow();
          highlightConsoleProfiles();
        }
        await saveConsoles(); 
      };
      tdProf.appendChild(profInput);
      tr.appendChild(tdProf);

      // --- Delete ---
      const tdDel = document.createElement('td');
      const delBtn = document.createElement('button');
      delBtn.textContent = 'Delete';
      delBtn.onclick = () => deleteConsole(idx);
      tdDel.appendChild(delBtn);
      tr.appendChild(tdDel);
      tbody.appendChild(tr);
    });
  }

  async function saveConsoles(){
    if (updatingConsoles) return;
    updatingConsoles = true;

    const payload = consoles.map(c => ({
      Desc: c.Desc,
      Address: c.Address,
      DefaultProf: c.DefaultProf,
      Enabled: c.Enabled
    }));

    try {
      const res = await fetch('/updateConsoles', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }, // <--- important
        body: JSON.stringify(payload)
      });

      if (!res.ok) {
        console.error('Failed to save consoles:', res.statusText);
      }
    } catch (err) {
      console.error('Network error while saving consoles:', err);
    }

    updatingConsoles = false;
  }

  async function addConsole(){
    consoles.push({
      Desc: "Console Name",
      Address: "http://",
      DefaultProf: 0,
      Enabled: true
    });

    await saveConsoles();
    loadData();
  }

  async function deleteConsole(idx) {
    if (!confirm('Delete this console?')) return;

    consoles.splice(idx, 1);
    await fetch('/updateConsoles', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(consoles)
    });

    renderConsoles();
    consoles.forEach((c, i) => updateStatusIcon(i));
  }

  // ---------------- PROFILES ----------------
  function renderProfiles(){
    const tbody = document.querySelector('#profileTable tbody');
    tbody.innerHTML = '';

    gameProfiles.forEach((p, idx) => {
      const tr = document.createElement('tr');

      const tdName = document.createElement('td');
      const nameInput = document.createElement('input');
      nameInput.value = p[0];
      nameInput.onchange = async () => await saveProfiles();
      tdName.appendChild(nameInput);
      tr.appendChild(tdName);

      const tdGameID = document.createElement('td');
      const gameidInput = document.createElement('input');
      gameidInput.value = p[1];
      gameidInput.onchange = async () => await saveProfiles();
      tdGameID.appendChild(gameidInput);
      tr.appendChild(tdGameID);

      const tdVal = document.createElement('td');
      const valInput = document.createElement('input');
      valInput.type = 'number';
      valInput.value = p[2];
      valInput.onchange = async () => {
        await saveProfiles();
        highlightProfileRow();
        highlightConsoleProfiles();
      };
      tdVal.appendChild(valInput);
      tr.appendChild(tdVal);

      const tdAction = document.createElement('td');
      const delBtn = document.createElement('button');
      delBtn.textContent = 'Delete';
      delBtn.onclick = () => deleteProfile(idx);
      tdAction.appendChild(delBtn);
      tr.appendChild(tdAction);

      tr._gameid = gameidInput;
      tr._name = nameInput;
      tr._val = valInput;

      tbody.appendChild(tr);
    });
  }

  async function saveProfiles(){
    const rows = document.querySelectorAll('#profileTable tbody tr');
    rows.forEach((row, i) => {
      gameProfiles[i][0] = row._name.value;
      gameProfiles[i][1] = row._gameid.value;
      gameProfiles[i][2] = row._val.value;
    });
    await fetch('/updateGameDB', {
      method: 'POST',
      body: JSON.stringify(gameProfiles)
    });
  }

  async function addProfile() {
    const response = await fetch("/getPayload");
    const payload = await response.text();
    gameProfiles.sort((a,b) => 0);
    gameProfiles.unshift(["Current Game", payload, "999"]);
    await fetch('/updateGameDB', {
      method: 'POST',
      body: JSON.stringify(gameProfiles)
    });
    renderProfiles();
    updateArrows();
  }

  async function deleteProfile(idx) {
    if (!confirm('Delete this profile?')) return;
    gameProfiles.splice(idx, 1);
    await fetch('/updateGameDB', {
      method: 'POST',
      body: JSON.stringify(gameProfiles)
    });

    // loadData();

    const resC = await fetch('/getConsoles');
    consoles = await resC.json();

    const resG = await fetch('/getGameDB');
    gameProfiles = await resG.json();

    renderConsoles();
    consoles.forEach((c, i) => updateStatusIcon(i));

    renderProfiles();

    highlightProfileRow();
    highlightConsoleProfiles();

  }

  // ---------------- SORTING ----------------
  function sortProfiles(col) {
    if (currentSortCol === col) {
      currentSortDir = currentSortDir === 'asc' ? 'desc' : 'asc';
    } else {
      currentSortCol = col;
      currentSortDir = 'asc';
    }
    gameProfiles.sort((a, b) => {
      let valA = a[col], valB = b[col];
      if (!isNaN(valA) && !isNaN(valB)) {
        valA = Number(valA);
        valB = Number(valB);
      } else {
        valA = valA.toString().toLowerCase();
        valB = valB.toString().toLowerCase();
      }
      if (valA < valB) return currentSortDir === 'asc' ? -1 : 1;
      if (valA > valB) return currentSortDir === 'asc' ? 1 : -1;
      return 0;
    });
    renderProfiles();
    updateArrows();
  }

  function updateArrows() {
    for (let i = 0; i < 3; i++) {
      const arrow = document.getElementById('arrow' + i);
      if (i === currentSortCol) {
        arrow.textContent = currentSortDir === 'asc' ? '▲' : '▼';
      } else {
        arrow.textContent = '▲▼';
      }
    }
  }

  function updateStatusIcon(idx) {
    const row = document.querySelectorAll('#consoleTable tbody tr')[idx];
    if (!row) return;

    const td = row.children[2]; // Address column
    const input = td.querySelector('input');
    if (!input) return;

    const oldTip = td.querySelector('.tooltip');
    if (oldTip) td.removeChild(oldTip);

    const c = consoles[idx];

    const tip = document.createElement('span');
    tip.className = 'tooltip';

    const icon = document.createElement('span');
    let tipText = '';

    if (c.On === 1 && c.King === 1) {
      icon.className = 'leader-icon on'; // diamond green
      tipText = 'Leader console (active game source)';
    } else if (c.On === 1) {
      icon.className = 'status-icon on'; // circle green
      tipText = 'Address is reachable.';
    } else {
      icon.className = 'status-icon off'; // circle red
      tipText = 'Address is not reachable or Disabled.';
    }

    const bubble = document.createElement('span');
    bubble.className = 'tooltip-bubble';

    if (c.Prof > 0 && c.Prof < 30000) {
      tipText += ` — Profile ${c.Prof}`;
    }
    else if (c.Prof < 0){
      tipText += ` - Remote Button Profile ${c.Prof}`;
    }

    bubble.textContent = tipText;

    tip.appendChild(icon);
    tip.appendChild(bubble);

    td.insertBefore(tip, input);

    if (c.King === 1 && c.Prof !== undefined) {
      activeProfileNumber = parseInt(c.Prof, 10);
      highlightProfileRow();
      highlightConsoleProfiles();
    }
  }

  // ---------------- S0_gameID HANDLER ----------------
  function updateS0GameID(cb) {
    S0Vars['S0_gameID'] = cb.checked;
    saveS0Vars();
  }

  function saveS0Vars() { 
    fetch('/updateS0Vars',{
      method:'POST',
      body:JSON.stringify(S0Vars)
    }); 
  }

  // ---------------- INITIALIZE ----------------
  loadData();

  refreshInterval = setInterval(async () => {
    if (updatingConsoles) return; // skip refresh if user is editing

    try {
      const res = await fetch('/getConsoles');
      const updated = await res.json();

      // Sync the local consoles array with backend
      for (let i = 0; i < Math.min(updated.length, consoles.length); i++) {
        const local = consoles[i];
        const remote = updated[i];

        // Only update fields that the backend controls
        local.On = remote.On;
        local.Enabled = remote.Enabled;
        local.King = remote.King;
        local.Prof = remote.Prof;
      }

      // If new consoles were added or removed, replace the array
      if (updated.length !== consoles.length) {
        consoles = updated;
      }

      // Update icons and highlights
      consoles.forEach((c, i) => updateStatusIcon(i)); // Update icons

      // Track King console
      const king = consoles.find(c => c.King === 1);
      activeProfileNumber = king ? parseInt(king.Prof || 0, 10) : null;

      highlightProfileRow();
      highlightConsoleProfiles();

    } catch (err) {
      console.error("Error refreshing consoles:", err);
    }
  }, 2500);

  // ---------------- HIGHLIGHT FUNCTIONS ----------------
  function highlightProfileRow() {
    const rows = document.querySelectorAll('#profileTable tbody tr');

    rows.forEach(row => {
      row.classList.remove('profile-match');

      const valInput = row._val;
      if (!valInput) return;

      const profNum = parseInt(valInput.value, 10);

      // Only highlight gameDB if activeProfileNumber does NOT match any console DefaultProf
      const consoleMatch = consoles.some(c => parseInt(c.DefaultProf, 10) === activeProfileNumber);
      if (!consoleMatch && profNum === activeProfileNumber) {
        row.classList.add('profile-match');
      }
    });
  }

  function highlightConsoleProfiles() {
    const rows = document.querySelectorAll('#consoleTable tbody tr');

    rows.forEach((row, i) => {
      const cell = row.children[3]; // DefaultProf column
      if (!cell) return;

      const input = cell.querySelector('input');
      if (!input) return;

      // Reset background
      cell.style.backgroundColor = '';

      const c = consoles[i];
      if (!c) return;

      // Highlight if DefaultProf matches activeProfileNumber
      if (parseInt(c.DefaultProf, 10) === activeProfileNumber) {
        cell.style.backgroundColor = '#4CAF50';
      }
    });
  }

  // ---------------- EXPORT / IMPORT FUNCTIONS ----------------
  function exportData() {
    window.open('/exportAll', '_blank');
  }

  function importData(event) {
    const file = event.target.files[0];
    if (!file) return;

    //Pause background polling during import
    if (refreshInterval) {
      clearInterval(refreshInterval);
      refreshInterval = null;
    }

    const reader = new FileReader();
    reader.onload = () => {
      fetch('/importAll', {
        method: 'POST',
        body: reader.result,
        headers: {'Content-Type': 'application/json'}
      })
      .then(resp => {
        if (!resp.ok) throw new Error("Import failed");
        alert('Import successful!');
        window.location.reload(); // reload restarts interval cleanly
      })
      .catch(err => {
        alert('Import failed: ' + err);

        //Resume polling if import fails
        if (!refreshInterval) {
          refreshInterval = setInterval(() => location.reload(), 2500);
        }
      });
    };
    reader.readAsText(file);
  }

  </script>

  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", page);
} // end of handleRoot()

#if automatrixSW1 || automatrixSW2
uint8_t readAMstate(String& sinput, uint8_t size){

  uint32_t newAMstate = 0;
  for(uint8_t i=0;i < size;i++){
    char c = sinput.charAt(i);
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
  return AMstate[AMstateTop];
} // end of readAMstate()
#endif
