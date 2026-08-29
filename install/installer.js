import { ESPLoader, Transport } from "https://unpkg.com/esptool-js@0.6.1/bundle.js";

const cfg = window.DONUTSHOP_INSTALLER_CONFIG;

const $ = (id) => document.getElementById(id);
const ui = {
  browserWarning: $("browserWarning"),
  configWarning: $("configWarning"),
  releaseBadge: $("releaseBadge"),
  releaseVersion: $("releaseVersion"),
  releaseDate: $("releaseDate"),
  releaseNotes: $("releaseNotes"),
  releaseLink: $("releaseLink"),
  fullName: $("fullName"),
  recoveryName: $("recoveryName"),
  fullSize: $("fullSize"),
  recoverySize: $("recoverySize"),
  startButton: $("startButton"),
  bootloaderButton: $("bootloaderButton"),
  statusDot: $("statusDot"),
  stepRelease: $("stepRelease"),
  stepReleaseText: $("stepReleaseText"),
  stepTrigger: $("stepTrigger"),
  stepTriggerText: $("stepTriggerText"),
  stepConnect: $("stepConnect"),
  stepConnectText: $("stepConnectText"),
  stepFlash: $("stepFlash"),
  stepFlashText: $("stepFlashText"),
  fullProgressLabel: $("fullProgressLabel"),
  fullProgress: $("fullProgress"),
  fullPercent: $("fullPercent"),
  recoveryProgress: $("recoveryProgress"),
  recoveryPercent: $("recoveryPercent"),
  successBox: $("successBox"),
  errorBox: $("errorBox"),
  consoleOutput: $("consoleOutput")
};

let releaseInfo = null;
let images = null;
let busy = false;
let primaryStage = "trigger";
let lastProgressLog = [-10, -10];

function log(message){
  const line = `[${new Date().toLocaleTimeString()}] ${message}`;
  console.log(line);
  ui.consoleOutput.textContent += `${line}\n`;
  ui.consoleOutput.scrollTop = ui.consoleOutput.scrollHeight;
}

function setStatus(kind){
  ui.statusDot.className = "status-dot" + (kind ? ` ${kind}` : "");
}

function setStep(element, state){
  element.classList.remove("active", "done", "error");
  if(state) element.classList.add(state);
}

function showError(message, step = null){
  if(step) setStep(step, "error");
  ui.errorBox.textContent = message;
  ui.errorBox.classList.remove("hidden");
  ui.successBox.classList.add("hidden");
  setStatus("bad");
  log(`ERROR: ${message}`);
}

function clearError(){
  ui.errorBox.classList.add("hidden");
  ui.errorBox.textContent = "";
}

function setBusy(value){
  busy = value;
  ui.startButton.disabled = value || !images || primaryStage === "done";
  ui.bootloaderButton.disabled = value || !images;
  if(value) setStatus("busy");
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
  return new Intl.DateTimeFormat(undefined, { year:"numeric", month:"long", day:"numeric" }).format(date);
}

function normalizeNotes(text){
  if(!text) return "Latest stable DonutShop release.";
  const stripped = text
    .replace(/```[\s\S]*?```/g, "")
    .replace(/!\[[^\]]*\]\([^)]*\)/g, "")
    .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1")
    .replace(/^#{1,6}\s+/gm, "")
    .replace(/^[-*+]\s+/gm, "• ")
    .trim();
  const max = 420;
  return stripped.length > max ? `${stripped.slice(0, max).trimEnd()}…` : stripped;
}

function validateLayout(fullAsset, recoveryAsset){
  const fullEnd = cfg.firmware.fullAddress + fullAsset.size;
  const recoveryEnd = cfg.firmware.recoveryAddress + recoveryAsset.size;

  if(fullEnd > cfg.firmware.recoveryAddress){
    throw new Error(`Main image would overlap the recovery region: ${fullAsset.name} ends at 0x${fullEnd.toString(16).toUpperCase()}.`);
  }
  if(recoveryEnd > cfg.firmware.flashSizeBytes){
    throw new Error(`Recovery image would extend past the configured 16 MB flash boundary (ends at 0x${recoveryEnd.toString(16).toUpperCase()}).`);
  }
}

