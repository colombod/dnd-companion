#include "rim_text.h"
#include <lvgl.h>
#include <cmath>

namespace {
struct Track {
    bool top;
    int radius;
    int maxAdvance; // Pixels measured along the baseline, before bending the text.
    int spacing;
};
constexpr Track tracks[] = {
    {true,  dial::labelRadius, 238, 1}, // Status
    {true,  131, 206, 0},               // Level and proficiency
    {true,   93, 232, 0},               // Character name, above the portrait
    {false, 106, 294, 0},               // Class names and levels
    {false, 130, 236, 0},               // Cache and refresh information
    {false, dial::labelRadius, 238, 1}, // Action
    {true,  142, 212, 1},               // Game screen title
    {false, dial::returnRadius, 134, 0} // Inset return control
};
struct CachedArc {
    String text;
    const lv_font_t *font=nullptr;
    lv_img_dsc_t image{};
    uint8_t *mask=nullptr;
    size_t capacity=0;
    int x=0,y=0;
};
CachedArc labels[sizeof(tracks)/sizeof(*tracks)];

int measure(const String &text,const lv_font_t *font,int spacing) {
    return lv_txt_get_width(text.c_str(),text.length(),font,spacing,LV_TEXT_FLAG_NONE);
}

String fit(const String &text,const lv_font_t *font,int spacing,int budget) {
    if (measure(text,font,spacing)<=budget) return text;
    const int dots=measure("...",font,spacing)+spacing;
    uint32_t i=0,last=0;
    while (i<text.length()) {
        _lv_txt_encoded_next(text.c_str(),&i);
        if (lv_txt_get_width(text.c_str(),i,font,spacing,LV_TEXT_FLAG_NONE)+dots>budget) break;
        last=i;
    }
    return text.substring(0,last)+"...";
}

void addCoverage(CachedArc &label,int x,int y,int alpha) {
    int width=label.image.header.w;
    if (x<0 || y<0 || x>=width || y>=label.image.header.h) return;
    auto &p=label.mask[y*width+x];
    p=std::min(255,int(p)+alpha);
}

bool renderMask(CachedArc &label,const String &text,const lv_font_t *font,const Track &track) {
    const int widthAlongArc=measure(text,font,track.spacing);
    const float halfAngle=widthAlongArc/(2.f*track.radius);
    const int pad=font->line_height/2+3;
    const int halfWidth=ceilf(track.radius*sinf(halfAngle))+pad;
    const int rise=ceilf(track.radius*(1.f-cosf(halfAngle)));
    const int width=2*halfWidth+1,height=rise+2*pad+1;
    size_t bytes=size_t(width)*height;
    if (bytes>label.capacity) {
        auto *larger=static_cast<uint8_t *>(ps_realloc(label.mask,bytes));
        if (!larger) return false;
        label.mask=larger;
        label.capacity=bytes;
    }
    lv_img_cache_invalidate_src(&label.image);
    label.x=dial::center-halfWidth;
    label.y=track.top ? dial::center-track.radius-pad : dial::center+track.radius-rise-pad;
    label.image.header.cf=LV_IMG_CF_ALPHA_8BIT;
    label.image.header.w=width;
    label.image.header.h=height;
    label.image.data_size=bytes;
    label.image.data=label.mask;
    memset(label.mask,0,bytes);
    float advance=-widthAlongArc/2.f;
    for (uint32_t i=0;i<text.length();) {
        auto letter=_lv_txt_encoded_next(text.c_str(),&i);
        uint32_t after=i;
        auto next=_lv_txt_encoded_next(text.c_str(),&after);
        lv_font_glyph_dsc_t g{};
        if (!lv_font_get_glyph_dsc(font,&g,letter,next) || g.is_placeholder) {
            // Keep the original advance when a character is outside the bundled font.
            int advanceWidth=g.adv_w;
            if (!advanceWidth) continue;
            letter='?';
            if (!lv_font_get_glyph_dsc(font,&g,letter,0)) continue;
            g.ofs_x+=(advanceWidth-int(g.adv_w))/2;
            g.adv_w=advanceWidth;
        }
        float a=(advance+g.adv_w/2.f)/track.radius;
        advance+=g.adv_w+track.spacing;
        if (!g.box_w || !g.box_h || g.bpp!=4) continue;
        const uint8_t *bitmap=lv_font_get_glyph_bitmap(g.resolved_font,letter);
        if (!bitmap) continue;
        const float sinA=sinf(a),cosA=cosf(a),turn=track.top?sinA:-sinA;
        const float cx=dial::center+track.radius*sinA-label.x;
        const float cy=dial::center+(track.top?-1:1)*track.radius*cosA-label.y;
        const float startX=g.ofs_x-g.adv_w/2.f;
        const float startY=font->line_height/2.f-font->base_line-g.box_h-g.ofs_y;
        for (int y=0;y<g.box_h;y++) for (int x=0;x<g.box_w;x++) {
            int index=y*g.box_w+x;
            int coverage=((bitmap[index/2]>>(index%2?0:4))&15)*17;
            if (!coverage) continue;
            float px=cx+(startX+x)*cosA-(startY+y)*turn;
            float py=cy+(startX+x)*turn+(startY+y)*cosA;
            int ix=floorf(px),iy=floorf(py);
            float fx=px-ix,fy=py-iy;
            addCoverage(label,ix,iy,lroundf(coverage*(1-fx)*(1-fy)));
            addCoverage(label,ix+1,iy,lroundf(coverage*fx*(1-fy)));
            addCoverage(label,ix,iy+1,lroundf(coverage*(1-fx)*fy));
            addCoverage(label,ix+1,iy+1,lroundf(coverage*fx*fy));
        }
    }
    label.text=text;
    label.font=font;
    return true;
}
}

