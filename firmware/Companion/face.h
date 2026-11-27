#pragma once
#include <lvgl.h>

constexpr unsigned faceSize = 360;
constexpr unsigned iconSize = 128;
constexpr size_t backgroundLimit = 40960;
constexpr size_t iconLimit = 12288;

struct ImageBuffer {
    lv_img_dsc_t image{};
    lv_color_t *pixels = nullptr;
    bool decode(const uint8_t *jpeg, size_t length, unsigned side, bool shade);
};
bool validImage(const uint8_t *jpeg, size_t length, unsigned side);
