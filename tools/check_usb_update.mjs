import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { bootPlan, eligible, healthyTarget, install, release, verifyImage } from "../usb/update.mjs";

const old = Uint8Array.from(Buffer.from("01000000ffffffffffffffffffffffffffffffffffffffffffffffff9a984347", "hex"));
const valid = Uint8Array.from(Buffer.from("02000000ffffffffffffffffffffffffffffffffffffffff020000007437f655", "hex"));
const source = { channel: "dev", version: "0.1.3", slot: "app1", buildId: "1a8dc714d", validity: 2, stage: 0 };
const plan = bootPlan([old, valid], "app1");

test("plan preserves the valid record whichever sector holds it", () => {
  assert.equal(plan.address, 0x10000);
  assert.equal(plan.selector, 0xe000);
  assert.equal(plan.slot, "app0");
  assert.equal(bootPlan([valid, old], "app1").selector, 0xf000);
  assert.deepEqual([...plan.record.slice(24, 28)], [0, 0, 0, 0]);
  const next = plan.record.slice();
  next[24] = 2;
  const reverse = bootPlan([next, valid], "app0");
  assert.equal(reverse.address, 0x310000);
  assert.equal(reverse.selector, 0xf000);
});

test("refuse mismatched, pending, ambiguous and corrupt boot records", () => {
  const corrupt = valid.slice();
  corrupt[31] ^= 1;
  for (const r of [[old, corrupt], [plan.record, valid], [valid, valid], [old, new Uint8Array(0)]])
    assert.throws(() => bootPlan(r, "app1"));
  assert.throws(() => bootPlan([old, valid], "app0"));
});

test("require exact healthy source, target, and local image", async () => {
  assert.ok(eligible(source));
  for (const change of [{ stage: 4 }, { validity: 1 }, { buildId: "" }, { channel: "release" }, { slot: "factory" }])
    assert.equal(eligible({ ...source, ...change }), false);
  const target = { ...source, ...release, channel: "release", slot: "app0" };
  assert.ok(healthyTarget(target, "app0"));
  assert.equal(healthyTarget({ ...target, validity: 1 }, "app0"), false);
  assert.equal(healthyTarget(target, "app1"), false);
  await assert.rejects(verifyImage(new Uint8Array(1)), /app-only/);
  await assert.rejects(verifyImage(new Uint8Array(release.size)), /checksum/);
});

test("qualified artifact stages only after source checks and switches only after readback", {
  skip: !process.env.COMPANION_USB_IMAGE && "Set COMPANION_USB_IMAGE to the qualified app-only binary",
}, async () => {
  const bytes = new Uint8Array(await readFile(process.env.COMPANION_USB_IMAGE));
  for (const fail of ["chip", "protected", "capacity", "bootloader", "partition map", "running firmware", "app write", "new firmware", null]) {
    const writes = [];
    const mock = {
      chip: { CHIP_NAME: fail === "chip" ? "other" : "ESP32-S3" }, IS_STUB: false,
      async getSecurityInfo() { return { flashCryptCnt: 0, parsedFlags: { SECURE_BOOT_EN: fail === "protected", SECURE_DOWNLOAD_ENABLE: false } }; },
      async flashSpiAttach() {}, async detectFlashSize() { return fail === "capacity" ? "8MB" : "16MB"; },
      async checkCommand(label, op, args) {
        const address = new DataView(args.buffer).getUint32(0, true);
        if (op === 0x0b) { assert.equal(new DataView(args.buffer).getUint32(4, true), 0x1000000); return 0; }
        if (op === 0x0e) return address === 0xe000 ? (writes.length === 2 ? plan.record : old) : valid;
        assert.equal(op, 0x13);
        const md5 = { 0: "15bf2a1f056b4eabbc98313956c90ffd", 32768: "e26072f13a22f8f614b8e8b48bd41d7a",
          3211264: "9ec6d6553f775838fcc5a9e3be1a92e3", 65536: "68ea1c2ffbc990a0e212518fe7437191" };
        return new TextEncoder().encode(label === "verify " + fail ? "0".repeat(32) : md5[address]);
      },
      async writeFlash(options) {
        assert.equal(options.eraseAll, false);
        assert.ok(this.FLASH_WRITE_SIZE <= 4096);
        writes.push(options.fileArray[0].address);
        if (fail === "app write") throw Error("Disconnected");
      },
    };
    const action = install(mock, source, bytes, () => {});
    if (fail) await assert.rejects(action);
    else assert.equal(await action, "app0");
    assert.deepEqual(writes, !fail ? [0x10000, 0xe000] : ["app write", "new firmware"].includes(fail) ? [0x10000] : []);
  }
});
