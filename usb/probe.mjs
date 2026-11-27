const identity = /^FIRMWARE image=DIAL\/waveshare-esp32-s3-knob-touch-lcd-1\.8\/(release|dev)\/(\d+\.\d+\.\d+) slot=(app[01]) validity=(-?\d+) stage=(\d+)\b(?: build=([0-9a-f]{8,64})\b)?/;

export async function readCompanion(port, timeout = 2200) {
  await port.open({ baudRate: 115200 });
  try {
    const reader = port.readable.getReader();
    const writer = port.writable.getWriter();
    const timer = setTimeout(() => reader.cancel().catch(() => {}), timeout);
    try {
      await writer.write(new TextEncoder().encode("STATUS\n"));
      const decoder = new TextDecoder();
      let partial = "";
      while (true) {
        const { done, value } = await reader.read();
        if (done) return null;
        const lines = (partial + decoder.decode(value, { stream: true })).split("\n");
        partial = lines.pop().slice(-1024);
        for (const line of lines) {
          const match = identity.exec(line);
          if (match) {
            const app = { channel: match[1], version: match[2], slot: match[3], validity: Number(match[4]), stage: Number(match[5]) };
            if (match[6]) app.buildId = match[6];
            return app;
          }
        }
      }
    } finally {
      clearTimeout(timer);
      await reader.cancel().catch(() => {});
      reader.releaseLock();
      writer.releaseLock();
    }
  } finally {
    await port.close();
  }
}

export async function withProcessor(port, { ESPLoader, Transport }, inspect) {
  const transport = new Transport(port);
  const terminal = { clean() {}, writeLine() {}, write() {} };
  const loader = new ESPLoader({ transport, baudrate: 115200, terminal });
  try {
    await loader.detectChip("default_reset", 2);
    return await inspect(loader);
  } finally {
    try {
      // On the native USB Serial/JTAG port, pulse RTS with GPIO0 released.
      // esptool-js 0.7.1's hard_reset only deasserts RTS on this transport.
      if (loader.chip) await loader.after("custom_reset", false, "D0|R1|W150|R0");
    } finally {
      if (port.readable || port.writable) await transport.disconnect();
    }
  }
}

export function readProcessor(port, tools) {
  return withProcessor(port, tools, async loader => {
    const chip = loader.chip.CHIP_NAME;
    if (chip !== "ESP32-S3") return { chip };
    await loader.flashSpiAttach(0);
    return { chip, flash: await loader.detectFlashSize() };
  });
}
