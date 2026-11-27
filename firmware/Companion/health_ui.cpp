#include "health_ui.h"
#include "board.h"
#include "roll_aid.h"
#include "roll_animation.h"
#include "portraits.h"
#include "rim_text.h"
#include "round_controls.h"
#include "session_store.h"
#include "ui_language.h"
#include <esp_random.h>

namespace healthui {
namespace {
enum class Face { Health, Maximum, Damage, Healing, Death, Rolling, Review, Modifier, Revival, Receipt, Effects, EditEffect };
using vitals::Status;
using session::Op;
Face face;
uint32_t character;
bool effects,melee,critical;
int amount,mode,first,second,kept,adjust,aid,option;
String notice;
rollaid::Picker aidPicker;
constexpr uint32_t ink=0x18252a,ivory=0xf5eddf,gold=0xf5d390,teal=0xa8e3da;
constexpr dial::Capsule left{111,248,118,44},right{249,248,118,44},middle{180,249,164,44};
constexpr dial::Capsule upper{180,184,188,44},upperL{112,184,121,44},upperR{248,184,121,44};
constexpr dial::Capsule counter{180,133,215,44};

const char *const modes[]={"Disadvantage","Normal","Advantage"};
const char *status(Status st){
    switch(st){case Status::Dying:return "Dying";case Status::Stable:return "Stable";case Status::Dead:return "Dead";default:return "Hit points";}
}
session::Build build(const Sheet &sheet){return casting::build(bookui::currentBook(sheet));}
void line(const String &s,int y,int w=265){dial::text(s,y,w,&lv_font_montserrat_14,teal,true);}
String signedN(int n){return n>=0?String("+")+n:String(n);}
String counts(const vitals::Health &h){return String(ui::tr("Successes"))+" "+h.successes+"   •   "+ui::tr("Failures")+" "+h.failures;}
int wrap(int value,int n){return (value%n+n)%n;}
String choice(int index,int value){
    if(index==vitals::Guidance)return value?String("2024  •  ")+ui::tr(checks::name(value-1)):String("2014  •  ")+ui::tr("One check");
    if(index==vitals::Bardic)return String(value<4?"2014":"2024")+"  •  d"+(6+2*(value%4));
    return index==vitals::Bless?"+1d4":"-1d4";
}
bool change(const session::Build &b,session::Change op,const String &receipt){
    if(auto err=session::commit(character,b,op)){notice=ui::tr(err);return false;}
    notice=receipt;pulse();face=Face::Receipt;return true;
}
void roll(int steps){
    aidPicker={};
    first=rollaid::die(20);second=mode==1?0:rollaid::die(20);
    kept=mode==1?first:mode==2?std::max(first,second):std::min(first,second);
    adjust=0;rollanim::begin(steps,millis());face=Face::Rolling;
}
}
uint32_t owner(){return character;}
void open(const Sheet &sheet,bool showEffects){
    character=sheet.id;effects=showEffects;face=effects?Face::Effects:Face::Health;
    notice="";aid=0;aidPicker={};
}
String reminder(const vitals::Aids &a,vitals::Roll roll,int skill){
    String out;
    for(int i=0;i<vitals::AidCount;++i){
        if(!vitals::applies(a,vitals::Aid(i),roll,skill))continue;
        if(out.length())out+="  •  ";
        out+=ui::tr(rollaid::names[i]);
        if(i==vitals::Bardic){out+=" d";out+=6+2*(a.bardic%4);}
        else out+=i==vitals::Bane?" -d4":" +d4";
    }
    return out;
}
void draw(const Sheet &sheet){
    auto b=build(sheet);auto s=session::read(character,b);const auto &h=s.hp;
    if(auto art=playBackdrop(false)){auto *img=lv_img_create(lv_scr_act());lv_img_set_src(img,art);lv_obj_set_style_img_opa(img,LV_OPA_70,0);}
    arcText(ui::tr(effects?"Roll effects":"Health"),ArcLabel::PlayTitle,gold);
    auto heading=[](const String &str){dial::text(str,89,242,&lv_font_montserrat_20,ivory,true);};
    auto number=[](const String &str){dial::text(str,133,256,&lv_font_montserrat_28,gold,true);};
    if(!casting::playable(bookui::currentBook(sheet)["schema"]|0u)){
        heading(ui::tr("Refresh your sheet"));middle.draw(ui::tr("Tap to refresh"),ink,gold);
        returnArc(ui::tr("Back to sheet"));return;
    }
    if(aidPicker.open){aidPicker.draw(s.aids);returnArc(ui::tr("Back to roll"));return;}
    switch(face){
    case Face::Health:
        if(!h.maximum){
            heading(ui::tr("Set up hit points"));line(ui::tr("Use the maximum on your sheet"),155);
            middle.draw(ui::tr("Set maximum"),ink,gold);break;
        }
        heading(ui::tr(status(h.status)));number(String(h.current)+" / "+h.maximum+" HP");
        if(h.status==Status::Dying){
            line(counts(h),155);
            upperL.draw(ui::tr("Death save"),ink,gold);upperR.draw(ui::tr("Stabilize"),ivory,ink);
        }else if(h.status==Status::Dead){line(ui::tr("Confirm revival at the table"),185);middle.draw(ui::tr("Revival"),ivory,ink);break;}
        else if(h.status==Status::Stable){line(ui::tr("Unconscious  •  no saves"),172);line(h.recoveryRounds?String((h.recoveryRounds+9)/10)+ui::tr(" min until 1 HP"):String(ui::tr("1 HP after 1d4 hours")),201);}
        else {line(String(s.temp)+ui::tr(" temporary HP"),177);line(ui::tr("Tap HP to edit maximum"),205);}
        left.draw(ui::tr("Damage"),ivory,ink);right.draw(ui::tr("Heal"),ivory,ink);break;
    case Face::Maximum:case Face::Damage:case Face::Healing:case Face::Modifier:
        heading(ui::tr(face==Face::Maximum?"Maximum HP":face==Face::Damage?"Incoming damage":face==Face::Healing?"Healing":"Save modifier"));
        number(String(LV_SYMBOL_MINUS "     ")+(face==Face::Modifier?signedN(adjust):String(amount))+"     " LV_SYMBOL_PLUS);
        if(face==Face::Damage){
            upperL.draw(ui::tr(melee?"Melee hit":"Other"),ivory,ink);
            upperR.draw(ui::tr(critical?"Critical hit":"Normal hit"),ivory,ink);
        }else if(face==Face::Maximum){
            line(ui::tr(b.hpHint || h.maximum?"Confirm against your sheet":"Enter sheet maximum"),176);
            line(String(ui::tr("Current HP"))+"  "+String(h.maximum?std::min(int(h.current),amount):std::max(0,amount-b.removedHP)),200);
        }else line(ui::tr(face==Face::Modifier?"Include applicable bonuses":"Turn to change amount"),179);
        middle.draw(ui::tr(face==Face::Modifier?"Done":"Confirm"),ink,gold);break;
    case Face::Death:
        heading(ui::tr("Start of your turn"));number("d20");
        upper.draw(ui::tr(modes[mode]),ivory,ink);
        line(reminder(s.aids,vitals::Roll::Death),215);
        middle.draw(ui::tr("Tap or spin"),ink,gold);break;
    case Face::Rolling:
        heading(ui::tr("Death save"));rollanim::draw();line("d20",244);break;
    case Face::Review:
        {String hint=(kept==1 || kept==20)?String(""):reminder(s.aids,vitals::Roll::Death);
        if(aidPicker.receipt.length())hint=aidPicker.receipt+(hint.length()?"  •  "+hint:"");
        if(hint.length())line(hint,85,240);else heading(ui::tr("Death save"));}
        number(String(kept)+" "+signedN(adjust)+" = "+(kept+adjust));
        line(second?String("d20  ")+first+" / "+second:String("d20  ")+first,163);
        line(ui::tr(kept==20?"Regain 1 HP":kept==1?"Two failures":kept+adjust>=10?"Success":"Failure"),194);
        left.draw(ui::tr("Modifier"),ivory,ink);right.draw(ui::tr("Record"),ink,gold);break;
    case Face::Revival:
        heading(ui::tr("Revival"));line(ui::tr("After a revival effect resolves"),163);
        middle.draw(ui::tr("Return at 1 HP"),ink,gold);break;
    case Face::Receipt:
        heading(ui::tr("Recorded"));dial::text(notice,146,272,&lv_font_montserrat_18,ivory,true);
        if(!effects){line(String(ui::tr(status(h.status)))+"   "+h.current+" / "+h.maximum+" HP",184);if(h.status==Status::Dying)line(counts(h),205);}
        left.draw(ui::tr("Undo"),ivory,ink);right.draw(ui::tr(!effects && h.status==Status::Dying?"Death saves":"Done"),ivory,ink);break;
    case Face::Effects:
        heading(String(LV_SYMBOL_LEFT "  ")+ui::tr(rollaid::names[aid])+"  " LV_SYMBOL_RIGHT);
        number(s.aids.rounds[aid]?ui::tr("Active"):ui::tr("Off"));
        if(s.aids.rounds[aid])line(choice(aid,aid==vitals::Guidance?s.aids.guidance:s.aids.bardic),173);
        line(ui::tr("Turn to browse effects"),204);
        left.draw(ui::tr("Set"),ivory,ink);if(s.aids.rounds[aid])right.draw(ui::tr("Clear"),ivory,ink);break;
    case Face::EditEffect:
        heading(ui::tr(rollaid::names[aid]));number(choice(aid,option));
        line(ui::tr(aid==vitals::Guidance?(option?"Only the selected skill":"One ability check"):aid==vitals::Bardic?(option<4?"Before outcome is known":"After a failed D20 Test"):"Attacks and saves"),180);
        middle.draw(ui::tr("Received"),ink,gold);break;
    }
    if(notice.length() && face!=Face::Receipt)line(notice,278,210);
    returnArc(ui::tr(face==Face::Health || face==Face::Effects?"Back to sheet":face==Face::Modifier?"Back to roll":effects?"Back to effects":"Back to health"));
}
bool turn(const Sheet &sheet,int steps){
    if(!steps || sheet.id!=character)return false;
    if(aidPicker.open)return aidPicker.choose(session::read(character,build(sheet)).aids,vitals::Roll::Death,-1,steps>0?1:-1);
    if(face==Face::Death){roll(steps);return true;}
    if(face==Face::Rolling){rollanim::sample(steps,millis());return false;}
    if(face==Face::Maximum || face==Face::Damage || face==Face::Healing)amount=std::clamp(amount+steps,1,vitals::limit);
    else if(face==Face::Modifier)adjust=std::clamp(adjust+steps,-100,100);
    else if(face==Face::Effects)aid=wrap(aid+steps,vitals::AidCount);
    else if(face==Face::EditEffect)option=wrap(option+steps,aid==vitals::Guidance?19:aid==vitals::Bardic?8:1);
    else return false;
    return true;
}
bool tick(uint32_t now){
    if(face!=Face::Rolling || !rollanim::finished(now))return false;
    face=Face::Review;pulse();return true;
}
bookui::Event touch(const Sheet &sheet,int x,int y,int dx,int dy){
    using E=bookui::Event;
    if(sheet.id!=character)return E::Back;
    if(face==Face::Rolling)return E::None;
    bool tap=abs(dx)<=18 && abs(dy)<=18;
    bool back=(dx>=42 && dx>abs(dy)) || (tap && dial::inReturn(x,y));
    if(aidPicker.open){
        if(back)aidPicker.open=false;
        else if(tap && dial::Capsule{180,250,174,44}.contains(x,y))adjust+=aidPicker.take(character,build(sheet),vitals::Roll::Death,-1);
        else if(tap && y>=107 && y<=157)turn(sheet,x<180?-1:1);
        return E::Paint;
    }
    if(back){
        if(face==Face::Health || face==Face::Effects)return E::Back;
        face=face==Face::Modifier?Face::Review:effects?Face::Effects:Face::Health;notice="";return E::Paint;
    }
    if(!tap){
        if(abs(dy)>=42 && abs(dy)>=abs(dx) && (face==Face::EditEffect || face==Face::Effects))
            return turn(sheet,dy<0?1:-1)?E::Paint:E::None;
        return E::None;
    }
    if(!casting::playable(bookui::currentBook(sheet)["schema"]|0u))return middle.contains(x,y)?E::Refresh:E::None;
    auto b=build(sheet);auto s=session::read(character,b);const auto &h=s.hp;
    notice="";
    session::Change c{Op::Heal};
    switch(face){
    case Face::Health:
        if((!h.maximum && middle.contains(x,y)) || (h.maximum && counter.contains(x,y))){
            amount=h.maximum?h.maximum:std::clamp(b.hpHint,1,vitals::limit);face=Face::Maximum;
        }else if(!h.maximum)return E::None;
        else if(h.status==Status::Dead){if(middle.contains(x,y))face=Face::Revival;}
        else if(h.status==Status::Dying && upperL.contains(x,y)){mode=1;face=Face::Death;}
        else if(h.status==Status::Dying && upperR.contains(x,y)){
            c.op=Op::Stabilize;c.recoveryHours=1+int(esp_random()%4);change(b,c,ui::tr("Stable at 0 HP"));
        }else if(left.contains(x,y) || right.contains(x,y)){
            face=left.contains(x,y)?Face::Damage:Face::Healing;amount=1;melee=critical=false;
        }
        break;
    case Face::Maximum:case Face::Healing:case Face::Damage:case Face::Modifier:{
        if(counter.contains(x,y)){turn(sheet,x<180?-1:1);break;}
        if(face==Face::Damage && upperL.contains(x,y)){melee=!melee;break;}
        if(face==Face::Damage && upperR.contains(x,y)){critical=!critical;break;}
        if(!middle.contains(x,y))return E::None;
        if(face==Face::Modifier){face=Face::Review;break;}
        c.op=face==Face::Maximum?Op::SetMaxHP:face==Face::Damage?Op::TakeHit:Op::Heal;
        c.amount=amount;c.melee=melee;c.critical=critical;
        int cold=session::retaliation(s,melee && c.op==Op::TakeHit);
        change(b,c,cold?String(cold)+ui::tr(" cold to attacker"):ui::tr("Hit points updated"));break;
    }
    case Face::Death:
        if(upper.contains(x,y))mode=wrap(mode+1,3);
        else if(middle.contains(x,y) || counter.contains(x,y))roll(1);
        break;
    case Face::Rolling:break;
    case Face::Review:
        if(kept!=1 && kept!=20 && dial::Capsule{180,85,231,44}.contains(x,y)){aidPicker.start(s.aids,vitals::Roll::Death,-1);break;}
        if(left.contains(x,y))face=Face::Modifier;
        else if(right.contains(x,y)){c.op=Op::DeathSave;c.amount=kept;c.points=adjust;c.recoveryHours=1+int(esp_random()%4);change(b,c,String("d20  ")+kept+" "+signedN(adjust));}
        break;
    case Face::Revival:if(middle.contains(x,y)){c.op=Op::Revive;change(b,c,ui::tr("Returned at 1 HP"));}break;
    case Face::Receipt:
        if(left.contains(x,y) && !session::undo(character)){notice=ui::tr("Could not undo");break;}
        if(left.contains(x,y) || right.contains(x,y))face=effects?Face::Effects:Face::Health;
        break;
    case Face::Effects:
        if(y>=66 && y<=111){turn(sheet,x<180?-1:1);break;}
        if(left.contains(x,y)){option=aid==vitals::Guidance?s.aids.guidance:aid==vitals::Bardic?s.aids.bardic:0;face=Face::EditEffect;}
        else if(right.contains(x,y) && s.aids.rounds[aid]){c.op=Op::SetAid;c.pool=aid;c.amount=-1;change(b,c,ui::tr("Effect cleared"));}
        break;
    case Face::EditEffect:
        if(counter.contains(x,y)){turn(sheet,x<180?-1:1);break;}
        if(middle.contains(x,y)){c.op=Op::SetAid;c.pool=aid;c.amount=option;change(b,c,ui::tr("Effect active"));}
        break;
    }
    return E::Paint;
}
}
