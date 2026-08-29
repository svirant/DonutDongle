#include <Arduino.h>
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include "esp_system.h"

void setup(){
  // The Nano recovery DFU loader boots this temporary application after
  // the user presses RST once. Immediately reboot into the ESP32-S3 ROM
  // USB Serial/JTAG downloader (303A:1001).
  REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
  esp_restart();
}

void loop(){
}
