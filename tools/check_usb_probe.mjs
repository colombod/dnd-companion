import assert from "node:assert/strict";
import test from "node:test";
import { readCompanion, readProcessor } from "../usb/probe.mjs";

function serialPort(chunks) {
  const port = {
    async open({ baudRate }) {
      assert.equal(baudRate, 115200);
      this.readable = new ReadableStream({
        start(controller) {
          for (const part of chunks) controller.enqueue(new TextEncoder().encode(part));
        },
      });
      this.writable = new WritableStream({
        write(bytes) { assert.equal(new TextDecoder().decode(bytes), "STATUS\n"); },
      });
    },
    async close() {
      assert.equal(this.readable.locked, false);
      assert.equal(this.writable.locked, false);
      this.closed = true;
    },
  };
  return port;
}

const statusLine = "FIRMWARE image=DIAL/waveshare-esp32-s3-knob-touch-lcd-1.8/release/0.1.3 slot=app1 validity=2 stage=0 offered=\n";

test("recognizes a split firmware line and closes USB", async () => {
  const port = serialPort(["HW psram=8388608\nFIRM", statusLine.slice(4)]);
  assert.deepEqual(await readCompanion(port), { version: "0.1.3", channel: "release", slot: "app1", validity: 2, stage: 0 });
  assert.equal(port.closed, true);
});

test("a silent port, wrong target or truncated reply produces no firmware identity", async () => {
  for (const chunks of [[], [statusLine.replace("lcd-1.8", "amoled")], [statusLine.slice(0, 70)]]) {
    const port = serialPort(chunks);
    assert.equal(await readCompanion(port, 10), null);
    assert.equal(port.closed, true);
  }
});

test("returns the build ID supplied by newer firmware", async () => {
  const port = serialPort([statusLine.replace(" offered=", " build=123abc789 offered=")]);
  assert.equal((await readCompanion(port)).buildId, "123abc789");
});

test("processor check reads flash capacity only on S3 and releases the port on failure", async () => {
  for (const chip of ["ESP32", "ESP32-S3"]) {
    for (const capacity of ["16MB", "8MB", undefined, new Error("disconnected")]) {
      const calls = [];
      const port = { readable: {} };
      class Transport {
        async disconnect() { calls.push("close"); }
      }
      class ESPLoader {
        async detectChip() { this.chip = { CHIP_NAME: chip }; }
        async flashSpiAttach(arg) { assert.equal(arg, 0); calls.push("attach"); }
        async detectFlashSize() {
          calls.push("capacity");
          if (capacity instanceof Error) throw capacity;
          return capacity;
        }
        async after(mode, _, sequence) { assert.equal(sequence, "D0|R1|W150|R0"); calls.push(mode); }
      }
      const probe = readProcessor(port, { Transport, ESPLoader });
      if (chip === "ESP32-S3" && capacity instanceof Error) {
        await assert.rejects(probe, /disconnected/);
      } else {
        assert.deepEqual(await probe, chip === "ESP32-S3" ? { chip, flash: capacity } : { chip });
      }
      const reads = chip === "ESP32-S3" ? ["attach", "capacity"] : [];
      assert.deepEqual(calls, [...reads, "custom_reset", "close"]);
    }
  }
});
