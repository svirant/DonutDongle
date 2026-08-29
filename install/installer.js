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
  ui.startButton.disabled = value || !images;
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

function isConfigured(){
  return cfg &&
    cfg.github &&
    cfg.github.owner && cfg.github.owner !== "CHANGE_ME" &&
    cfg.github.repo && cfg.github.repo !== "CHANGE_ME";
}

function releaseApiUrl(){
  const owner = encodeURIComponent(cfg.github.owner);
  const repo = encodeURIComponent(cfg.github.repo);
  if(!cfg.github.release || cfg.github.release === "latest"){
    return `https://api.github.com/repos/${owner}/${repo}/releases/latest`;
  }
  return `https://api.github.com/repos/${owner}/${repo}/releases/tags/${encodeURIComponent(cfg.github.release)}`;
}

function findAssets(release){
  const assets = release.assets || [];
  let fullAsset;

  if(cfg.firmware.fullBinName){
    fullAsset = assets.find((asset) => asset.name === cfg.firmware.fullBinName);
  }
  else{
    const matches = assets.filter((asset) => asset.name.endsWith(cfg.firmware.fullBinSuffix));
    if(matches.length > 1){
      throw new Error(`Release contains multiple ${cfg.firmware.fullBinSuffix} files. Set firmware.fullBinName in config.js to the exact asset name.`);
    }
    fullAsset = matches[0];
  }

  const recoveryAsset = assets.find((asset) => asset.name === cfg.firmware.recoveryName);
  if(!fullAsset) throw new Error(`Could not find the main firmware asset (${cfg.firmware.fullBinName || `*${cfg.firmware.fullBinSuffix}`}).`);
  if(!recoveryAsset) throw new Error(`Could not find ${cfg.firmware.recoveryName} in this Release.`);
  return { fullAsset, recoveryAsset };
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

async function fetchAsset(asset){
  log(`Downloading ${asset.name} (${bytesText(asset.size)})…`);
  const response = await fetch(asset.browser_download_url, { cache:"no-store" });
  if(!response.ok) throw new Error(`Download failed for ${asset.name}: HTTP ${response.status}`);

  const data = new Uint8Array(await response.arrayBuffer());
  if(data.byteLength !== asset.size){
    throw new Error(`${asset.name} size mismatch. GitHub reports ${asset.size} bytes; downloaded ${data.byteLength} bytes.`);
  }

  if(asset.digest && asset.digest.toLowerCase().startsWith("sha256:")){
    const expected = asset.digest.slice(7).toLowerCase();
    const actual = await sha256Hex(data);
    if(actual !== expected){
      throw new Error(`SHA-256 verification failed for ${asset.name}.`);
    }
    log(`${asset.name}: SHA-256 verified.`);
  }
  else{
    log(`${asset.name}: GitHub did not provide a SHA-256 digest; size verified only.`);
  }

  return data;
}

async function prepareRelease(){
  if(!isConfigured()){
    ui.configWarning.classList.remove("hidden");
    ui.releaseBadge.textContent = "Configure first";
    ui.releaseNotes.textContent = "Set github.owner and github.repo in config.js, then reload this page.";
    setStep(ui.stepRelease, "error");
    ui.stepReleaseText.textContent = "config.js needs your GitHub repository details.";
    return;
  }

  try{
    setStatus("busy");
    log(`Loading GitHub Release from ${cfg.github.owner}/${cfg.github.repo}…`);
    const response = await fetch(releaseApiUrl(), {
      headers: { "Accept":"application/vnd.github+json" },
      cache: "no-store"
    });
    if(!response.ok){
      if(response.status === 403) throw new Error("GitHub API request was rate-limited. Reload later or use a dedicated Pages repository with fewer API requests.");
      if(response.status === 404) throw new Error("Configured GitHub Release was not found. Check github.owner, github.repo, and github.release in config.js.");
      throw new Error(`GitHub Release lookup failed: HTTP ${response.status}`);
    }

    const release = await response.json();
    const { fullAsset, recoveryAsset } = findAssets(release);
    validateLayout(fullAsset, recoveryAsset);

    releaseInfo = { release, fullAsset, recoveryAsset };
    ui.releaseBadge.textContent = cfg.github.release === "latest" ? (release.prerelease ? "Pre-release" : "Latest stable") : "Pinned release";
    ui.releaseVersion.textContent = release.tag_name || release.name || "—";
    ui.releaseDate.textContent = formatDate(release.published_at || release.created_at);
    ui.releaseNotes.textContent = normalizeNotes(release.body);
    ui.releaseLink.href = release.html_url;
    ui.releaseLink.classList.remove("hidden");
    ui.fullName.textContent = fullAsset.name;
    ui.recoveryName.textContent = recoveryAsset.name;
    ui.fullSize.textContent = bytesText(fullAsset.size);
    ui.recoverySize.textContent = bytesText(recoveryAsset.size);
    ui.fullProgressLabel.textContent = fullAsset.name;

    ui.stepReleaseText.textContent = "Downloading and verifying Release assets…";
    const [fullData, recoveryData] = await Promise.all([
      fetchAsset(fullAsset),
      fetchAsset(recoveryAsset)
    ]);

    images = { fullData, recoveryData };
    setStep(ui.stepRelease, "done");
    ui.stepReleaseText.textContent = `${release.tag_name}: both firmware files downloaded and verified.`;
    setStep(ui.stepTrigger, "active");
    setStatus("good");
    ui.startButton.disabled = false;
    ui.bootloaderButton.disabled = false;
    log("Release is ready to install.");
  }
  catch(error){
    showError(error?.message || String(error), ui.stepRelease);
    ui.releaseBadge.textContent = "Unavailable";
    ui.releaseNotes.textContent = "The firmware Release could not be prepared.";
  }
}

async function triggerFactoryBootloader(){
  if(busy || !images) return;
  clearError();
  setBusy(true);
  setStep(ui.stepTrigger, "active");
  ui.stepTriggerText.textContent = "Select the factory Arduino Nano ESP32 in Chrome's serial chooser.";

  let port = null;
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
    ui.stepConnectText.textContent = "Click “Connect Bootloader & Flash”, then select the Espressif device.";
    setStatus("good");
    log("1200-baud trigger sent. Waiting for the ESP32-S3 ROM USB device.");
  }
  catch(error){
    const message = error?.name === "NotFoundError"
      ? "No Nano ESP32 was selected. Click Install DonutShop to try again."
      : `Could not trigger installation mode: ${error?.message || error}`;
    showError(message, ui.stepTrigger);
  }
  finally{
    setBusy(false);
  }
}

