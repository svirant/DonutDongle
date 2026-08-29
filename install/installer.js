import { ESPLoader, Transport } from "https://unpkg.com/esptool-js@0.6.1/bundle.js";

const cfg = window.DONUTSHOP_INSTALLER_CONFIG;

const $ = (id) => document.getElementById(id);
const ui = {
  browserWarning: $("browserWarning"),
  releaseBadge: $("releaseBadge"),
  releaseVersion: $("releaseVersion"),
  releaseDate: $("releaseDate"),
  releaseLink: $("releaseLink"),
  fullName: $("fullName"),
  recoveryName: $("recoveryName"),
  helperName: $("helperName"),
  startButton: $("startButton"),
  statusDot: $("statusDot"),
  fullProgress: $("fullProgress"),
  recoveryProgress: $("recoveryProgress"),
  fullPercent: $("fullPercent"),
  recoveryPercent: $("recoveryPercent"),
  successBox: $("successBox"),
  errorBox: $("errorBox"),
  consoleOutput: $("consoleOutput")
};

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

let images = null;
let busy = false;
let stage = "connect";
let watchTimer = null;
let watchDeadline = 0;
let lastProgressLog = [-10, -10];

function log(message){
  const line = `[${new Date().toLocaleTimeString()}] ${message}`;
  console.log(line);
  ui.consoleOutput.textContent += `${line}\n`;
  ui.consoleOutput.scrollTop = ui.consoleOutput.scrollHeight;
}

function sleep(ms){
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function hex4(value){
  if(value === undefined) return "????";
  return Number(value).toString(16).toUpperCase().padStart(4, "0");
}

function bytesText(bytes){
  if(!Number.isFinite(bytes)) return "—";
  const units = ["B", "KB", "MB", "GB"];
  let value = bytes;
  let unit = 0;
  while(value >= 1024 && unit < units.length - 1){
    value /= 1024;
    unit++;
  }
  return `${value >= 10 || unit === 0 ? value.toFixed(0) : value.toFixed(1)} ${units[unit]}`;
}

function formatDate(iso){
  if(!iso) return "—";
  const date = new Date(iso);
  return new Intl.DateTimeFormat(undefined, {
    year: "numeric",
    month: "long",
    day: "numeric"
  }).format(date);
}

function setStatus(kind){
  ui.statusDot.className = "status-dot" + (kind ? ` ${kind}` : "");
}

function clearError(){
  ui.errorBox.textContent = "";
  ui.errorBox.classList.add("hidden");
}

function showError(message){
  ui.errorBox.textContent = message;
  ui.errorBox.classList.remove("hidden");
  ui.successBox.classList.add("hidden");
  setStatus("bad");
  log(`ERROR: ${message}`);
}

function setBusy(value){
  busy = value;
  if(value){
    ui.startButton.disabled = true;
    setStatus("busy");
  }
  else{
    updateButtonForStage();
  }
}

function updateButtonForStage(){
  ui.startButton.classList.remove("reset-cue", "reset-done");

  if(!images){
    ui.startButton.disabled = true;
    ui.startButton.textContent = "Connect and Flash";
    return;
  }

  if(stage === "connect"){
    ui.startButton.disabled = false;
    ui.startButton.textContent = "Connect and Flash";
  }
  else if(stage === "wait-reset"){
    ui.startButton.disabled = true;
    ui.startButton.textContent = "Press RST Once";
    ui.startButton.classList.add("reset-cue");
  }
  else if(stage === "flashing"){
    ui.startButton.disabled = true;
    ui.startButton.textContent = "Press RST Once";
    ui.startButton.classList.add("reset-cue", "reset-done");
  }
  else if(stage === "done"){
    ui.startButton.disabled = true;
    ui.startButton.textContent = "Installed";
  }
}

async function sha256Hex(bytes){
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(digest)]
    .map((b) => b.toString(16).padStart(2, "0"))
    .join("");
}

function firmwareUrl(name){
  return new URL(`firmware/${encodeURIComponent(name)}`, window.location.href).href;
}

async function fetchAsset(asset){
  log(`Downloading ${asset.name} (${bytesText(asset.size)})…`);
  const response = await fetch(firmwareUrl(asset.name), { cache: "no-store" });
  if(!response.ok){
    throw new Error(`Download failed for ${asset.name}: HTTP ${response.status}`);
  }

  const data = new Uint8Array(await response.arrayBuffer());

  if(data.byteLength !== asset.size){
    throw new Error(`${asset.name} size mismatch.`);
  }

  if(!asset.sha256){
    throw new Error(`${asset.name} has no SHA-256 value in firmware/manifest.json.`);
  }

  const actual = await sha256Hex(data);
  if(actual !== String(asset.sha256).toLowerCase()){
    throw new Error(`SHA-256 verification failed for ${asset.name}.`);
  }

  log(`${asset.name}: SHA-256 verified.`);
  return data;
}

