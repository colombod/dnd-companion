#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <algorithm>
namespace dial {
inline void text(const String &s,int y,int w,const lv_font_t *font,uint32_t color,bool singleLine=false){
    String display=s;
    display.replace("’","'");display.replace("‘","'");display.replace("“","\"");display.replace("”","\"");
    display.replace("–","-");display.replace("—","-");display.replace("−","-");display.replace("…","...");
    auto *l=lv_label_create(lv_scr_act());lv_label_set_text(l,display.c_str());
    if(singleLine && lv_txt_get_width(display.c_str(),display.length(),font,0,LV_TEXT_FLAG_NONE)>w){
        font=&lv_font_montserrat_14;
        lv_label_set_long_mode(l,LV_LABEL_LONG_SCROLL_CIRCULAR);
    }
    lv_obj_set_width(l,w);lv_obj_set_style_text_font(l,font,0);
    lv_obj_set_style_text_color(l,lv_color_hex(color),0);
    lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_align(l,LV_ALIGN_CENTER,0,y-180);
}
// The same capsule defines the painted surface and the touch target.
struct Capsule{
    int x,y,w,h;
    bool contains(int px,int py) const{
        int dx=std::max(0,abs(px-x)-(w-h)/2),dy=py-y;
        return dx*dx+dy*dy<=h*h/4;
    }
    void draw(const String &title,uint32_t ink,uint32_t fill,const char *caption=nullptr) const{
        auto *b=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(b);
        lv_obj_set_size(b,w,h);lv_obj_set_pos(b,x-w/2,y-h/2);
        lv_obj_set_style_radius(b,h/2,0);lv_obj_set_style_bg_color(b,lv_color_hex(fill),0);
        lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0);
        auto label=[&](const char *value,int offset,const lv_font_t *font){
            auto *l=lv_label_create(b);lv_label_set_text(l,value);
            if(lv_txt_get_width(value,strlen(value),font,0,LV_TEXT_FLAG_NONE)>w-12)font=&lv_font_montserrat_14;
            lv_obj_set_width(l,w-8);
            lv_label_set_long_mode(l,LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
            lv_obj_set_style_text_font(l,font,0);lv_obj_set_style_text_color(l,lv_color_hex(ink),0);
            lv_obj_align(l,LV_ALIGN_CENTER,0,offset);
        };
        label(title.c_str(),caption?-8:0,&lv_font_montserrat_18);
        if(caption)label(caption,10,&lv_font_montserrat_14);
    }
};
}