String displayNetworkName(const String &ssid) {
    String text;
    for (uint32_t i=0;i<ssid.length();) {
        uint32_t start=i;
        uint32_t letter=_lv_txt_encoded_next(ssid.c_str(),&i);
        lv_font_glyph_dsc_t glyph{};
        if (letter>=32 && lv_font_get_glyph_dsc(&lv_font_montserrat_14,&glyph,letter,0) && !glyph.is_placeholder) {
            text.concat(ssid.c_str()+start,i-start);
        } else {
            for (uint32_t j=start;j<i;j++) {
                char escaped[5];
                snprintf(escaped,sizeof(escaped),"\\x%02X",uint8_t(ssid[j]));
                text+=escaped;
            }
        }
    }
    return text;
}

String connectionArc(const String &network,const String &suffix) {
    const auto &track=tracks[int(ArcLabel::Status)];
    const String prefix=LV_SYMBOL_WIFI " ";
    int remaining=track.maxAdvance-measure(prefix+suffix,&lv_font_montserrat_14,track.spacing)-2*track.spacing;
    return prefix+fit(network,&lv_font_montserrat_14,track.spacing,remaining)+suffix;
}

void arcText(const String &text,ArcLabel slot,uint32_t color) {
    if (!text.length()) return;
    auto &label=labels[int(slot)];
    const Track &track=tracks[int(slot)];
    const lv_font_t *font=&lv_font_montserrat_14;
    if (slot==ArcLabel::Name || slot==ArcLabel::Classes) {
        for (auto *size:{&lv_font_montserrat_36,&lv_font_montserrat_28,&lv_font_montserrat_20,&lv_font_montserrat_18}) {
            if (slot==ArcLabel::Classes && size->line_height>lv_font_montserrat_20.line_height) continue;
            if (measure(text,size,track.spacing)<=track.maxAdvance) {font=size;break;}
        }
    }
    String fitted=fit(text,font,track.spacing,track.maxAdvance);
    if ((label.text!=fitted || label.font!=font) && !renderMask(label,fitted,font,track)) {
        auto *fallback=lv_label_create(lv_scr_act());
        lv_label_set_text(fallback,fitted.c_str());
        lv_obj_set_style_text_font(fallback,font,0);
        lv_obj_set_style_text_color(fallback,lv_color_hex(color),0);
        lv_obj_align(fallback,LV_ALIGN_CENTER,0,track.top?-track.radius:track.radius);
        return;
    }
    auto *img=lv_img_create(lv_scr_act());
    lv_img_set_src(img,&label.image);
    lv_obj_set_style_img_recolor(img,lv_color_hex(color),0);
    lv_obj_set_style_img_recolor_opa(img,LV_OPA_COVER,0);
    lv_obj_set_pos(img,label.x,label.y);
}

void returnArc(const char *caption) {
    auto *band=lv_arc_create(lv_scr_act());
    lv_obj_remove_style_all(band);
    constexpr int diameter=2*(dial::returnRadius+dial::returnHalfWidth);
    lv_obj_set_size(band,diameter,diameter);
    lv_obj_center(band);
    lv_arc_set_bg_angles(band,90-dial::returnAngle,90+dial::returnAngle);
    lv_obj_set_style_arc_width(band,dial::returnHalfWidth*2,LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(band,true,LV_PART_MAIN);
    lv_obj_set_style_arc_opa(band,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_arc_color(band,lv_color_hex(0x273642),LV_PART_MAIN);
    lv_obj_clear_flag(band,LV_OBJ_FLAG_CLICKABLE);
    arcText(caption,ArcLabel::Return,0xf0e7d6);
}