function validateLayout(fullAsset, recoveryAsset, helperAsset){
  const fullEnd = cfg.firmware.fullAddress + fullAsset.size;
  const recoveryEnd = cfg.firmware.recoveryAddress + recoveryAsset.size;

  if(fullEnd > cfg.firmware.recoveryAddress){
    throw new Error(`Main image would overlap the recovery region.`);
  }

  if(recoveryEnd > cfg.firmware.flashSizeBytes){
    throw new Error(`Recovery image would extend beyond 16 MB flash.`);
  }

  if(helperAsset.size < 1024 || helperAsset.size > (3 * 1024 * 1024)){
    throw new Error(`DFU helper size is not plausible.`);
  }
}

async function prepareRelease(){
  try{
    setStatus("busy");
    log("Loading firmware manifest…");

    const url = new URL("firmware/manifest.json", window.location.href);
    url.searchParams.set("_", Date.now().toString());

    const response = await fetch(url, { cache: "no-store" });

    if(response.status === 404){
      throw new Error("Firmware mirror is not initialized. Run the Sync Installer Firmware workflow once.");
    }

    if(!response.ok){
      throw new Error(`Firmware manifest lookup failed: HTTP ${response.status}`);
    }

    const manifest = await response.json();

    if(!manifest.tag || !manifest.full || !manifest.recovery || !manifest.helper){
      throw new Error("firmware/manifest.json is missing full, recovery, or helper data. Run the updated Sync Installer Firmware workflow.");
    }

    validateLayout(manifest.full, manifest.recovery, manifest.helper);

    ui.releaseVersion.textContent = manifest.tag || manifest.name || "—";
    ui.releaseDate.textContent = formatDate(manifest.published_at);
    ui.releaseBadge.textContent = manifest.prerelease ? "Pre-release" : "Latest stable";

    if(manifest.html_url){
      ui.releaseLink.href = manifest.html_url;
      ui.releaseLink.classList.remove("hidden");
    }

    ui.fullName.textContent = manifest.full.name;
    ui.recoveryName.textContent = manifest.recovery.name;
    ui.helperName.textContent = manifest.helper.name;

    const [fullData, recoveryData, helperData] = await Promise.all([
      fetchAsset(manifest.full),
      fetchAsset(manifest.recovery),
      fetchAsset(manifest.helper)
    ]);

    images = { fullData, recoveryData, helperData };
    stage = "connect";
    setStatus("good");
    updateButtonForStage();
    log("Firmware is ready.");
  }
  catch(error){
    showError(error?.message || String(error));
  }
}

function findDfuInterface(device){
  for(const configuration of device.configurations || []){
    for(const iface of configuration.interfaces || []){
      for(const alt of iface.alternates || []){
        if(alt.interfaceClass === DFU_CLASS && alt.interfaceSubclass === DFU_SUBCLASS){
          return {
            configurationValue: configuration.configurationValue,
            interfaceNumber: iface.interfaceNumber
          };
        }
      }
    }
  }

  return null;
}

async function readConfigDescriptor(device){
  const head = await device.controlTransferIn({
    requestType: "standard",
    recipient: "device",
    request: 6,
    value: (2 << 8),
    index: 0
  }, 9);

  if(head.status !== "ok" || !head.data || head.data.byteLength < 9){
    throw new Error("Could not read USB configuration descriptor.");
  }

  const hv = new DataView(head.data.buffer, head.data.byteOffset, head.data.byteLength);
  const total = hv.getUint16(2, true);

  const full = await device.controlTransferIn({
    requestType: "standard",
    recipient: "device",
    request: 6,
    value: (2 << 8),
    index: 0
  }, total);

  if(full.status !== "ok" || !full.data){
    throw new Error("Could not read full USB configuration descriptor.");
  }

  return new Uint8Array(full.data.buffer, full.data.byteOffset, full.data.byteLength);
}

