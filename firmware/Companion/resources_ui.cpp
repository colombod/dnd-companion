#include "ui_language.h"
#include "resources_ui.h"
#include "cast_import.h"
#include "session_store.h"
#include "portraits.h"
#include "rim_text.h"
#include "board.h"
#include "round_controls.h"
namespace resources {
namespace {
enum class Stage { Menu,Preview,Receipt };
Stage stage;bool rest,restore;uint32_t character;
int candidates[30],count,index,metaLevel,metaCost;
session::Op op;
String message;
constexpr uint32_t ivory=0xf3ecd9,gold=0xffcf8c,teal=0x9edacd;
void label(String str,int y,int w,const lv_font_t *font,uint32_t color){
    auto *l=lv_label_create(lv_scr_act());lv_label_set_text(l,str.c_str());lv_obj_set_width(l,w);
    lv_obj_set_style_text_font(l,font,0);lv_obj_set_style_text_color(l,lv_color_hex(color),0);
    lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_align(l,LV_ALIGN_CENTER,0,y-180);
}
int sumSlots(const session::Build &b,const session::State &s,int first,int last){
    int sum=0;for(int i=first;i<last;i++)sum+=session::remaining(b,s,i);return sum;
}
String pool(int p){return String(p>=9?ui::tr("Pact L"):ui::tr("Spell L"))+session::level(p);}
const char *name(session::Op op){
    switch(op){
    case session::Op::SlotToPoints:return ui::tr("Slot to points");
    case session::Op::PointsToSlot:return ui::tr("Points to slot");
    case session::Op::ShortRest:return ui::tr("Short rest");
    case session::Op::LongRest:return ui::tr("Long rest");
    case session::Op::Cast:return ui::tr("Cast");
    case session::Op::Stock:case session::Op::ItemUse:return ui::tr("Pack");
    case session::Op::Coin:return ui::tr("Purse");
    case session::Op::MarkTurn:case session::Op::SpendTurn:case session::Op::StartTurn:case session::Op::EndTracking:return ui::tr("My turn");
    case session::Op::TakeHit:return ui::tr("Hit");
    case session::Op::Elapse:return ui::tr("Game time");
    case session::Op::GrantTemp:return ui::tr("Temp HP");
    case session::Op::SetMaxHP:return ui::tr("Maximum HP");
    case session::Op::Heal:return ui::tr("Heal");
    case session::Op::Stabilize:return ui::tr("Stabilize");
    case session::Op::DeathSave:return ui::tr("Death save");
    case session::Op::Revive:return ui::tr("Revival");
    case session::Op::UseAid:
    case session::Op::SetAid:return ui::tr("Roll effects");
    case session::Op::FlashOfGenius:return ui::tr("Flash of Genius");
    case session::Op::UseMisc:return ui::tr("AC bonus");
    default:return ui::tr("Metamagic");
    }
}
void preview(JsonDocument &book,session::Op next){
    stage=Stage::Preview;op=next;index=count=0;restore=false;metaLevel=metaCost=1;message="";
    auto b=casting::build(book);auto s=session::read(character,b);
    if(op==session::Op::SlotToPoints){
        for(int p=0;p<18;p++)if(session::remaining(b,s,p))candidates[count++]=p;
    }else if(op==session::Op::PointsToSlot){
        for(int l=1;l<=5;l++)if(session::canCreate(b,l))candidates[count++]=l-1;
    }else if(op==session::Op::Metamagic){
        for(unsigned i=0;i<book["metamagic"].size() && i<30;i++)candidates[count++]=i;
    }else candidates[count++]=-1;
}
session::Change change(){return {op,count?candidates[index]:-1,op==session::Op::Metamagic?metaCost:0,restore};}
}
void open(uint32_t id,bool resting){character=id;rest=resting;stage=Stage::Menu;message="";}
uint32_t owner(){return character;}
void draw(const Sheet &sheet,const String &notice){
    auto &book=bookui::currentBook(sheet);auto b=casting::build(book);auto s=session::read(character,b);
    if(auto art=bookBackdrop()){auto *im=lv_img_create(lv_scr_act());lv_img_set_src(im,art);lv_obj_set_pos(im,0,0);}
    arcText(rest?ui::tr("Rest"):ui::tr("Magic resources"),ArcLabel::PlayTitle,gold);
    if(!casting::playable(book["schema"]|0u)){
        label(ui::tr("Refresh your sheet"),146,258,&lv_font_montserrat_20,ivory);label(notice,195,254,&lv_font_montserrat_14,teal);
    }else if(stage==Stage::Menu){
        if(!rest && b.spMax)label(String(session::points(b,s))+" / "+b.spMax+ui::tr(" Sorcery Points"),97,250,&lv_font_montserrat_20,gold);
        if(rest){
            dial::Capsule{180,143,186,46}.draw(ui::tr("Short rest"),ivory,0x283b40);dial::Capsule{180,209,186,46}.draw(ui::tr("Long rest"),ivory,0x283b40);
            session::Op prior;if(session::last(character,prior))label(ui::tr("Undo: ")+String(name(prior)),267,226,&lv_font_montserrat_14,gold);
        }else if(b.spMax){
            dial::Capsule{180,151,176,44}.draw(ui::tr("Slot to points"),ivory,0x283b40);
            dial::Capsule{180,201,176,44}.draw(ui::tr("Points to slot"),ivory,0x283b40);
            dial::Capsule{180,252,176,44}.draw(ui::tr("Metamagic"),ivory,0x283b40);
        }else{
            label(ui::tr("Spell slots   ")+String(sumSlots(b,s,0,9)),162,248,&lv_font_montserrat_20,ivory);
            label(ui::tr("Pact slots   ")+String(sumSlots(b,s,9,18)),210,248,&lv_font_montserrat_20,ivory);
        }
    }else if(stage==Stage::Receipt){
        label(name(op),124,248,&lv_font_montserrat_28,gold);label(message,166,260,&lv_font_montserrat_18,ivory);
        label(ui::tr("SP ")+String(session::points(b,s))+ui::tr("   •   Spell ")+String(sumSlots(b,s,0,9))+ui::tr("   •   Pact ")+String(sumSlots(b,s,9,18)),203,280,&lv_font_montserrat_14,teal);
        dial::Capsule{180,250,130,44}.draw(ui::tr("Undo"),ivory,0x283b40);
    }else{
        label(name(op),94,244,&lv_font_montserrat_20,gold);
        if(op==session::Op::Metamagic && count)metaCost=session::metamagicCost(book["metamagic"][candidates[index]]|"",b.modern,metaLevel);
        auto trial=s;const char *error=count?session::apply(b,trial,change()):ui::tr("No eligible options");
        if(rest){
            label(ui::tr("Spell slots   ")+String(sumSlots(b,s,0,9))+ui::tr(" to ")+sumSlots(b,trial,0,9),128,257,&lv_font_montserrat_18,ivory);
            label(ui::tr("Pact slots   ")+String(sumSlots(b,s,9,18))+ui::tr(" to ")+sumSlots(b,trial,9,18),153,257,&lv_font_montserrat_18,ivory);
            if(b.geniusMax || b.inkMax){
                String uses;
                if(b.spMax)uses=String(ui::tr("SP "))+session::points(b,s)+">"+session::points(b,trial)+"   ";
                if(b.geniusMax)uses+=String(ui::tr("Flash of Genius"))+" "+session::geniusLeft(b,s)+">"+session::geniusLeft(b,trial);
                if(b.inkMax){if(uses.length())uses+="   ";uses+=String(ui::tr("Mapping Magic"))+" "+session::inkLeft(b,s)+">"+session::inkLeft(b,trial);}
                dial::text(uses,178,273,&lv_font_montserrat_14,ivory,true);
            }else label(ui::tr("Sorcery Points   ")+String(session::points(b,s))+ui::tr(" to ")+session::points(b,trial),178,260,&lv_font_montserrat_18,ivory);
            String expiring;
            if(session::agathysLevel(s) && !session::agathysLevel(trial))expiring="Agathys";
            if(s.mageRounds && !trial.mageRounds)expiring+=expiring.length()?ui::tr(" / Mage Armor"):ui::tr("Mage Armor");
            if(s.shieldSpell)expiring+=expiring.length()?ui::tr(" / Shield"):ui::tr("Shield");
            String recovery;
            if(op==session::Op::LongRest && s.hp.maximum)recovery=String("HP  ")+s.hp.current+" > "+trial.hp.current;
            if(expiring.length()){if(recovery.length())recovery+="   •   ";recovery+=expiring+ui::tr(" ends");}
            if(s.temp && !trial.temp){if(recovery.length())recovery+="   •   ";recovery+=ui::tr("Temporary HP   ")+String(s.temp)+ui::tr(" to 0");}
            if(recovery.length())dial::text(recovery,199,266,&lv_font_montserrat_14,teal,true);
            if(op==session::Op::ShortRest && b.modern && b.sorcerer>=5 && !s.restored && s.spUsed)
                label(restore?ui::tr("Restoration on"):ui::tr("Use Sorcerous Restoration?"),215,250,&lv_font_montserrat_14,teal);
        }else if(count){
            int p=candidates[index];
            String option=op==session::Op::Metamagic?String(book["metamagic"][p]|""):pool(p);
            label((count>1?"<  ":"")+option+(count>1?"  >":""),148,270,&lv_font_montserrat_20,ivory);
            if(op==session::Op::Metamagic){
                bool scaled=option=="Twinned Spell" && !b.modern;
                label(scaled?ui::tr("< Level ")+String(metaLevel)+" • "+String(metaCost)+ui::tr(" SP >"):String(metaCost)+ui::tr(" SP"),182,248,&lv_font_montserrat_18,teal);
                if(option=="Distant Spell")label(ui::tr("Double range or touch to 30 ft"),209,254,&lv_font_montserrat_14,ivory);
                if(option=="Twinned Spell")label(b.modern?ui::tr("Upcast for an extra target"):ui::tr("Eligible single-target spell"),209,248,&lv_font_montserrat_14,ivory);
            }
            else{
                int before=session::points(b,s),after=session::points(b,trial);
                String balance=ui::tr("SP   ")+String(before);
                if(!error)balance+="   " LV_SYMBOL_RIGHT "   "+String(after);
                label(balance+" / "+b.spMax,180,250,&lv_font_montserrat_20,teal);
                String detail;
                if(op==session::Op::PointsToSlot)detail=ui::tr("Create one slot  •  ")+String(session::creationCost(p+1))+ui::tr(" SP");
                else if(!error && after-before<session::level(p))
                    detail=ui::tr("Room for ")+String(after-before)+ui::tr(" of ")+session::level(p)+ui::tr(" SP");
                else if(!error)detail=ui::tr("Spend one slot");
                else if(before==b.spMax)detail=ui::tr("Slot stays available");
                if(detail.length())label(detail,207,270,&lv_font_montserrat_14,ivory);
            }
        }
        if(message.length() || error)label(message.length()?message:String(ui::tr(error)),249,246,&lv_font_montserrat_14,teal);
        else dial::Capsule{180,249,158,44}.draw(rest?ui::tr("Finish rest"):ui::tr("Apply"),0x20252b,gold);
    }
    returnArc(stage==Stage::Menu?ui::tr("Back to sheet"):rest?ui::tr("Back to rests"):ui::tr("Back to magic"));
}
bool turn(int step){
    if(stage!=Stage::Preview || !count || !step)return false;
    index=((index+step)%count+count)%count;message="";return true;
}
bookui::Event touch(const Sheet &sheet,int x,int y,int dx,int dy){
    using E=bookui::Event;
    if((dx>=42 && abs(dx)>abs(dy)) || (abs(dx)<=18 && abs(dy)<=18 && dial::inReturn(x,y))){
        if(stage==Stage::Menu)return E::Back;
        stage=Stage::Menu;message="";return E::Paint;
    }
    if(abs(dy)>=42 && abs(dy)>=abs(dx))return turn(dy<0?1:-1)?E::Paint:E::None;
    if(abs(dx)>18 || abs(dy)>18)return E::None;
    auto &book=bookui::currentBook(sheet);
    if(!casting::playable(book["schema"]|0u))return E::Refresh;
    auto b=casting::build(book);
    if(stage==Stage::Menu){
        if(rest && y>=247 && y<=288){if(session::undo(character))pulse();}
        else if(rest && y>=120 && y<=232)preview(book,y<177?session::Op::ShortRest:session::Op::LongRest);
        else if(!rest && b.spMax && y>=124 && y<=277)preview(book,y<177?session::Op::SlotToPoints:y<226?session::Op::PointsToSlot:session::Op::Metamagic);
    }else if(stage==Stage::Receipt && y>=223 && y<=276){
        if(session::undo(character)){message="";stage=Stage::Menu;pulse();}
    }else if(stage==Stage::Preview){
        if(!rest && y>=120 && y<=171 && count)turn(x<180?-1:1);
        else if(op==session::Op::Metamagic && y>=172 && y<=210)metaLevel=std::clamp(metaLevel+(x<180?-1:1),1,9);
        else if(op==session::Op::ShortRest && y>=199 && y<=229)restore=!restore;
        else if(count && y>=228 && y<=274){
            if(op==session::Op::Metamagic)metaCost=session::metamagicCost(book["metamagic"][candidates[index]]|"",b.modern,metaLevel);
            const char *err=session::commit(character,b,change());
            if(err)message=ui::tr(err);else{stage=Stage::Receipt;message=ui::tr("Saved on dial");pulse();}
        }
    }
    return E::Paint;
}
void describe(const Sheet &sheet){
    auto &book=bookui::currentBook(sheet);auto b=casting::build(book);auto s=session::read(character,b);
    Serial.printf("RESOURCE id=%u rest=%d stage=%d sp=%d regular=%d pact=%d created=%d\n",character,rest,int(stage),session::points(b,s),sumSlots(b,s,0,9),sumSlots(b,s,9,18),s.created[0]);
}
}
