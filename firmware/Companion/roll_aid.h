#pragma once
#include "round_controls.h"
#include "session_store.h"
#include "ui_language.h"
#include <esp_random.h>
namespace rollaid {
constexpr const char *names[]={"Guidance","Bardic Inspiration","Bless","Bane"};
inline int die(int sides){
    uint32_t n,limit=(0u-uint32_t(sides))%uint32_t(sides);
    do{n=esp_random();}while(n<limit);
    return 1+int(n%uint32_t(sides));
}
// Ephemeral choices belong to one roll. A one-use benefit is committed before its die changes the result.
struct Picker {
    bool open=false;
    unsigned used=0;
    int selected=0;
    String receipt;
    bool choose(const vitals::Aids &a,vitals::Roll kind,int skill,int step){
        for(int i=0;i<vitals::AidCount;i++){
            selected=(selected+step+vitals::AidCount)%vitals::AidCount;
            if(!(used&(1u<<selected)) && vitals::applies(a,vitals::Aid(selected),kind,skill))return true;
        }
        return false;
    }
    bool start(const vitals::Aids &a,vitals::Roll kind,int skill){
        selected=vitals::AidCount-1;open=choose(a,kind,skill,1);return open;
    }
    void draw(const vitals::Aids &a){
        dial::text(ui::tr("Apply roll effect"),83,236,&lv_font_montserrat_20,0xa8e3da);
        dial::text(String(LV_SYMBOL_LEFT "  ")+ui::tr(names[selected])+"  " LV_SYMBOL_RIGHT,132,272,&lv_font_montserrat_20,0xf5eddf,true);
        dial::text(String(selected==vitals::Bane?"-d":"+d")+vitals::dieSize(a,vitals::Aid(selected)),172,200,&lv_font_montserrat_28,0xf5d390);
        const char *hint=selected==vitals::Bardic?(a.bardic<4?"Before outcome is known":"After a failed D20 Test"):"Include applicable bonuses";
        dial::text(ui::tr(hint),202,262,&lv_font_montserrat_14,0xa8e3da,true);
        dial::Capsule{180,250,174,44}.draw(ui::tr(selected==vitals::Bardic && a.bardic>=4?"Failed • use die":"Use die"),0x18252a,0xf5d390);
    }
    int take(uint32_t id,const session::Build &b,vitals::Roll kind,int skill){
        open=false;
        auto a=session::read(id,b).aids;
        if((used&(1u<<selected)) || !vitals::applies(a,vitals::Aid(selected),kind,skill))return 0;
        auto aid=vitals::Aid(selected);
        bool spend=vitals::singleUse(a,aid);
        int face=die(vitals::dieSize(a,aid)),value=0;
        if(!vitals::useDie(a,aid,kind,skill,face,value))return 0;
        if(spend){
            session::Change c{session::Op::UseAid};c.pool=selected;c.amount=face;c.roll=kind;c.skill=skill;
            if(auto err=session::commit(id,b,c)){receipt=ui::tr(err);return 0;}
        }
        used|=1u<<selected;
        receipt=String(ui::tr(names[selected]))+(value>0?" +":" ")+value;
        return value;
    }
};
}