function parseDfuFunctionalDescriptor(bytes){
  for(let i = 0; i + 1 < bytes.length; ){
    const len = bytes[i];
    const type = bytes[i + 1];

    if(!len) break;

    if(type === 0x21 && len >= 9){
      return {
        attributes: bytes[i + 2],
        detachTimeout: bytes[i + 3] | (bytes[i + 4] << 8),
        transferSize: bytes[i + 5] | (bytes[i + 6] << 8),
        version: bytes[i + 7] | (bytes[i + 8] << 8)
      };
    }

    i += len;
  }

  return null;
}

async function dfuStatus(device, iface){
  const response = await device.controlTransferIn({
    requestType: "class",
    recipient: "interface",
    request: DFU_GETSTATUS,
    value: 0,
    index: iface
  }, 6);

  if(response.status !== "ok" || !response.data || response.data.byteLength < 6){
    throw new Error(`DFU GETSTATUS failed (${response.status}).`);
  }

  const b = new Uint8Array(response.data.buffer, response.data.byteOffset, 6);
  return {
    status: b[0],
    pollTimeout: b[1] | (b[2] << 8) | (b[3] << 16),
    state: b[4]
  };
}

async function makeDfuReady(device, iface){
  let status = await dfuStatus(device, iface);

  if(status.state === STATE_DFU_ERROR){
    const clear = await device.controlTransferOut({
      requestType: "class",
      recipient: "interface",
      request: DFU_CLRSTATUS,
      value: 0,
      index: iface
    });

    if(clear.status !== "ok"){
      throw new Error(`DFU CLRSTATUS failed (${clear.status}).`);
    }

    status = await dfuStatus(device, iface);
  }

  if(status.state !== STATE_DFU_IDLE && status.state !== STATE_DFU_DNLOAD_IDLE){
    try{
      await device.controlTransferOut({
        requestType: "class",
        recipient: "interface",
        request: DFU_ABORT,
        value: 0,
        index: iface
      });
      status = await dfuStatus(device, iface);
    }
    catch(_error){}
  }

  return status;
}

async function waitForDownloadIdle(device, iface){
  for(let n = 0; n < 100; n++){
    const status = await dfuStatus(device, iface);

    if(status.status !== 0){
      throw new Error(`DFU status error ${status.status}, state ${status.state}.`);
    }

    if(status.state === STATE_DFU_DNLOAD_IDLE || status.state === STATE_DFU_IDLE){
      return;
    }

    if(status.state === STATE_DFU_ERROR){
      throw new Error("DFU entered error state.");
    }

    if(status.state !== STATE_DFU_DNLOAD_SYNC && status.state !== STATE_DFU_DNBUSY){
      throw new Error(`Unexpected DFU state ${status.state} while downloading.`);
    }

    await sleep(Math.max(1, status.pollTimeout));
  }

  throw new Error("Timed out waiting for DFU download block.");
}

async function manifestDfu(device, iface, blockNumber){
  log("Finishing helper upload…");

  const result = await device.controlTransferOut({
    requestType: "class",
    recipient: "interface",
    request: DFU_DNLOAD,
    value: blockNumber,
    index: iface
  });

  if(result.status !== "ok"){
    throw new Error(`Final DFU DNLOAD failed (${result.status}).`);
  }

  for(let n = 0; n < 80; n++){
    const status = await dfuStatus(device, iface);

    if(status.status !== 0){
      throw new Error(`DFU manifestation error ${status.status}.`);
    }

    if(status.state === STATE_DFU_MANIFEST_WAIT_RESET ||
       status.state === STATE_DFU_IDLE ||
       status.state === STATE_DFU_DNLOAD_IDLE){
      return;
    }

    if(status.state !== STATE_DFU_MANIFEST_SYNC &&
       status.state !== STATE_DFU_MANIFEST){
      return;
    }

    await sleep(Math.max(10, status.pollTimeout));
  }
}

