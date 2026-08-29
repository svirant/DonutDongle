(() => {
  const ARDUINO_VID = 0x2341;
  const NANO_PID = 0x0070;
  const ESPRESSIF_VID = 0x303A;
  const USB_JTAG_PID = 0x1001;

  const DFU_CLASS = 0xFE;
  const DFU_SUBCLASS = 0x01;

  const DFU_DNLOAD = 1;
  const DFU_GETSTATUS = 3;
  const DFU_CLRSTATUS = 4;
  const DFU_ABORT = 6;

  const STATE_DFU_IDLE = 2;
  const STATE_DFU_DNLOAD_SYNC = 3;
  const STATE_DFU_DNBUSY = 4;
  const STATE_DFU_DNLOAD_IDLE = 5;
  const STATE_DFU_MANIFEST_SYNC = 6;
  const STATE_DFU_MANIFEST = 7;
  const STATE_DFU_MANIFEST_WAIT_RESET = 8;
  const STATE_DFU_ERROR = 10;

  const fileInput = document.getElementById("helperFile");
  const fileInfo = document.getElementById("fileInfo");
  const runBtn = document.getElementById("runBtn");
  const checkBtn = document.getElementById("checkBtn");
  const terminal = document.getElementById("terminal");
  const summary = document.getElementById("summary");

  let helperFile = null;

  function ts(){ return new Date().toLocaleTimeString(); }
  function log(msg=""){
    terminal.textContent += `[${ts()}] ${msg}\n`;
    terminal.scrollTop = terminal.scrollHeight;
    console.log("[DFU→JTAG]", msg);
  }
  function hex(v,w=4){ return Number(v ?? 0).toString(16).toUpperCase().padStart(w,"0"); }
  function sleep(ms){ return new Promise(r => setTimeout(r, ms)); }
  function setSummary(kind,text){ summary.hidden=false; summary.className=`summary ${kind}`; summary.textContent=text; }
  function clearSummary(){ summary.hidden=true; summary.className="summary"; summary.textContent=""; }
  function pct(done,total){ return Math.min(100, Math.floor((done*100)/Math.max(total,1))); }

  fileInput.addEventListener("change", () => {
    helperFile = fileInput.files?.[0] || null;
    if(!helperFile){
      fileInfo.innerHTML = 'Select the compiled helper <b>.ino.bin</b>.';
      return;
    }
    const kb = Math.round(helperFile.size / 1024);
    fileInfo.textContent = `${helperFile.name} — ${kb} KB`;
  });

  function findDfuInterface(device){
    for(const cfg of device.configurations || []){
      for(const iface of cfg.interfaces || []){
        for(const alt of iface.alternates || []){
          if(alt.interfaceClass === DFU_CLASS && alt.interfaceSubclass === DFU_SUBCLASS){
            return { configurationValue: cfg.configurationValue, interfaceNumber: iface.interfaceNumber };
          }
        }
      }
    }
    return null;
  }

  async function readConfigDescriptor(device){
    // Read the first 9 bytes to obtain wTotalLength, then fetch the entire descriptor.
    const head = await device.controlTransferIn({
      requestType:"standard", recipient:"device", request:6, value:(2 << 8), index:0
    }, 9);
    if(head.status !== "ok" || !head.data || head.data.byteLength < 9) throw new Error("Could not read USB configuration descriptor header.");
    const hv = new DataView(head.data.buffer, head.data.byteOffset, head.data.byteLength);
    const total = hv.getUint16(2, true);
    const full = await device.controlTransferIn({
      requestType:"standard", recipient:"device", request:6, value:(2 << 8), index:0
    }, total);
    if(full.status !== "ok" || !full.data) throw new Error("Could not read full USB configuration descriptor.");
    return new Uint8Array(full.data.buffer, full.data.byteOffset, full.data.byteLength);
  }

  function parseDfuFunctionalDescriptor(bytes){
    for(let i=0; i+1<bytes.length; ){
      const len = bytes[i];
      const type = bytes[i+1];
      if(!len) break;
      if(type === 0x21 && len >= 9){
        return {
          attributes: bytes[i+2],
          detachTimeout: bytes[i+3] | (bytes[i+4] << 8),
          transferSize: bytes[i+5] | (bytes[i+6] << 8),
          version: bytes[i+7] | (bytes[i+8] << 8)
        };
      }
      i += len;
    }
    return null;
  }

  async function dfuStatus(device, iface){
    const r = await device.controlTransferIn({
      requestType:"class", recipient:"interface", request:DFU_GETSTATUS, value:0, index:iface
    }, 6);
    if(r.status !== "ok" || !r.data || r.data.byteLength < 6) throw new Error(`DFU GETSTATUS failed (${r.status}).`);
    const b = new Uint8Array(r.data.buffer, r.data.byteOffset, 6);
    return { status:b[0], pollTimeout:b[1] | (b[2]<<8) | (b[3]<<16), state:b[4], iString:b[5] };
  }

  async function clearDfuError(device, iface){
    let st = await dfuStatus(device, iface);
    if(st.state === STATE_DFU_ERROR){
      log(`DFU is in error state (status=${st.status}); clearing status…`);
      const r = await device.controlTransferOut({
        requestType:"class", recipient:"interface", request:DFU_CLRSTATUS, value:0, index:iface
      });
      if(r.status !== "ok") throw new Error(`DFU CLRSTATUS failed (${r.status}).`);
      st = await dfuStatus(device, iface);
    }
    if(st.state !== STATE_DFU_IDLE && st.state !== STATE_DFU_DNLOAD_IDLE){
      try{
        await device.controlTransferOut({ requestType:"class", recipient:"interface", request:DFU_ABORT, value:0, index:iface });
        st = await dfuStatus(device, iface);
      }
      catch(e){}
    }
    return st;
  }

  async function waitForDownloadIdle(device, iface){
    for(let n=0;n<100;n++){
      const st = await dfuStatus(device, iface);
      if(st.status !== 0) throw new Error(`DFU status error ${st.status}, state ${st.state}.`);
      if(st.state === STATE_DFU_DNLOAD_IDLE || st.state === STATE_DFU_IDLE) return st;
      if(st.state === STATE_DFU_ERROR) throw new Error("DFU entered error state.");
      if(st.state !== STATE_DFU_DNLOAD_SYNC && st.state !== STATE_DFU_DNBUSY){
        throw new Error(`Unexpected DFU state ${st.state} while downloading.`);
      }
      await sleep(Math.max(1, st.pollTimeout));
    }
    throw new Error("Timed out waiting for DFU download block.");
  }

  async function manifest(device, iface, blockNumber){
    log("Sending final zero-length DFU block…");
    const out = await device.controlTransferOut({
      requestType:"class", recipient:"interface", request:DFU_DNLOAD, value:blockNumber, index:iface
    });
    if(out.status !== "ok") throw new Error(`Final DFU DNLOAD failed (${out.status}).`);

    // The Nano may disconnect during manifestation/final-detach. That is success-compatible.
    for(let n=0;n<80;n++){
      try{
        const st = await dfuStatus(device, iface);
        log(`Manifest state=${st.state}, status=${st.status}, poll=${st.pollTimeout}ms`);
        if(st.status !== 0) throw new Error(`DFU manifestation error status ${st.status}.`);
        if(st.state === STATE_DFU_MANIFEST_WAIT_RESET) return;
        if(st.state === STATE_DFU_IDLE) return;
        if(st.state !== STATE_DFU_MANIFEST_SYNC && st.state !== STATE_DFU_MANIFEST){
          // Some manifestation-tolerant implementations return to idle quickly.
          if(st.state === STATE_DFU_DNLOAD_IDLE) return;
        }
        await sleep(Math.max(10, st.pollTimeout));
      }
      catch(e){
        if(!device.opened) return;
        // USB disconnection commonly surfaces as NetworkError here.
        if(e?.name === "NetworkError" || /disconnected|device/i.test(String(e?.message || ""))) return;
        throw e;
      }
    }
  }

  async function uploadHelper(){
    clearSummary();
    if(!helperFile){
      setSummary("warn", "Select the compiled DFU_to_USB_JTAG.ino.bin file first.");
      return;
    }
    if(!("usb" in navigator)){
      setSummary("bad", "WebUSB is not available. Use Chrome or Edge over HTTPS.");
      return;
    }

    runBtn.disabled = true;
    checkBtn.disabled = true;
    let device = null;
    let iface = null;
    let claimed = false;

    try{
      const firmware = new Uint8Array(await helperFile.arrayBuffer());
      if(firmware.length < 1000 || firmware.length > 3*1024*1024){
        throw new Error(`Helper size ${firmware.length} bytes is not plausible. Select the plain .ino.bin application image.`);
      }

      log(`Helper selected: ${helperFile.name} (${firmware.length} bytes).`);
      log("Put the Nano in double-RST recovery mode (green LED pulsing).`".slice(0,-1));
      log("Requesting Nano ESP32 recovery DFU (2341:0070)…");

      device = await navigator.usb.requestDevice({ filters:[{vendorId:ARDUINO_VID, productId:NANO_PID}] });
      log(`Selected ${device.productName || "Nano"} ${hex(device.vendorId)}:${hex(device.productId)}.`);

      iface = findDfuInterface(device);
      if(!iface) throw new Error("No DFU interface found on the selected device.");
      log(`DFU interface ${iface.interfaceNumber}.`);

      await device.open();
      log("WebUSB open succeeded.");
      if(!device.configuration){
        await device.selectConfiguration(iface.configurationValue);
      }
      await device.claimInterface(iface.interfaceNumber);
      claimed = true;
      log("DFU interface claimed.");

      let transferSize = 4096;
      try{
        const rawCfg = await readConfigDescriptor(device);
        const fd = parseDfuFunctionalDescriptor(rawCfg);
        if(fd){
          log(`DFU descriptor: attributes=0x${hex(fd.attributes,2)}, transferSize=${fd.transferSize}, detachTimeout=${fd.detachTimeout}ms, bcdDFU=0x${hex(fd.version)}.`);
          log(`DFU flags: download=${!!(fd.attributes & 0x01)}, upload=${!!(fd.attributes & 0x02)}, manifestationTolerant=${!!(fd.attributes & 0x04)}, willDetach=${!!(fd.attributes & 0x08)}.`);
          if(fd.transferSize > 0) transferSize = Math.min(fd.transferSize, 4096);
        }
        else{
          log("DFU functional descriptor not parsed; using 4096-byte blocks.");
        }
      }
      catch(e){
        log(`Descriptor read note: ${e.message} Using 4096-byte blocks.`);
      }
      if(transferSize < 64) transferSize = 64;
      log(`Using ${transferSize}-byte DFU blocks.`);

      const initial = await clearDfuError(device, iface.interfaceNumber);
      log(`Initial DFU state=${initial.state}, status=${initial.status}.`);
      if(initial.status !== 0) throw new Error(`DFU not ready (status ${initial.status}).`);

      let block = 0;
      let lastPct = -1;
      for(let off=0; off<firmware.length; off += transferSize, block++){
        const chunk = firmware.subarray(off, Math.min(off + transferSize, firmware.length));
        const out = await device.controlTransferOut({
          requestType:"class", recipient:"interface", request:DFU_DNLOAD,
          value:block, index:iface.interfaceNumber
        }, chunk);
        if(out.status !== "ok") throw new Error(`DFU block ${block} failed (${out.status}).`);
        await waitForDownloadIdle(device, iface.interfaceNumber);

        const p = pct(Math.min(off + chunk.length, firmware.length), firmware.length);
        if(p >= lastPct + 5 || p === 100){
          lastPct = p;
          log(`Uploading helper: ${p}%`);
        }
      }

      log("Helper upload complete; requesting manifestation/final detach…");
      await manifest(device, iface.interfaceNumber, block);

      // Try to replace the physical RST press with a host-issued USB reset.
      // On Chrome this maps to WebUSB USBDevice.reset(). If the Nano recovery
      // firmware treats the USB bus reset as sufficient to boot the selected
      // application, the helper should immediately run and force 303A:1001.
      if(device.opened){
        try{
          log("Attempting WebUSB device.reset() to leave DFU without pressing RST…");
          await device.reset();
          log("WebUSB device.reset() completed.");
          await sleep(500);
        }
        catch(e){
          // A reset can make the old USB handle disappear; NetworkError is
          // therefore compatible with the device having reset successfully.
          log(`WebUSB reset result: ${e.name || "Error"}: ${e.message || e}`);
        }
      }

      try{
        if(device.opened && claimed){ await device.releaseInterface(iface.interfaceNumber); claimed=false; }
      }
      catch(e){}
      try{ if(device.opened) await device.close(); }catch(e){}

      log("DFU transfer/reset finished. Waiting for helper to boot and enter ROM USB Serial/JTAG…");
      await waitForAuthorizedJtag(12000);
    }
    catch(e){
      log(`ERROR: ${e.name || "Error"}: ${e.message || e}`);
      setSummary("bad", `${e.name || "Error"}: ${e.message || e}`);
    }
    finally{
      try{
        if(device?.opened && claimed && iface) await device.releaseInterface(iface.interfaceNumber);
      }catch(e){}
      try{ if(device?.opened) await device.close(); }catch(e){}
      runBtn.disabled = false;
      checkBtn.disabled = false;
    }
  }

  async function getAuthorizedJtag(){
    if(!("serial" in navigator)) return [];
    const ports = await navigator.serial.getPorts();
    return ports.filter(p => {
      const i = p.getInfo();
      return i.usbVendorId === ESPRESSIF_VID && i.usbProductId === USB_JTAG_PID;
    });
  }

  async function waitForAuthorizedJtag(timeoutMs){
    const start = Date.now();
    while(Date.now() - start < timeoutMs){
      const ports = await getAuthorizedJtag();
      if(ports.length === 1){
        log("SUCCESS: authorized 303A:1001 Espressif USB JTAG/serial debug unit detected.");
        setSummary("good", "SUCCESS: DFU helper booted and the Nano re-enumerated as 303A:1001 USB Serial/JTAG.");
        return true;
      }
      if(ports.length > 1){
        log("303A:1001 is present, but more than one authorized device matches.");
        setSummary("good", "USB Serial/JTAG is present. More than one previously authorized 303A:1001 device was found.");
        return true;
      }
      await sleep(250);
    }
    log("303A:1001 was not visible through navigator.serial.getPorts(). It may simply not be authorized to this site yet.");
    log("Click “Check USB JTAG” to open Chrome's serial chooser and verify it manually.");
    setSummary("warn", "DFU completed. Click “Check USB JTAG” and select the Espressif USB JTAG/serial debug unit if it appears.");
    return false;
  }

  async function checkJtag(){
    clearSummary();
    if(!("serial" in navigator)){
      setSummary("bad", "Web Serial is unavailable in this browser.");
      return;
    }
    try{
      log("Requesting 303A:1001 USB Serial/JTAG…");
      const port = await navigator.serial.requestPort({ filters:[{usbVendorId:ESPRESSIF_VID, usbProductId:USB_JTAG_PID}] });
      const i = port.getInfo();
      log(`SUCCESS: selected ${hex(i.usbVendorId)}:${hex(i.usbProductId)}.`);
      setSummary("good", "SUCCESS: The Nano is in ESP32-S3 ROM USB Serial/JTAG mode (303A:1001). The DFU → JTAG handoff works.");
    }
    catch(e){
      log(`USB JTAG check: ${e.name}: ${e.message}`);
      if(e.name === "NotFoundError") setSummary("warn", "No USB JTAG device was selected/found.");
      else setSummary("bad", `${e.name}: ${e.message}`);
    }
  }

  runBtn.addEventListener("click", uploadHelper);
  checkBtn.addEventListener("click", checkJtag);

  log("Ready.");
  log("Compile the included helper sketch, select its plain .ino.bin above, then double-RST the Nano and click Upload Helper.");
})();
