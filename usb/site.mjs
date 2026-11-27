import { readCompanion, readProcessor, withProcessor } from "./probe.mjs";
import { ESPLoader, Transport } from "./esptool.js";
import { eligible, verifyImage, install, release, healthyTarget } from "./update.mjs";

const connect = document.querySelector("#connect");
const hardware = document.querySelector("#hardware");
const title = document.querySelector("#title");
const message = document.querySelector("#message");
const flip = document.querySelector("#flip");
const bothWays = document.querySelector("#both-ways");
const update = document.querySelector("#update");
const fileInput = document.querySelector("#image");
const preview = document.querySelector("#preview");
const confirm = document.querySelector("#install");
const filter = { usbVendorId: 0x303a, usbProductId: 0x1001 };
let port;
let paired;
let image;

function clearUpdate() {
  update.hidden = true;
  confirm.hidden = true;
  image = null;
  fileInput.value = "";
  preview.textContent = "";
}

async function refreshPaired() {
  const ports = (await navigator.serial.getPorts()).filter(p => {
    const info = p.getInfo();
    return info.usbVendorId === filter.usbVendorId && info.usbProductId === filter.usbProductId;
  });
  paired = ports.length === 1 ? ports[0] : null;
}

function show(heading, detail, state = "reading") {
  title.textContent = heading;
  message.textContent = detail;
  title.parentElement.dataset.state = state;
  flip.hidden = state !== "choose";
  bothWays.hidden = flip.hidden;
}

if (navigator.serial && window.isSecureContext) {
  refreshPaired().catch(() => {}).finally(() => { connect.disabled = false; });
  navigator.serial.addEventListener("connect", () => refreshPaired().catch(() => {}));
  navigator.serial.addEventListener("disconnect", event => {
    refreshPaired().catch(() => {});
    if (event.target !== port) return;
    clearUpdate();
    port = null;
    hardware.hidden = true;
    show("Dial disconnected", "Reconnect the cable and choose Check my dial to verify this orientation.", "choose");
  });
} else {
  show("Open in a desktop browser", "Open this page over HTTPS or localhost in Chrome or Edge on macOS, Windows or Linux.");
}

connect.addEventListener("click", async () => {
  connect.disabled = true;
  hardware.hidden = true;
  port = null;
  clearUpdate();
  show("Choose the dial’s port", "Select USB JTAG/serial debug unit and click Connect. If the list is empty, close it to try the other orientation.", "choose");
  try {
    const selected = paired || await navigator.serial.requestPort({ filters: [filter] });
    port = selected;
    show("Reading firmware…", "Keep the dial connected.");
    let app;
    try {
      app = await readCompanion(selected);
    } catch {
      if (port === selected) show("Couldn’t read the port", "Close any serial monitor using the dial, reconnect it and choose Check my dial again.");
      return;
    }
    if (port !== selected) return;
    if (app && [1, 4, 5].includes(app.stage)) {
      show("Firmware update in progress", "The USB plug is the right way round. Leave the cable connected until the update finishes.", "confirmed");
    } else if (app) {
      const build = app.buildId ? ` · Build ID ${app.buildId}` : "";
      show("Cable orientation confirmed", `Keep the plug this way. Companion ${app.version} (${app.channel}, ${app.slot})${build}.`, "confirmed");
      hardware.hidden = false;
      update.hidden = !eligible(app);
    } else {
      show("Check the processor", "The port opened without a Companion reply. For a factory dial, keep the plug this way for now and choose Restart and check processor.");
      hardware.hidden = false;
    }
  } catch (error) {
    if (error.name === "NotFoundError") {
      show("No port selected", "If the list was empty, follow the half-turn shown below. If you closed it by mistake, choose Check my dial again.", "choose");
    } else {
      show("USB access unavailable", "Allow serial access in your desktop browser and choose Check my dial again.");
    }
  } finally {
    connect.disabled = false;
  }
});

hardware.addEventListener("click", async () => {
  if (!port) return;
  const selected = port;
  clearUpdate();
  port = null; // The deliberate restart can disconnect and re-enumerate this port.
  hardware.hidden = true;
  connect.disabled = true;
  show("Identifying processor…", "The screen may go dark briefly while the dial restarts.");
  try {
    const info = await readProcessor(selected, { ESPLoader, Transport });
    if (info.chip !== "ESP32-S3") {
      show(`${info.chip} detected`, "The dial needs its main ESP32-S3. Follow the half-turn below, reconnect and choose Check my dial again.", "choose");
    } else if (info.flash !== "16MB") {
      show("ESP32-S3 found", `Keep the plug this way. Verify flash capacity before installation. This model needs 16 MiB. Reported capacity is ${info.flash || "unknown"}.`);
    } else {
      show("Cable orientation confirmed", "ESP32-S3 · 16 MiB flash. Keep the plug this way. For a first install, check the Waveshare model name on the board or packaging.", "confirmed");
    }
  } catch {
    show("Couldn’t finish the processor check", "Reconnect or power-cycle the dial, close any serial monitor and choose Check my dial again.");
  } finally {
    connect.disabled = false;
  }
});

fileInput.addEventListener("change", async () => {
  const file = fileInput.files[0];
  image = null;
  confirm.hidden = true;
  preview.textContent = "";
  if (!file) return;
  try {
    if (file.size !== release.size) throw Error("Choose the 0.1.4 app-only firmware file.");
    const bytes = new Uint8Array(await file.arrayBuffer());
    await verifyImage(bytes);
    if (fileInput.files[0] !== file) return;
    image = bytes;
    preview.textContent = `Ready to install 0.1.4 · Build ID ${release.buildId}. Keep the cable connected through the restart.`;
    confirm.hidden = false;
  } catch (error) {
    if (fileInput.files[0] === file) preview.textContent = error.message;
  }
});

confirm.addEventListener("click", async () => {
  if (!image || !port || connect.disabled) return;
  const selected = port;
  const bytes = image;
  port = null;
  clearUpdate();
  hardware.hidden = true;
  connect.disabled = true;
  const progress = detail => show("Updating over USB", detail);
  try {
    progress("Checking the installed build and flash layout…");
    const source = await readCompanion(selected);
    if (!eligible(source)) throw Error("Recheck the dial’s running firmware before installing.");
    const slot = await withProcessor(selected, { ESPLoader, Transport }, loader => install(loader, source, bytes, progress));
    progress("Restarting and waiting for the dial to confirm its build…");
    const deadline = Date.now() + 45000;
    while (Date.now() < deadline) {
      await new Promise(resolve => setTimeout(resolve, 1200));
      const app = await readCompanion(selected, 1200).catch(() => null);
      if (healthyTarget(app, slot)) {
        show("USB update verified", `Companion ${app.version} (${app.channel}, ${app.slot}) · Build ID ${app.buildId}. On the dial, tap the top status arc and open Firmware to compare.`, "confirmed");
        return;
      }
    }
    throw Error("The dial has not confirmed the new build. Choose Check my dial to read the installed version.");
  } catch (error) {
    show("Recheck the dial", error.message + " Reconnect if needed, then check the cable and installed build.");
  } finally {
    connect.disabled = false;
  }
});
