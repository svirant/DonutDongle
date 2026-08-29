// DonutShop Installer configuration
// Edit only the values in this file before publishing.
window.DONUTSHOP_INSTALLER_CONFIG = {
  github: {
    // Firmware Release source. The GitHub Action mirrors assets from this repository.
    owner: "svirant",
    repo: "DonutDongle",

    // Informational only in the browser; the workflow always mirrors the latest published Release.
    release: "latest"
  },

  firmware: {
    // If exactly one Release asset ends in this suffix, it will be selected.
    fullBinSuffix: "_full.bin",

    // Optional exact filename. Leave blank to use fullBinSuffix above.
    fullBinName: "",

    recoveryName: "nora_recovery.bin",

    fullAddress: 0x000000,
    recoveryAddress: 0xF70000,
    flashSizeBytes: 16 * 1024 * 1024
  },

  device: {
    // Factory Arduino Nano ESP32 USB identity (TinyUSB CDC).
    nanoVendorId: 0x2341,
    nanoProductId: 0x0070,

    // ESP32-S3 built-in USB Serial/JTAG ROM downloader identity.
    bootVendorId: 0x303A,
    bootProductId: 0x1001,

    expectedChip: "ESP32-S3",

    // 1200 baud is only the factory-bootloader trigger.
    triggerBaud: 1200,

    // esptool-js connection setting after the board is in ROM download mode.
    flashBaud: 921600
  }
};