async function uploadHelperDfu(){
  let device = null;
  let iface = null;
  let claimed = false;

  try{
    log("Select “Nano ESP32 (bootloader)” in the USB chooser.");

    device = await navigator.usb.requestDevice({
      filters: [{
        vendorId: cfg.device.recoveryVendorId,
        productId: cfg.device.recoveryProductId
      }]
    });

    log(`Selected ${device.productName || "Nano ESP32 (bootloader)"} ${hex4(device.vendorId)}:${hex4(device.productId)}.`);

    iface = findDfuInterface(device);
    if(!iface){
      throw new Error("The selected device does not expose the Nano ESP32 DFU recovery interface.");
    }

    await device.open();

    if(!device.configuration){
      await device.selectConfiguration(iface.configurationValue);
    }

    await device.claimInterface(iface.interfaceNumber);
    claimed = true;

    log(`DFU interface ${iface.interfaceNumber} claimed.`);

    let transferSize = 4096;

    try{
      const descriptor = parseDfuFunctionalDescriptor(await readConfigDescriptor(device));
      if(descriptor?.transferSize > 0){
        transferSize = Math.min(descriptor.transferSize, 4096);
      }
      if(descriptor){
        log(`DFU ready: ${transferSize}-byte blocks.`);
      }
    }
    catch(_error){
      log("Using 4096-byte DFU blocks.");
    }

    if(transferSize < 64) transferSize = 64;

    const initial = await makeDfuReady(device, iface.interfaceNumber);
    if(initial.status !== 0){
      throw new Error(`DFU is not ready (status ${initial.status}).`);
    }

    let block = 0;
    let lastPercent = -1;

    for(let offset = 0; offset < images.helperData.length; offset += transferSize, block++){
      const chunk = images.helperData.subarray(
        offset,
        Math.min(offset + transferSize, images.helperData.length)
      );

      const out = await device.controlTransferOut({
        requestType: "class",
        recipient: "interface",
        request: DFU_DNLOAD,
        value: block,
        index: iface.interfaceNumber
      }, chunk);

      if(out.status !== "ok"){
        throw new Error(`DFU helper block ${block} failed (${out.status}).`);
      }

      await waitForDownloadIdle(device, iface.interfaceNumber);

      const percent = Math.min(
        100,
        Math.floor(((offset + chunk.length) * 100) / images.helperData.length)
      );

      if(percent >= lastPercent + 10 || percent === 100){
        lastPercent = percent;
        log(`Preparing USB JTAG helper: ${percent}%`);
      }
    }

    await manifestDfu(device, iface.interfaceNumber, block);
    log("USB JTAG helper installed successfully.");
  }
  finally{
    try{
      if(device?.opened && claimed && iface){
        await device.releaseInterface(iface.interfaceNumber);
      }
    }
    catch(_error){}

    try{
      if(device?.opened){
        await device.close();
      }
    }
    catch(_error){}
  }
}

function isBootloaderPort(port){
  try{
    const info = port.getInfo();
    return info.usbVendorId === cfg.device.bootVendorId &&
           info.usbProductId === cfg.device.bootProductId;
  }
  catch(_error){
    return false;
  }
}

async function getAuthorizedBootloaderPorts(){
  const ports = await navigator.serial.getPorts();
  return ports.filter(isBootloaderPort);
}

function stopBootloaderWatcher(){
  if(watchTimer){
    clearInterval(watchTimer);
    watchTimer = null;
  }
}

function startBootloaderWatcher(){
  stopBootloaderWatcher();

  watchTimer = setInterval(async () => {
    if(busy || stage !== "wait-reset"){
      return;
    }

    try{
      const ports = await getAuthorizedBootloaderPorts();

      if(ports.length === 1){
        stopBootloaderWatcher();
        stage = "flashing";
        updateButtonForStage();
        log("RST detected. USB JTAG is ready; flashing automatically.");
        await flashBootloaderDevice(ports[0], true);
      }
    }
    catch(_error){}
  }, 500);
}

async function beginFactoryFlow(){
  if(busy || !images) return;

  clearError();
  ui.successBox.classList.add("hidden");

  // If this board is already in ROM USB JTAG mode and Chrome already has
  // permission, skip the recovery helper entirely.
  try{
    const authorized = await getAuthorizedBootloaderPorts();

    if(authorized.length === 1){
      log("Authorized USB JTAG device already present. Skipping recovery helper.");
      await flashBootloaderDevice(authorized[0], true);
      return;
    }
  }
  catch(_error){}

  setBusy(true);

  try{
    log("Connecting to Nano ESP32 recovery mode…");
    await uploadHelperDfu();

    // Give the Nano recovery/DFU stack a full two seconds to settle before
    // telling the user to press RST. Pressing RST earlier can interrupt the
    // transition immediately after the helper upload.
    log("Preparing reset step…");
    await sleep(2000);

    stage = "wait-reset";
    setBusy(false);
    updateButtonForStage();

    log("");
    log("========================================");
    log("PRESS THE NANO RST BUTTON ONCE TO CONTINUE");
    log("========================================");
    log("Waiting for USB JTAG…");

    // Keep the same button in the Press RST Once state. The page watches for
    // the already-authorized 303A:1001 device and continues automatically.
    startBootloaderWatcher();
  }
  catch(error){
    setBusy(false);

    if(error?.name === "NotFoundError"){
      stage = "connect";
      updateButtonForStage();
      showError("Nano recovery mode was not selected. Double-click RST until the GREEN LED strobes, then click “Connect and Flash” again.");
    }
    else{
      stage = "connect";
      updateButtonForStage();
      showError(error?.message || String(error));
    }
  }
}

