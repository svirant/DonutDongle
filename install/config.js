window.DONUTSHOP_INSTALLER_CONFIG = {
  firmware: {
    fullAddress: 0x000000,
    recoveryAddress: 0xF70000,
    flashSizeBytes: 16 * 1024 * 1024
  },

  device: {
    recoveryVendorId: 0x2341,
    recoveryProductId: 0x0070,

    bootVendorId: 0x303A,
    bootProductId: 0x1001,

    expectedChip: "ESP32-S3",
    flashBaud: 921600,

    // If the USB JTAG device was previously authorized, the installer can
    // detect it automatically after the user presses RST once.
    jtagWatchMs: 60000
  }
};
