#pragma once
#include <Arduino.h>
// HTTPClient removes chunk framing. Keep the entire bounded transport buffer
// in PSRAM so a large linked sheet never consumes the display's internal heap.
class Body : public Stream {
public:
    Body(size_t cap) : data(static_cast<uint8_t *>(ps_malloc(cap))), capacity(cap) {}
    ~Body() { free(data); }
    size_t write(uint8_t b) override { return write(&b, 1); }
    size_t write(const uint8_t *b, size_t n) override {
        if (!data || n > capacity - length) { setWriteError(); return 0; }
        memcpy(data + length, b, n);
        length += n;
        return n;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    uint8_t *data;
    size_t capacity, length = 0;
};
