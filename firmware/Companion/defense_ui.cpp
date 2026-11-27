#include "ui_language.h"
#include "book_ui.h"
#include "defense_ui.h"
#include "round_controls.h"
#include "session_store.h"
#include "board.h"
namespace defenseui {
namespace {
enum class Face { Overview, Equipment, Hit, Time, Temp, Receipt, Error };
Face face;
int amount,index;
bool melee,critical;
String message;
constexpr uint32_t ivory=0xf7edda,gold=0xf5d390,teal=0xa8e3da,night=0x283b40;
constexpr dial::Capsule left{115,246,118,44},right{245,246,118,44},apply{180,252,158,44};
constexpr dial::Capsule sheetBonus{180,206,192,44};
constexpr int rounds[]={1,10,100,600,4800};
const char *times[]={"My next turn","1 minute","10 minutes","1 hour","8 hours"};
String ac(int value){return value?String(value):String("?");}
String duration(int r){return r>=10?String((r+9)/10)+" min":String(r)+ui::tr(" rounds");}
}
void open(){face=Face::Overview;message="";}
bool overview(){return face==Face::Overview;}
bool back(){if(face==Face::Overview)return false;open();return true;}
bool healthTouched(uint32_t id,int x,int y){
    const auto hp=session::health(id);
    if(face==Face::Overview && left.contains(x,y))return !hp.maximum;
    return face==Face::Receipt && right.contains(x,y) && hp.maximum && !hp.current;
}
bool castTouched(int x,int y){return face==Face::Equipment && apply.contains(x,y);}
void draw(JsonDocument &book,uint32_t id,int armorSpell){
    using dial::text;
    auto b=casting::build(book);auto s=session::read(id,b);auto g=book["gear"];
    switch(face){
    case Face::Overview:
        text(ui::tr("Armor Class"),85,232,&lv_font_montserrat_18,teal);
        text(ac(session::armorClass(b,s)),121,208,&lv_font_montserrat_36,ivory);
        text(s.mageRounds && !b.armored?ui::tr("Mage Armor  •  ")+duration(s.mageRounds):String(g["armor"]|ui::tr("Unarmored")),152,258,&lv_font_montserrat_14,teal,true);
        text(String(s.temp)+ui::tr(" temporary HP"),185,268,&lv_font_montserrat_20,ivory);
        text(session::agathysLevel(s)?String(session::retaliation(s,true))+ui::tr(" cold on melee hit"):s.shieldSpell?ui::tr("Shield until next turn"):"",210,245,&lv_font_montserrat_14,teal);
        left.draw(ui::tr(s.hp.maximum?"Take hit":"Set maximum"),ivory,night);right.draw(ui::tr("Pass time"),ivory,night);break;
    case Face::Equipment:
        text(ui::tr("Armor Class ")+ac(session::armorClass(b,s)),86,248,&lv_font_montserrat_28,ivory);
        text(g["armor"]|ui::tr("Unarmored"),118,276,&lv_font_montserrat_18,teal,true);
        text(g["shield"].is<const char *>()?String(g["shield"].as<const char *>())+"  +"+(g["shieldBonus"]|0):ui::tr("No shield equipped"),142,276,&lv_font_montserrat_14,ivory,true);
        if(armorSpell>=0){
            text(s.mageRounds?ui::tr("Mage Armor  •  ")+duration(s.mageRounds):String(ui::tr("Armor of Shadows  •  at will")),168,270,&lv_font_montserrat_14,teal);
            apply.draw(s.mageRounds?ui::tr("Renew ward"):ui::tr("Mage Armor"),0x20252b,gold);
        }else text(s.mageRounds && !b.armored?ui::tr("Mage Armor active"):ui::tr("Equipment AC  ")+ac(b.baseAC),168,258,&lv_font_montserrat_18,ivory);
        if(b.miscAC){
            String title=ui::tr("Sheet ")+String(b.miscAC>0?"+":"")+b.miscAC+(s.useMisc?ui::tr("  •  on"):ui::tr("  •  off"));
            sheetBonus.draw(title,ivory,night);
        }else if(g["customAC"].is<int>())text(ui::tr("Custom sheet AC   ")+String(g["customAC"].as<int>()),214,256,&lv_font_montserrat_14,gold);
        break;
    case Face::Time:
        text(ui::tr("Advance game time"),104,260,&lv_font_montserrat_20,ivory);
        text(String(LV_SYMBOL_LEFT "   ")+ui::tr(times[index])+"   " LV_SYMBOL_RIGHT,163,279,&lv_font_montserrat_20,gold);
        text(ui::tr("Counts down active spells"),201,260,&lv_font_montserrat_14,teal);
        apply.draw(ui::tr("Advance"),0x20252b,gold);break;
    case Face::Hit:
    case Face::Temp:{
        bool hit=face==Face::Hit;
        text(hit?ui::tr("Incoming damage"):ui::tr("Set temporary HP"),84,250,&lv_font_montserrat_20,ivory);
        text(String(LV_SYMBOL_MINUS "    ")+amount+"    " LV_SYMBOL_PLUS,128,270,&lv_font_montserrat_28,gold);
        if(hit){
            dial::Capsule{115,171,120,44}.draw(ui::tr(melee?"Melee hit":"Other"),ivory,night);
            dial::Capsule{245,171,120,44}.draw(ui::tr(critical?"Critical hit":"Normal hit"),ivory,night);
            text(ui::tr("Temp HP ")+String(s.temp)+" " LV_SYMBOL_RIGHT " "+std::max(0,int(s.temp)-amount)+"   •   "+std::max(0,amount-int(s.temp))+ui::tr(" to HP"),206,284,&lv_font_montserrat_14,ivory);
            if(int cold=session::retaliation(s,melee))text(String(cold)+ui::tr(" cold to attacker"),226,255,&lv_font_montserrat_14,teal);
        }else text(ui::tr("Replaces ")+String(s.temp)+ui::tr(" temporary HP"),181,270,&lv_font_montserrat_18,teal);
        dial::Capsule{180,260,158,44}.draw(hit?ui::tr("Apply hit"):ui::tr("Set temp HP"),0x20252b,gold);break;
    }
    case Face::Error:
        text(message,164,255,&lv_font_montserrat_18,ivory);break;
    case Face::Receipt:
        text(ui::tr("Recorded"),106,255,&lv_font_montserrat_28,ivory);
        text(message,161,270,&lv_font_montserrat_18,teal);
        if(s.hp.maximum)text(String(s.hp.current)+" / "+s.hp.maximum+" HP   •   "+s.temp+ui::tr(" temp HP"),202,265,&lv_font_montserrat_18,ivory);
        else text(String(s.temp)+ui::tr(" temporary HP"),202,252,&lv_font_montserrat_18,ivory);
        left.draw(ui::tr("Undo"),ivory,night);right.draw(ui::tr(s.hp.maximum && !s.hp.current?"Health":"Done"),ivory,night);break;
    }
}
bool turn(int step){
    if(face==Face::Hit || face==Face::Temp)amount=std::clamp(amount+step,0,1000);
    else if(face==Face::Time)index=std::clamp(index+step,0,4);
    else return false;
    return true;
}
bool tap(JsonDocument &book,uint32_t id,int x,int y){
    if(x<45 || x>315)return false;
    auto b=casting::build(book);auto s=session::read(id,b);
    if(face==Face::Overview){
        if(left.contains(x,y)){face=Face::Hit;amount=1;melee=true;critical=false;}
        else if(right.contains(x,y)){face=Face::Time;index=0;}
        else if(y>=66 && y<=160)face=Face::Equipment;
        else if(y>=164 && y<=218){face=Face::Temp;amount=s.temp;}
        else return false;
        return true;
    }
    if(face==Face::Error){open();return true;}
    if(face==Face::Receipt){
        if(right.contains(x,y))open();
        else if(left.contains(x,y)){if(session::undo(id))open();else{face=Face::Error;message=ui::tr("Could not undo");}}
        return true;
    }
    if((face==Face::Hit || face==Face::Temp) && y>=104 && y<151)return turn(x<180?-1:1);
    if(face==Face::Time && y>=138 && y<=186)return turn(x<180?-1:1);
    if(face==Face::Hit && dial::Capsule{115,171,120,44}.contains(x,y)){melee=!melee;return true;}
    if(face==Face::Hit && dial::Capsule{245,171,120,44}.contains(x,y)){critical=!critical;return true;}
    auto confirm=face==Face::Equipment?sheetBonus:(face==Face::Hit || face==Face::Temp)?dial::Capsule{180,260,158,44}:apply;
    if(!confirm.contains(x,y))return false;
    session::Change change{session::Op::TakeHit};
    if(face==Face::Time){change.op=session::Op::Elapse;change.amount=rounds[index];message=ui::tr(times[index]);}
    else if(face==Face::Equipment){if(!b.miscAC)return false;change.op=session::Op::UseMisc;message=ui::tr("AC adjustment updated");}
    else if(face==Face::Temp){change.op=session::Op::GrantTemp;change.amount=amount;message=ui::tr("Temporary HP updated");}
    else{change.amount=amount;change.melee=melee;change.critical=critical;message=String(session::retaliation(s,melee))+ui::tr(" cold   •   ")+std::max(0,amount-int(s.temp))+ui::tr(" to normal HP");}
    const char *err=session::commit(id,b,change);
    if(err){message=ui::tr(err);face=Face::Error;return true;}
    pulse();face=Face::Receipt;return true;
}
}
