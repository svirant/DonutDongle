#include <Arduino.h>
#include "esp32-hal-tinyusb.h"

void setup(){
  // Test Espressif's USB-aware reboot path instead of directly setting
  // RTC_CNTL_FORCE_DOWNLOAD_BOOT + esp_restart().
  //
  // On ESP32-S3, this switches the USB PHY to the integrated
  // USB Serial/JTAG controller, prepares ROM download boot, then restarts.
  usb_persist_restart(RESTART_BOOTLOADER);
}

void loop(){
}