function resetProgress(){
  ui.fullProgress.value = 0;
  ui.recoveryProgress.value = 0;
  ui.fullPercent.textContent = "0%";
  ui.recoveryPercent.textContent = "0%";
  lastProgressLog = [-10, -10];
}

async function flashBootloaderDevice(portOverride = null, automatic = false){
  if(busy || !images) return;

  stopBootloaderWatcher();
  clearError();
  ui.successBox.classList.add("hidden");
  resetProgress();
  setBusy(true);

  let transport = null;
  let loaderConnected = false;

  try{
    let port = portOverride;

    if(!port){
      log("Select “Espressif USB JTAG/serial debug unit” in Chrome.");
      port = await navigator.serial.requestPort({
        filters: [{
          usbVendorId: cfg.device.bootVendorId,
          usbProductId: cfg.device.bootProductId
        }]
      });
    }
    else{
      log("Using previously authorized USB JTAG device.");
    }

    const info = port.getInfo();
    log(`Selected USB ${hex4(info.usbVendorId)}:${hex4(info.usbProductId)}.`);

    transport = new Transport(port, false);

    const terminal = {
      clean(){},
      writeLine(data){
        const text = String(data || "").trimEnd();
        if(text) log(text);
      },
      write(data){
        const text = String(data || "").trimEnd();
        if(text) log(text);
      }
    };

    const loader = new ESPLoader({
      transport,
      baudrate: cfg.device.flashBaud,
      terminal,
      debugLogging: false
    });

    log("Connecting to ESP32-S3 ROM downloader…");
    const chipName = await loader.main();
    loaderConnected = true;
    log(`Detected chip: ${chipName}`);

    if(!String(chipName).toUpperCase().includes(cfg.device.expectedChip.toUpperCase())){
      throw new Error(`Wrong chip detected (${chipName}). This installer requires ${cfg.device.expectedChip}.`);
    }

    log("Writing DonutShop firmware…");

    await loader.writeFlash({
      fileArray: [
        { data: images.fullData, address: cfg.firmware.fullAddress },
        { data: images.recoveryData, address: cfg.firmware.recoveryAddress }
      ],
      flashMode: "keep",
      flashFreq: "keep",
      flashSize: "keep",
      eraseAll: false,
      compress: true,
      reportProgress(fileIndex, written, total){
        const percent = total
          ? Math.min(100, Math.round((written / total) * 100))
          : 0;

        if(fileIndex === 0){
          ui.fullProgress.value = percent;
          ui.fullPercent.textContent = `${percent}%`;
        }
        else{
          ui.recoveryProgress.value = percent;
          ui.recoveryPercent.textContent = `${percent}%`;
        }

        const bucket = percent === 100 ? 100 : Math.floor(percent / 10) * 10;

        if(bucket >= lastProgressLog[fileIndex] + 10 || percent === 100){
          lastProgressLog[fileIndex] = bucket;
          log(`${fileIndex === 0 ? "Main firmware" : "Recovery image"}: ${percent}%`);
        }
      }
    });

    log("Flash complete. Resetting Nano…");
    await loader.after("hard_reset");

    stage = "done";
    setStatus("good");
    setBusy(false);
    updateButtonForStage();
    ui.successBox.classList.remove("hidden");
    log("Installation complete.");
  }
  catch(error){
    if(transport){
      try{
        await transport.disconnect();
      }
      catch(_error){}
    }

    setBusy(false);

    if(automatic && !loaderConnected){
      stage = "wait-reset";
      updateButtonForStage();
      setStatus("good");
      log(`Automatic USB JTAG connection was not ready yet: ${error?.message || error}`);
      log("Waiting for USB JTAG to become available…");
      startBootloaderWatcher();
      return;
    }

    stage = "wait-reset";
    updateButtonForStage();
    showError(`Installation failed: ${error?.message || error}`);
  }
}

async function handlePrimaryAction(){
  if(stage === "connect"){
    await beginFactoryFlow();
  }
}

function init(){
  ui.startButton.addEventListener("click", handlePrimaryAction);

  if(!("usb" in navigator) || !("serial" in navigator)){
    ui.browserWarning.classList.remove("hidden");
    setStatus("bad");
    return;
  }

  prepareRelease();
}

init();
