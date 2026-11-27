#include "face.h"
#include <Arduino.h>
#include <extra/libs/sjpg/tjpgd.h>

struct Decoder {
    const uint8_t *input;
    size_t remaining;
    lv_color_t *pixels;
    bool shade;
};
static size_t readJpeg(JDEC *jd, uint8_t *out, size_t n) {
    auto *s = static_cast<Decoder *>(jd->device);
    n = std::min(n, s->remaining);
    if (out) memcpy(out, s->input, n);
    s->input += n;
    s->remaining -= n;
    return n;
}
static int drawBlock(JDEC *jd, void *rgb, JRECT *r) {
    auto *s = static_cast<Decoder *>(jd->device);
    if (!s->pixels) return 1;
    auto *src = static_cast<uint8_t *>(rgb);
    for (unsigned y = r->top; y <= r->bottom; ++y) {
        auto *pixel = s->pixels + y * jd->width + r->left;
        int shade = 0;
        if (s->shade) {
            // Leave the equator visible and darken the two text bands.
            int distance = abs(int(y) - 180);
            shade = std::min(205, 74 + distance * 2);
        }
        for (unsigned x = r->left; x <= r->right; ++x) {
            auto mix = [shade](int color, int ink) { return (color*(255-shade) + ink*shade)/255; };
            *pixel++ = lv_color_make(mix(src[0],9), mix(src[1],14), mix(src[2],25));
            src += 3;
        }
    }
    return 1;
}
static bool decodeJpeg(Decoder &source, unsigned side) {
    JDEC jd{};
    auto *work=malloc(4096);
    bool ok=work && jd_prepare(&jd,readJpeg,work,4096,&source)==JDR_OK &&
            jd.width==side && jd.height==side && jd_decomp(&jd,drawBlock,0)==JDR_OK;
    free(work);
    return ok;
}
bool validImage(const uint8_t *jpeg,size_t length,unsigned side) {
    Decoder source{jpeg,length,nullptr,false};
    return decodeJpeg(source, side);
}
bool ImageBuffer::decode(const uint8_t *jpeg,size_t length,unsigned side,bool shade) {
    size_t bytes=side*side*sizeof(lv_color_t);
    if (!pixels) pixels=static_cast<lv_color_t *>(ps_malloc(bytes));
    if (!pixels) return false;
    Decoder source{jpeg,length,pixels,shade};
    if (!decodeJpeg(source,side)) return false;
    image.header.cf=LV_IMG_CF_TRUE_COLOR;
    image.header.w=image.header.h=side;
    image.data_size=bytes;
    image.data=reinterpret_cast<uint8_t *>(pixels);
    return true;
}
