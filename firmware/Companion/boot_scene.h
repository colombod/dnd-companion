#include "ui_language.h"
#pragma once
#include "portraits.h"
namespace boot {
inline constexpr unsigned duration=1460;
inline lv_obj_t *scene,*title,*embers[7];
inline uint32_t last;
inline void draw(){
    scene=lv_img_create(lv_scr_act());
    if(auto art=bootBackdrop())lv_img_set_src(scene,art);
    lv_obj_set_pos(scene,0,0);lv_obj_set_style_img_opa(scene,LV_OPA_30,0);
    title=lv_label_create(lv_scr_act());lv_label_set_text(title,ui::tr("Character Companion"));
    lv_obj_set_style_text_font(title,&lv_font_montserrat_20,0);
    lv_obj_set_style_text_color(title,lv_color_hex(0xffdfad),0);
    lv_obj_align(title,LV_ALIGN_CENTER,0,90);lv_obj_set_style_text_opa(title,0,0);
    for(int i=0;i<7;i++){
        auto *p=embers[i]=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(p);
        lv_obj_set_size(p,i%3+3,i%3+3);lv_obj_set_style_radius(p,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_color(p,lv_color_hex(i%2?0xffac45:0xffedb0),0);
        lv_obj_set_style_bg_opa(p,0,0);
    }
    last=0;
}
inline void tick(unsigned elapsed){
    if(elapsed<last+36)return;
    last=elapsed;
    if(elapsed<320)lv_obj_set_style_img_opa(scene,std::min(255u,76+elapsed*179/240),0);
    lv_obj_set_style_text_opa(title,elapsed<=430?0:std::min(255u,(elapsed-430)*255/320),0);
    for(int i=0;i<7;i++){
        int t=int(elapsed)-110-i*53;
        if(t<0 || t>740){lv_obj_set_style_bg_opa(embers[i],0,0);continue;}
        int x=202+(28+i*9)*t/740,y=107-(55+i*9)*t/740+18*t*t/(740*740);
        lv_obj_set_pos(embers[i],x,y);
        lv_obj_set_style_bg_opa(embers[i],255-255*t/740,0);
    }
}
}
