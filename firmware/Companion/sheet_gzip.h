#pragma once
#include "body.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

// D&D Beyond sends a single gzip member. Bound both compressed and expanded
// sizes before handing any bytes to the character parser.
constexpr size_t MaxSheetBytes = 2 * 1024 * 1024;
static inline uint32_t gzipLE32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

static inline const char *expandSheetGzip(Body &body) {
    const char *invalid = "Incomplete compressed sheet";
    const uint8_t *wire = body.data;
    if (body.length < 20 || wire[0] != 0x1f || wire[1] != 0x8b || wire[2] != 8) return invalid;
    const uint8_t flags = wire[3];
    if (flags & 0xe0) return invalid;
    const size_t trailer = body.length - 8;
    size_t start = 10;
    if (flags & 4) {
        if (trailer - start < 2) return invalid;
        const size_t extra = wire[start] | size_t(wire[start+1]) << 8;
        start += 2;
        if (extra > trailer - start) return invalid;
        start += extra;
    }
    for (unsigned optional : {8u, 16u}) {
        if (!(flags & optional)) continue;
        const void *end = memchr(wire + start, 0, trailer - start);
        if (!end) return invalid;
        start = static_cast<const uint8_t *>(end) - wire + 1;
    }
    if (flags & 2) {
        if (trailer - start < 2) return invalid;
        if ((mz_crc32(0, wire, start) & 0xffff) != (wire[start] | unsigned(wire[start+1]) << 8)) return invalid;
        start += 2;
    }
    const size_t expected = gzipLE32(wire + trailer + 4);
    if (!expected || start >= trailer) return invalid;
    if (expected > MaxSheetBytes) return "Sheet exceeds 2 MiB";
    auto *out = static_cast<uint8_t *>(ps_malloc(expected));
    auto *decoder = static_cast<tinfl_decompressor *>(ps_malloc(sizeof(tinfl_decompressor)));
    if (!out || !decoder) {
        free(decoder);
        free(out);
        return "Insufficient PSRAM";
    }
    tinfl_init(decoder);
    size_t consumed = trailer - start, written = expected;
    const auto status = tinfl_decompress(decoder, wire + start, &consumed, out, out, &written,
                                         TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(decoder);
    if (status != TINFL_STATUS_DONE || consumed != trailer-start || written != expected ||
        mz_crc32(0, out, written) != gzipLE32(wire + trailer)) {
        free(out);
        return invalid;
    }
    free(body.data);
    body.data = out;
    body.capacity = body.length = written;
    return nullptr;
}