async function flashBootloaderDevice(){
  if(busy || !images) return;
  clearError();
  ui.successBox.classList.add("hidden");
  resetProgress();
  setBusy(true);
  setStep(ui.stepConnect, "active");
  ui.stepConnectText.textContent = "Select “Espressif USB JTAG/serial debug unit” in Chrome's serial chooser.";

  let transport = null;
  try{
    log("Requesting ESP32-S3 USB Serial/JTAG bootloader…");
    const port = await navigator.serial.requestPort({
      filters: [{
        usbVendorId: cfg.device.bootVendorId,
        usbProductId: cfg.device.bootProductId
      }]
    });

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
    const message = error?.name === "NotFoundError"
      ? "No bootloader device was selected. Click Connect Bootloader & Flash to try again."
      : `Installation failed: ${error?.message || error}`;
    showError(message, ui.stepFlash.classList.contains("active") ? ui.stepFlash : ui.stepConnect);
    if(transport){
      try{ await transport.disconnect(); }catch(_error){}
    }
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

function init(){
  ui.recoveryName.textContent = cfg?.firmware?.recoveryName || "nora_recovery.bin";
  ui.startButton.addEventListener("click", triggerFactoryBootloader);
  ui.bootloaderButton.addEventListener("click", flashBootloaderDevice);

  if(!("serial" in navigator)){
    ui.browserWarning.classList.remove("hidden");
    ui.releaseBadge.textContent = "Unsupported browser";
    setStatus("bad");
    return;
  }

  prepareRelease();
}

init();
