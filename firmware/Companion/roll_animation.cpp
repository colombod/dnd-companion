#include "roll_animation.h"
#include "dice_gestures.h"
#include <Arduino.h>
#include <lvgl.h>

namespace rollanim {
namespace {
play::SpinMotion motion;
lv_obj_t *rollingDie=nullptr;
const uint8_t spinPixels[][5472]={
#include "spin_frames.inc"
};
constexpr int spinFrames=sizeof(spinPixels)/sizeof(spinPixels[0]);
lv_img_dsc_t spinSprites[spinFrames]{};
uint32_t frameAt=0;
int spinAngle=0;
void animateSpin(uint32_t now){
    if(!rollingDie || now-frameAt<36)return;
    unsigned elapsed=now-motion.start;
    int remaining=std::max(0,100-int(elapsed*100/motion.duration()));
    auto dt=std::min(uint32_t(75),now-frameAt);
    frameAt=now;
    int rate=220+(370+190*motion.strength)*remaining/100;
    spinAngle=(spinAngle+motion.direction*int(dt)*rate/1000+360)%360;
    auto i=spinAngle*spinFrames/360;
    auto &sprite=spinSprites[i];
    if(!sprite.data){
        sprite.header.cf=LV_IMG_CF_INDEXED_4BIT;sprite.header.w=sprite.header.h=104;
        sprite.data=spinPixels[i];sprite.data_size=sizeof(spinPixels[i]);
    }
    lv_img_set_src(rollingDie,&sprite);
}
}
void begin(int steps,uint32_t now){motion.begin(steps,now);spinAngle=270;rollingDie=nullptr;}
void sample(int steps,uint32_t now){motion.sample(steps,now);}
void draw(){
    rollingDie=lv_img_create(lv_scr_act());lv_obj_set_pos(rollingDie,128,120);
    frameAt=millis()-36;animateSpin(millis());
}
bool finished(uint32_t now){
    if(now-motion.start>=motion.duration())return true;
    animateSpin(now);return false;
}
unsigned duration(){return motion.duration();}
int strength(){return motion.strength;}
int direction(){return motion.direction;}
}
