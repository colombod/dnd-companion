// This pair has a verified rollback-capable bootloader and an unchanged save format.
export const release = Object.freeze({
  version: "0.1.4", buildId: "ccc889911", size: 2369344,
  md5: "68ea1c2ffbc990a0e212518fe7437191",
  sha256: "08bbb9b88ae7c3328d212b3bdc16a4c6153defb945cc97b3387f9a857bc44dcc",
});
const release015 = Object.freeze({
  version: "0.1.5", buildId: "95a184a82", size: 2369344, md5: "be681b5b3a2673cabbafe17f15a9be6d",
  sha256: "7c7d387c30f4f97753b836a22d9ca3e30af01b1a9d7bc7c49ffd2cc1cf182879",
});
const release016 = Object.freeze({
  version: "0.1.6", buildId: "dcf978eee", size: 2369344, md5: "a2b57f2910732441df6ad402b43018a2",
  sha256: "d32437730e92bad4e3a776b7d2268885277e1d00bd7654b1ac178e3e42bd4059",
});
const routes = [
  { source: { version: "0.1.3", buildId: "1a8dc714d", size: 2365792, md5: "9ec6d6553f775838fcc5a9e3be1a92e3" }, channel: "dev", target: release },
  { source: release, channel: "release", target: release015 },
  { source: release015, channel: "release", target: release016 },
];
const appAddress = [0x10000, 0x310000];
const selectorAddress = [0xe000, 0xf000];
const sector = 4096;

export function eligible(app) {
  return !!updateRoute(app);
}

export function updateRoute(app) {
  if (app?.validity !== 2 || app.stage !== 0 || slotIndex(app.slot) < 0) return null;
  return routes.find(({source, channel}) => app.version === source.version &&
    app.buildId === source.buildId && app.channel === channel) || null;
}

function slotIndex(label) {
  return ["app0", "app1"].indexOf(label);
}

export async function verifyImage(image, target = release) {
  if (image.length !== target.size) throw Error(`Choose the ${target.version} app-only firmware file.`);
  const hash = new Uint8Array(await crypto.subtle.digest("SHA-256", image));
  const hex = Array.from(hash, byte => byte.toString(16).padStart(2, "0")).join("");
  if (hex !== target.sha256) throw Error(`The file checksum differs from the approved ${target.version} image.`);
}

function words(...values) {
  const bytes = new Uint8Array(4 * values.length);
  const view = new DataView(bytes.buffer);
  values.forEach((n, i) => view.setUint32(i * 4, n, true));
  return bytes;
}

function sequenceCrc(sequence) {
  // ESP-IDF uses esp_rom_crc32_le(UINT32_MAX, &ota_seq, 4).
  let crc = 0;
  for (const byte of words(sequence)) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return ~crc >>> 0;
}

export function bootPlan(records, slot) {
  const usable = [];
  for (const [index, record] of records.entries()) {
    if (record.length !== 32) throw Error("Incomplete boot record.");
    if (record.every(byte => byte === 0xff)) continue;
    const view = new DataView(record.buffer, record.byteOffset, 32);
    const seq = view.getUint32(0, true);
    const state = view.getUint32(24, true);
    if (!seq || seq === 0xffffffff || sequenceCrc(seq) !== view.getUint32(28, true))
      throw Error("Boot record checksum or sequence is invalid.");
    if (state === 3 || state === 4) continue; // ESP_OTA_IMG_INVALID / ABORTED
    if (state !== 2 && state !== 0xffffffff) throw Error("The dial has an unconfirmed boot record.");
    usable.push({ seq, state, index });
  }
  usable.sort((a, b) => b.seq - a.seq);
  const current = usable[0];
  if (!current || current.state !== 2 || current.seq >= 0xfffffffe ||
      current.seq === usable[1]?.seq || (current.seq - 1) % 2 !== slotIndex(slot))
    throw Error("The boot records do not confirm the running slot.");
  const next = current.seq + 1;
  const record = new Uint8Array(32).fill(0xff);
  record.set(words(next), 0);
  record.set(words(0, sequenceCrc(next)), 24); // NEW, subject to first-boot health checks
  return { address: appAddress[(next - 1) % 2], slot: "app" + ((next - 1) % 2),
    selector: selectorAddress[1 - current.index], record };
}

async function readRecord(loader, address) {
  const block = await loader.checkCommand("read boot record", 0x0e, words(address, 64), 0, 64);
  return block.slice(0, 32);
}

async function checkFlash(loader, address, size, expected, label) {
  // The S3 ROM returns its MD5 as 32 ASCII digits. No flasher stub is uploaded.
  const result = await loader.checkCommand("verify " + label, 0x13, words(address, size, 0, 0), 0, 32, 30000);
  if (new TextDecoder().decode(result).toLowerCase() !== expected)
    throw Error("The " + label + " checksum differs. USB update stopped.");
}

export async function install(loader, app, image, progress) {
  const route = updateRoute(app);
  if (!route) throw Error("The installed build is outside this USB update path.");
  await verifyImage(image, route.target);
  if (loader.chip.CHIP_NAME !== "ESP32-S3" || loader.IS_STUB !== false) throw Error("Expected the ESP32-S3 ROM.");
  const security = await loader.getSecurityInfo();
  if (security.flashCryptCnt !== 0 || security.parsedFlags.SECURE_BOOT_EN !== false ||
      security.parsedFlags.SECURE_DOWNLOAD_ENABLE !== false) throw Error("Unsupported flash protection settings.");
  await loader.flashSpiAttach(0);
  if (await loader.detectFlashSize() !== "16MB") throw Error("Expected 16 MiB of flash.");
  // SPI_SET_PARAMS sets the detected chip geometry in ROM RAM before accessing app1.
  await loader.checkCommand("set flash geometry", 0x0b, words(0, 16 * 1024 * 1024, 65536, sector, 256, 0xffff));
  await checkFlash(loader, 0, 0x8000, "15bf2a1f056b4eabbc98313956c90ffd", "bootloader");
  await checkFlash(loader, 0x8000, sector, "e26072f13a22f8f614b8e8b48bd41d7a", "partition map");
  await checkFlash(loader, appAddress[slotIndex(app.slot)], route.source.size, route.source.md5, "running firmware");
  progress("Reading both boot records…");
  const records = [];
  for (const address of selectorAddress) records.push(await readRecord(loader, address));
  const plan = bootPlan(records, app.slot);
  // ROM erases in multiples of the transfer block, rounded up to flash sectors.
  // A larger default block would erase the neighbouring boot record.
  loader.FLASH_WRITE_SIZE = 0x400;
  const options = { flashMode: "keep", flashFreq: "keep", flashSize: "keep", eraseAll: false };
  progress("Preparing the inactive slot…");
  await loader.writeFlash({ ...options, compress: true, fileArray: [{ address: plan.address, data: image }],
    reportProgress: (_i, sent, total) => progress("Writing " + plan.slot + " · " + Math.floor(100 * sent / total) + "%") });
  progress("Verifying the firmware on the dial…");
  await checkFlash(loader, plan.address, image.length, route.target.md5, "new firmware");
  progress("Selecting the verified firmware for its first boot…");
  await loader.writeFlash({ ...options, compress: false, fileArray: [{ address: plan.selector, data: plan.record }] });
  const written = await readRecord(loader, plan.selector);
  if (!plan.record.every((value, i) => value === written[i])) throw Error("Could not confirm the boot record.");
  return plan.slot;
}

export function healthyTarget(app, slot, target = release) {
  return app?.version === target.version && app.buildId === target.buildId && app.channel === "release" &&
    app.validity === 2 && app.slot === slot;
}