async function sha256Hex(bytes){
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

function firmwareUrl(name){
  // Firmware is mirrored into install/firmware by the GitHub Action.  Fetching
  // from the same GitHub Pages origin avoids GitHub Release CDN CORS blocking.
  return new URL(`firmware/${encodeURIComponent(name)}`, window.location.href).href;
}

async function fetchMirroredAsset(asset){
  log(`Downloading ${asset.name} (${bytesText(asset.size)})…`);
  const response = await fetch(firmwareUrl(asset.name), { cache:"no-store" });
  if(!response.ok) throw new Error(`Download failed for ${asset.name}: HTTP ${response.status}`);

  const data = new Uint8Array(await response.arrayBuffer());
  if(data.byteLength !== asset.size){
    throw new Error(`${asset.name} size mismatch. Manifest reports ${asset.size} bytes; downloaded ${data.byteLength} bytes.`);
  }

  if(asset.sha256){
    const actual = await sha256Hex(data);
    if(actual !== String(asset.sha256).toLowerCase()){
      throw new Error(`SHA-256 verification failed for ${asset.name}.`);
    }
    log(`${asset.name}: SHA-256 verified.`);
  }
  else{
    throw new Error(`${asset.name} has no SHA-256 value in firmware/manifest.json.`);
  }

  return data;
}

async function prepareRelease(){
  try{
    setStatus("busy");
    log("Loading mirrored firmware manifest…");
    const manifestUrl = new URL("firmware/manifest.json", window.location.href);
    manifestUrl.searchParams.set("_", Date.now().toString());
    const response = await fetch(manifestUrl, { cache:"no-store" });

    if(response.status === 404){
      throw new Error("Firmware mirror is not initialized yet. Run the “Sync Installer Firmware” GitHub Action once, then reload this page.");
    }
    if(!response.ok){
      throw new Error(`Firmware manifest lookup failed: HTTP ${response.status}`);
    }

    const manifest = await response.json();
    if(!manifest.tag || !manifest.full || !manifest.recovery){
      throw new Error("firmware/manifest.json is incomplete.");
    }

    const fullAsset = manifest.full;
    const recoveryAsset = manifest.recovery;
    validateLayout(fullAsset, recoveryAsset);

    releaseInfo = { release: manifest, fullAsset, recoveryAsset };
    ui.releaseBadge.textContent = manifest.prerelease ? "Pre-release" : "Latest stable";
    ui.releaseVersion.textContent = manifest.tag || manifest.name || "—";
    ui.releaseDate.textContent = formatDate(manifest.published_at);
    ui.releaseNotes.textContent = normalizeNotes(manifest.body);
    if(manifest.html_url){
      ui.releaseLink.href = manifest.html_url;
      ui.releaseLink.classList.remove("hidden");
    }
    ui.fullName.textContent = fullAsset.name;
    ui.recoveryName.textContent = recoveryAsset.name;
    ui.fullSize.textContent = bytesText(fullAsset.size);
    ui.recoverySize.textContent = bytesText(recoveryAsset.size);
    ui.fullProgressLabel.textContent = fullAsset.name;

    ui.stepReleaseText.textContent = "Downloading and verifying mirrored Release assets…";
    const [fullData, recoveryData] = await Promise.all([
      fetchMirroredAsset(fullAsset),
      fetchMirroredAsset(recoveryAsset)
    ]);

    images = { fullData, recoveryData };
    setStep(ui.stepRelease, "done");
    ui.stepReleaseText.textContent = `${manifest.tag}: both firmware files downloaded and verified.`;
    setStep(ui.stepTrigger, "active");
    setStatus("good");
    ui.startButton.disabled = false;
    ui.bootloaderButton.disabled = false;
    log("Release is ready to install.");
  }
  catch(error){
    showError(error?.message || String(error), ui.stepRelease);
    ui.releaseBadge.textContent = "Unavailable";
    ui.releaseNotes.textContent = "The firmware mirror could not be prepared.";
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

async function waitForAuthorizedBootloader(timeoutMs){
  const deadline = Date.now() + timeoutMs;

  while(Date.now() < deadline){
    const matches = await getAuthorizedBootloaderPorts();

    if(matches.length === 1){
      return { port: matches[0], multiple: false };
    }

    if(matches.length > 1){
      return { port: null, multiple: true };
    }

    await sleep(250);
  }

  return { port: null, multiple: false };
}

async function tryAutomaticBootloaderFlash(){
  if(busy || !images || primaryStage !== "flash") return;

  const waitMs = cfg.device.autoBootloaderWaitMs || 6000;
  setBusy(true);
  ui.stepConnectText.textContent = "Waiting for the ESP32-S3 USB bootloader…";
  log("Waiting for a previously authorized ESPressif USB JTAG/serial debug unit…");

  let result;
  try{
    result = await waitForAuthorizedBootloader(waitMs);
  }
  catch(error){
    log(`Could not check previously authorized serial ports: ${error?.message || error}`);
    result = { port: null, multiple: false };
  }
  finally{
    setBusy(false);
  }

  if(result.multiple){
    setStatus("good");
    ui.stepConnectText.textContent = "Multiple authorized ESPressif bootloaders were found. Select the correct one.";
    log("Multiple authorized bootloaders detected. Click Select USB JTAG and Flash and select the correct device.");
    return;
  }

  if(!result.port){
    setStatus("good");
    ui.stepConnectText.textContent = "Click Select USB JTAG and Flash and select the Espressif USB JTAG/serial debug unit.";
    log("Bootloader permission is not available yet. Click Select USB JTAG and Flash and select the Espressif USB JTAG/serial debug unit.");
    return;
  }

  log("Previously authorized bootloader detected. Flashing automatically.");
  await flashBootloaderDevice(result.port, true);
}

async function triggerFactoryBootloader(){
  if(busy || !images) return;
  clearError();
  setBusy(true);
  setStep(ui.stepTrigger, "active");
  ui.stepTriggerText.textContent = "Select the factory Arduino Nano ESP32 in Chrome's serial chooser.";

  let port = null;
  let tryAutoFlash = false;
  try{
    log("Requesting factory Arduino Nano ESP32…");
    port = await navigator.serial.requestPort({
      filters: [{
        usbVendorId: cfg.device.nanoVendorId,
        usbProductId: cfg.device.nanoProductId
      }]
    });

    const info = port.getInfo();
    log(`Selected USB ${hex4(info.usbVendorId)}:${hex4(info.usbProductId)}.`);
    ui.stepTriggerText.textContent = "Sending the 1200-baud bootloader trigger…";

    await port.open({
      baudRate: cfg.device.triggerBaud,
      dataBits: 8,
      stopBits: 1,
      parity: "none",
      bufferSize: 255,
      flowControl: "none"
    });

    // Opening the Nano's TinyUSB CDC interface at 1200 baud is the trigger.
    // The device may disappear before this delay/close completes; that is expected.
    await sleep(300);

    try{
      if(port.readable || port.writable) await port.close();
    }
    catch(_error){
      // Expected when USB re-enumerates during the bootloader transition.
    }

    setStep(ui.stepTrigger, "done");
    ui.stepTriggerText.textContent = "Bootloader requested. The Nano should now appear as an Espressif USB JTAG/serial debug unit.";
    setStep(ui.stepConnect, "active");
    ui.stepConnectText.textContent = "Looking for an already-authorized ESP32-S3 bootloader…";
    primaryStage = "flash";
    ui.startButton.textContent = "Select USB JTAG and Flash";
    setStatus("good");
    log("Bootloader requested. Checking whether Chrome already has permission to the Espressif USB device…");
    tryAutoFlash = true;
  }
  catch(error){
    const message = error?.name === "NotFoundError"
      ? "No Nano ESP32 was selected. Click Connect and Flash to try again."
      : `Could not trigger installation mode: ${error?.message || error}`;
    showError(message, ui.stepTrigger);
  }
  finally{
    setBusy(false);
  }

  if(tryAutoFlash){
    await tryAutomaticBootloaderFlash();
  }
}

async function flashBootloaderDevice(portOverride = null, automatic = false){
  if(busy || !images) return;
  clearError();
  ui.successBox.classList.add("hidden");
  resetProgress();
  lastProgressLog = [-10, -10];
  setBusy(true);
  setStep(ui.stepConnect, "active");
  ui.stepConnectText.textContent = "Select “Espressif USB JTAG/serial debug unit” in Chrome's serial chooser.";

  let transport = null;
  let loaderConnected = false;
  try{
    let port = portOverride;

    if(port){
      log("Using previously authorized ESP32-S3 USB Serial/JTAG bootloader.");
    }
    else{
      log("Requesting ESP32-S3 USB Serial/JTAG bootloader…");
      port = await navigator.serial.requestPort({
        filters: [{
          usbVendorId: cfg.device.bootVendorId,
          usbProductId: cfg.device.bootProductId
        }]
      });
    }

    const info = port.getInfo();
    log(`Selected USB ${hex4(info.usbVendorId)}:${hex4(info.usbProductId)}.`);

    transport = new Transport(port, false);
    transport.setDeviceLostCallback(() => {
      log("USB device disconnected.");
    });

    const terminal = {
      clean(){},
      writeLine(data){ if(data) log(String(data).trimEnd()); },
      write(data){ if(data) log(String(data).trimEnd()); }
    };

    const esploader = new ESPLoader({
      transport,
      baudrate: cfg.device.flashBaud,
      terminal,
      debugLogging: false
    });

    ui.stepConnectText.textContent = "Connecting to the ESP32-S3 ROM downloader…";
    const chipName = await esploader.main();
    loaderConnected = true;
    log(`Detected chip: ${chipName}`);

    if(!String(chipName).toUpperCase().includes(cfg.device.expectedChip.toUpperCase())){
      throw new Error(`Wrong chip detected (${chipName}). This installer only supports ${cfg.device.expectedChip}.`);
    }

    setStep(ui.stepConnect, "done");
    ui.stepConnectText.textContent = `${chipName} detected.`;
    setStep(ui.stepFlash, "active");
    ui.stepFlashText.textContent = "Writing main firmware and recovery image…";

    await esploader.writeFlash({
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
        const percent = total ? Math.min(100, Math.round((written / total) * 100)) : 0;
        if(fileIndex === 0){
          ui.fullProgress.value = percent;
          ui.fullPercent.textContent = `${percent}%`;
        }
        else if(fileIndex === 1){
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

    ui.fullProgress.value = 100;
    ui.fullPercent.textContent = "100%";
    ui.recoveryProgress.value = 100;
    ui.recoveryPercent.textContent = "100%";
    ui.stepFlashText.textContent = "Flash writes completed and verified by esptool-js. Resetting the Nano…";

    await esploader.after("hard_reset");

    setStep(ui.stepFlash, "done");
    ui.stepFlashText.textContent = "Installation complete.";
    setStatus("good");
    primaryStage = "done";
    ui.startButton.textContent = "Installed";
    ui.successBox.classList.remove("hidden");
    log("Installation complete. Hard reset requested.");

    try{
      await transport.disconnect();
    }
    catch(_error){
      // The reset may already have disconnected/re-enumerated the USB device.
    }
  }
  catch(error){
    if(transport){
      try{ await transport.disconnect(); }catch(_error){}
    }

    if(automatic && !loaderConnected){
      clearError();
      setStatus("good");
      setStep(ui.stepConnect, "active");
      ui.stepConnectText.textContent = "Automatic connection was not ready. Click Select USB JTAG and Flash and select the Espressif USB device.";
      primaryStage = "flash";
      ui.startButton.textContent = "Select USB JTAG and Flash";
      log(`Automatic bootloader connection was not ready: ${error?.message || error}`);
      log("Click Select USB JTAG and Flash and select the Espressif USB JTAG/serial debug unit.");
      return;
    }

    const message = error?.name === "NotFoundError"
      ? "No bootloader device was selected. Click Select USB JTAG and Flash to try again."
      : `Installation failed: ${error?.message || error}`;
    showError(message, ui.stepFlash.classList.contains("active") ? ui.stepFlash : ui.stepConnect);
  }
  finally{
    setBusy(false);
  }
}

function resetProgress(){
  ui.fullProgress.value = 0;
  ui.recoveryProgress.value = 0;
  ui.fullPercent.textContent = "0%";
  ui.recoveryPercent.textContent = "0%";
}

function hex4(value){
  if(value === undefined) return "????";
  return value.toString(16).toUpperCase().padStart(4, "0");
}

function sleep(ms){
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function handlePrimaryAction(){
  if(primaryStage === "trigger") await triggerFactoryBootloader();
  else if(primaryStage === "flash") await flashBootloaderDevice();
}

function init(){
  ui.recoveryName.textContent = cfg?.firmware?.recoveryName || "nora_recovery.bin";
  ui.startButton.addEventListener("click", handlePrimaryAction);

  if(!("serial" in navigator)){
    ui.browserWarning.classList.remove("hidden");
    ui.releaseBadge.textContent = "Unsupported browser";
    setStatus("bad");
    return;
  }

  prepareRelease();
}

init();
