(() => {
  const ARDUINO_VID = 0x2341;
  const NANO_PID = 0x0070;

  // USB Device Firmware Upgrade class codes.
  const DFU_CLASS = 0xfe;
  const DFU_SUBCLASS = 0x01;

  const btn = document.getElementById("testBtn");
  const terminal = document.getElementById("terminal");
  const summary = document.getElementById("summary");

  function ts(){
    return new Date().toLocaleTimeString();
  }

  function log(msg = ""){
    const line = `[${ts()}] ${msg}`;
    terminal.textContent += line + "\n";
    terminal.scrollTop = terminal.scrollHeight;
    console.log("[DFU TEST]", msg);
  }

  function hex(value, width = 2){
    return "0x" + Number(value ?? 0).toString(16).toUpperCase().padStart(width, "0");
  }

  function setSummary(kind, text){
    summary.hidden = false;
    summary.className = `summary ${kind}`;
    summary.textContent = text;
  }

  function resetOutput(){
    terminal.textContent = "";
    summary.hidden = true;
    summary.className = "summary";
    summary.textContent = "";
  }

  function describeEndpoint(ep){
    return `${ep.direction} ${ep.type} ep=${ep.endpointNumber} packetSize=${ep.packetSize}`;
  }

  function enumerateInterfaces(device){
    const rows = [];

    for(const cfg of device.configurations || []){
      log(`Configuration ${cfg.configurationValue}: ${cfg.configurationName || "(no name)"}`);

      for(const iface of cfg.interfaces || []){
        for(const alt of iface.alternates || []){
          const isDfu = alt.interfaceClass === DFU_CLASS &&
                        alt.interfaceSubclass === DFU_SUBCLASS;

          const info = {
            configurationValue: cfg.configurationValue,
            interfaceNumber: iface.interfaceNumber,
            alternateSetting: alt.alternateSetting,
            interfaceClass: alt.interfaceClass,
            interfaceSubclass: alt.interfaceSubclass,
            interfaceProtocol: alt.interfaceProtocol,
            interfaceName: alt.interfaceName || "",
            isDfu,
            endpoints: (alt.endpoints || []).map(describeEndpoint)
          };

          rows.push(info);

          log(
            `  IF ${iface.interfaceNumber} ALT ${alt.alternateSetting}` +
            ` class=${hex(alt.interfaceClass)}` +
            ` subclass=${hex(alt.interfaceSubclass)}` +
            ` protocol=${hex(alt.interfaceProtocol)}` +
            `${alt.interfaceName ? ` name="${alt.interfaceName}"` : ""}` +
            `${isDfu ? "  <== DFU" : ""}`
          );

          for(const endpoint of info.endpoints){
            log(`      ${endpoint}`);
          }
        }
      }
    }

    return rows;
  }

  async function safelyClose(device, claimed){
    for(const ifaceNum of claimed){
      try{
        await device.releaseInterface(ifaceNum);
        log(`Released interface ${ifaceNum}.`);
      }
      catch(e){
        log(`Release interface ${ifaceNum} failed: ${e.name}: ${e.message}`);
      }
    }

    if(device.opened){
      try{
        await device.close();
        log("USB device closed.");
      }
      catch(e){
        log(`USB close failed: ${e.name}: ${e.message}`);
      }
    }
  }

  async function run(){
    resetOutput();

    if(!("usb" in navigator)){
      setSummary("bad", "WebUSB is not available in this browser. Use Chrome or Edge over HTTPS.");
      log("ERROR: navigator.usb is unavailable.");
      return;
    }

    btn.disabled = true;
    btn.textContent = "Testing…";

    let device = null;
    const claimed = [];

    try{
      log("Requesting Arduino Nano ESP32 recovery device (2341:0070)…");

      device = await navigator.usb.requestDevice({
        filters: [{
          vendorId: ARDUINO_VID,
          productId: NANO_PID
        }]
      });

      log(
        `Selected ${device.productName || "(unnamed device)"} ` +
        `${hex(device.vendorId,4).slice(2)}:${hex(device.productId,4).slice(2)}`
      );
      log(`Manufacturer: ${device.manufacturerName || "(none)"}`);
      log(`Serial: ${device.serialNumber || "(none)"}`);
      log(`USB version: ${device.usbVersionMajor}.${device.usbVersionMinor}.${device.usbVersionSubminor}`);
      log(`Device class: ${hex(device.deviceClass)} subclass=${hex(device.deviceSubclass)} protocol=${hex(device.deviceProtocol)}`);
      log("");
      log("Descriptors visible before open:");
      const rowsBefore = enumerateInterfaces(device);

      const dfuBefore = rowsBefore.filter(x => x.isDfu);
      log("");
      log(`DFU alternate descriptors found: ${dfuBefore.length}`);

      log("");
      log("Calling device.open()…");
      await device.open();
      log("SUCCESS: WebUSB device.open() succeeded.");

      if(!device.configuration){
        const preferred = device.configurations?.[0]?.configurationValue ?? 1;
        log(`No active configuration. Selecting configuration ${preferred}…`);
        await device.selectConfiguration(preferred);
        log(`SUCCESS: configuration ${preferred} selected.`);
      }
      else{
        log(`Active configuration: ${device.configuration.configurationValue}.`);
      }

      log("");
      log("Active configuration interfaces:");
      const rows = enumerateInterfaces(device);

      const dfuRows = rows.filter(
        x => x.isDfu &&
             x.configurationValue === device.configuration.configurationValue
      );

      if(!dfuRows.length){
        setSummary(
          "warn",
          "WebUSB opened the device, but no USB DFU interface (class FE / subclass 01) was found in the active descriptors."
        );
        log("");
        log("RESULT: Device opened, but no DFU interface was identified.");
        return;
      }

      const interfaceNumbers = [...new Set(dfuRows.map(x => x.interfaceNumber))];
      log("");
      log(`DFU interface number(s): ${interfaceNumbers.join(", ")}`);

      let claimSuccess = 0;

      for(const ifaceNum of interfaceNumbers){
        log("");
        log(`Attempting claimInterface(${ifaceNum})…`);

        try{
          await device.claimInterface(ifaceNum);
          claimed.push(ifaceNum);
          claimSuccess++;
          log(`SUCCESS: claimed DFU interface ${ifaceNum}.`);

          const iface = device.configuration.interfaces.find(
            x => x.interfaceNumber === ifaceNum
          );

          if(iface){
            log(`Current alternate setting: ${iface.alternate.alternateSetting}.`);
            log(
              `Current interface protocol: ${hex(iface.alternate.interfaceProtocol)} ` +
              `(${iface.alternate.interfaceProtocol === 0x01 ? "DFU runtime" :
                  iface.alternate.interfaceProtocol === 0x02 ? "DFU mode" : "other"})`
            );
          }
        }
        catch(e){
          log(`FAILED: claimInterface(${ifaceNum}) -> ${e.name}: ${e.message}`);
        }
      }

      log("");

      if(claimSuccess > 0){
        log("RESULT: WebUSB can open the Nano recovery device AND claim a DFU interface.");
        setSummary(
          "good",
          "SUCCESS: This Windows/Chrome setup can access the Nano ESP32 DFU interface through WebUSB. A browser DFU handoff is viable for further testing."
        );
      }
      else{
        log("RESULT: WebUSB opened the device, but Windows/Chrome could not claim its DFU interface.");
        setSummary(
          "warn",
          "PARTIAL: Chrome can open 2341:0070 through WebUSB, but it cannot claim the DFU interface. This usually means Windows has another driver bound to that interface instead of a WebUSB-compatible WinUSB/libusb driver."
        );
      }
    }
    catch(e){
      log("");
      log(`ERROR: ${e.name}: ${e.message}`);

      if(e.name === "NotFoundError"){
        setSummary(
          "warn",
          "No device was selected. Double-RST the Nano so the green LED strobes, then try again."
        );
      }
      else if(e.name === "SecurityError"){
        setSummary(
          "bad",
          "Chrome blocked WebUSB access. Make sure the page is served over HTTPS and WebUSB is allowed."
        );
      }
      else if(e.name === "NetworkError"){
        setSummary(
          "bad",
          "WebUSB found the device but Windows could not open or claim it. The terminal output above tells us which operation failed."
        );
      }
      else{
        setSummary(
          "bad",
          `${e.name}: ${e.message}`
        );
      }
    }
    finally{
      if(device){
        await safelyClose(device, claimed);
      }

      btn.disabled = false;
      btn.textContent = "Test WebUSB DFU";
    }
  }

  btn.addEventListener("click", run);

  log("Ready.");
  log("Double-RST the Nano ESP32 so the green LED strobes, then click “Test WebUSB DFU”.");
})();
