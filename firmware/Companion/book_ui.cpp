#include "ui_language.h"
#include "book_ui.h"
#include "round_controls.h"
#include "defense_ui.h"
#include "portraits.h"
#include "rim_text.h"
#include "play.h"
#include "spell_rules.h"
#include "profile_state.h"
#include "board.h"
#include <algorithm>

namespace bookui {
namespace {
RollJsonMemory memory;
JsonDocument book(&memory);
uint32_t character,loadedId,loadedHash;
bool actions,detail,available;
int page,selected,detailPage,slot;
int armorSpell=-1,defenseEntry=-1;
equipment::WeaponBond weaponBond;
uint32_t pendingBond{};
bool bondWriteFailed{};
enum class Cast { None, Cost, Resolve };
Cast cast;
int poolChoices[20],choiceCount,choice,shot,effectIndex;
using casting::Meta;
Meta metamagic=Meta::None;
session::TempChoice tempChoice=session::TempChoice::Unspecified;
bool critical,twoHand,castSelf,geniusSpentHere,castRecorded;
String castMessage;
moves::Roll attack,damage;
String damageName;
constexpr uint32_t ivory=0xf7edda,gold=0xf5d390,teal=0xa8e3da;
JsonArrayConst entries(){return book[actions?"actions":"spells"].as<JsonArrayConst>();}
int pages(){return std::max(1,(int(entries().size())+2)/3);}
String sign(int n){return n>=0?"+"+String(n):String(n);}
String formula(moves::Roll r){return String(r.count)+"d"+r.sides+(r.bonus?" "+sign(r.bonus):"");}
String poolName(int p){return p<0?ui::tr("No slot"):String(p>=9?ui::tr("Pact L"):ui::tr("Spell L"))+session::level(p);}
bool slotFree(JsonObjectConst r){return r["lv"]==0 || r["ritualOnly"]==true || r["usesSlot"]!=true;}
void costChoices(JsonObjectConst r){
    choice=choiceCount=0;castMessage="";
    metamagic=Meta::None;castSelf=true;tempChoice=session::TempChoice::Unspecified;
    auto b=casting::build(book);int base=r["lv"]|0;
    if(!casting::playable(book["schema"]|0u)){castMessage=ui::tr("Refresh this sheet");return;}
    if(r["prepared"]==false || r["costReview"]==true){castMessage=ui::tr("Check preparation or use on sheet");return;}
    if(slotFree(r)){poolChoices[choiceCount++]=-1;return;}
    if(!b.known){castMessage=ui::tr("Slot progression needs review");return;}
    auto s=session::read(character,b);
    for(int i=0;i<18;i++)if(session::level(i)>=base && session::remaining(b,s,i))poolChoices[choiceCount++]=i;
    if(!choiceCount)castMessage=ui::tr("No eligible slots remaining");
}
String activation(JsonObjectConst r){
    const char *names[]={"",ui::tr("Action"),ui::tr("No action"),ui::tr("Bonus action"),ui::tr("Reaction"),"","min",ui::tr("hr"),ui::tr("Special")};
    int k=r["time"]|0;
    if(k<1 || k>8)return "";
    if(k==6 || k==7)return String(r["timeN"]|1)+" "+names[k];
    return names[k];
}
using dial::text;
using dial::Capsule;
constexpr Capsule confirmCast{180,263,128,44};
constexpr Capsule keepTemp{111,192,124,46},takeTemp{249,192,124,46};
constexpr Capsule chooseHand{113,125,118,44},confirmBond{180,245,174,44};
Capsule bondButton(JsonObjectConst row){return {row["twoHand"].is<const char *>()?245:180,125,132,44};}
const char *bondLabel(JsonObjectConst row){
    if(weaponBond.owns(row))return ui::tr("Bonded");
    return weaponBond.item?ui::tr("Unbonded"):ui::tr("Choose bond");
}
struct MetaChoices{
    Meta options[2];int count=0;
    MetaChoices(JsonObjectConst r,int level){
        for(Meta opt:{Meta::Distant,Meta::Twinned})
            if(casting::offersMeta(opt,r,book.as<JsonObjectConst>(),level))options[count++]=opt;
    }
    Capsule button(int i) const{return {count==1?180:(i?247:113),194,122,44};}
};
void drawSlots(int y,int minLevel){
    if(!casting::playable(book["schema"]|0u)){text(ui::tr("Refresh sheet for slots"),y,234,&lv_font_montserrat_14,teal);return;}
    auto b=casting::build(book);
    if(!b.known){text(ui::tr("Check sheet for slots"),y,234,&lv_font_montserrat_14,teal);return;}
    auto s=session::read(character,b);
    auto regular=session::slotCount(b,s,false,minLevel),pact=session::slotCount(b,s,true,minLevel);
    bool showRegular=regular.used+regular.left>0,showPact=pact.used+pact.left>0;
    if(!showRegular && !showPact){text(ui::tr("No eligible slots"),y,220,&lv_font_montserrat_14,gold);return;}
    for(bool isPact:{false,true}){
        auto count=isPact?pact:regular;int total=count.left+count.used;
        if(!total)continue;
        int x=showRegular && showPact?(isPact?238:122):180;
        auto color=lv_color_hex(isPact?0xd9b8fa:teal);
        String title=String(isPact?ui::tr("Pact "):ui::tr("Spell "))+count.left+"/"+total;
        auto *l=lv_label_create(lv_scr_act());lv_label_set_text(l,title.c_str());
        lv_obj_set_style_text_font(l,&lv_font_montserrat_14,0);lv_obj_set_style_text_color(l,color,0);
        lv_obj_align(l,LV_ALIGN_CENTER,x-180,y-180-4);
        // Small pools have individual pips. Larger pools keep the exact fraction.
        if(total>5)continue;
        for(int i=0;i<total;i++){
            auto *dot=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(dot);
            lv_obj_set_size(dot,7,7);lv_obj_set_pos(dot,x-(total-1)*6+i*12-3,y+6);
            lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,0);
            lv_obj_set_style_border_width(dot,1,0);lv_obj_set_style_border_color(dot,color,0);
            lv_obj_set_style_bg_color(dot,lv_color_hex(i<count.left?(isPact?0xd9b8fa:teal):0x18212c),0);
            lv_obj_set_style_bg_opa(dot,LV_OPA_COVER,0);
        }
    }
}
session::Change castChange(JsonObjectConst r,const session::Build &b,int p){
    auto change=casting::spellCast(r,metamagic,b.modern,p,castSelf);
    change.tempChoice=tempChoice;
    return change;
}
void leaveCast(){
    cast=Cast::None;critical=false;
    if(defenseEntry>=0){actions=true;selected=defenseEntry;defenseEntry=-1;}
    slot=std::max(1,entries()[selected]["lv"]|0);
}
bool freeCast(session::Change c){return !c.mappingMagic && c.pool<0 && !c.points && c.ward==session::Ward::None;}
String wardEffect(session::Change change,const session::Build &b,const session::State &s,const session::State &trial){
    if((change.ward==session::Ward::MageArmor || change.ward==session::Ward::Shield) && b.baseAC)
        return ui::tr("AC ")+String(session::armorClass(b,s))+" " LV_SYMBOL_RIGHT " "+session::armorClass(b,trial);
    return change.ward==session::Ward::MageArmor?ui::tr("Base AC 13 + DEX"):"";
}
void drawCost(JsonObjectConst r){
    String name=r["name"]|"?";
    text(name,74,244,name.length()>18?&lv_font_montserrat_18:&lv_font_montserrat_20,ivory,true);
    auto b=casting::build(book);auto s=session::read(character,b);
    if(!choiceCount){
        text(castMessage,166,255,&lv_font_montserrat_18,ivory);
        if(b.spMax)text(ui::tr("Sorcery Points   ")+String(session::points(b,s))+" / "+b.spMax,210,246,&lv_font_montserrat_18,teal);
        return;
    }
    int p=poolChoices[choice],level=casting::castLevel(r,p);
    String pool=poolName(p);
    if(p<0)pool=ui::tr(r["mappingMagic"]==true?"Mapping Magic":r["ritualOnly"]==true?"Ritual":"At will");
    if(choiceCount>1)pool=LV_SYMBOL_LEFT "    "+pool+"    " LV_SYMBOL_RIGHT;
    text(pool,104,260,&lv_font_montserrat_20,gold);
    String stock=p<0?String(ui::tr("No slot needed")):String(session::remaining(b,s,p))+ui::tr(" left   •   ")+std::min(s.spent[p],b.limit[p])+ui::tr(" used");
    if(r["mappingMagic"]==true){
        int left=session::inkLeft(b,s);
        stock=String(left)+" / "+b.inkMax+"   "+ui::tr("uses")+"   "+LV_SYMBOL_RIGHT+"   "+std::max(0,left-1);
    }
    if(r["ward"]==int(session::Ward::MageArmor))stock=r["selfOnly"]==true?ui::tr("Armor of Shadows  •  self"):(castSelf?ui::tr("Target: self  •  tap to switch"):ui::tr("Target: ally  •  tap to switch"));
    text(stock,128,246,&lv_font_montserrat_14,ivory);
    auto trial=s;
    auto change=castChange(r,b,p);
    const char *error=session::apply(b,trial,change);
    String ward=wardEffect(change,b,s,trial);
    if(change.ward==session::Ward::Agathys){
        int offer=session::agathysOffer(s,level);
        text(String(5*std::max(level,session::agathysLevel(s)))+ui::tr(" cold on a melee hit"),154,266,&lv_font_montserrat_18,teal);
        if(session::needsTempChoice(s,offer)){
            bool keep=tempChoice==session::TempChoice::Keep,take=tempChoice==session::TempChoice::Take;
            keepTemp.draw(ui::tr("Keep ")+String(s.temp),keep?0x13262a:ivory,keep?teal:0x283b40,ui::tr("temp HP"));
            takeTemp.draw(ui::tr("Take ")+String(offer),take?0x13262a:ivory,take?teal:0x283b40,ui::tr("temp HP"));
            if(!error && !castMessage.length())text(String(trial.temp)+ui::tr(" temp HP after cast"),230,265,&lv_font_montserrat_14,ivory);
        }else if(!offer){
            text(ui::tr("Level ")+String(session::agathysLevel(s))+ui::tr(" stays active"),189,255,&lv_font_montserrat_18,ivory);
            text(ui::tr("Level ")+String(level)+ui::tr(" can follow • same HP"),216,273,&lv_font_montserrat_14,teal);
        }else text(String(trial.temp)+ui::tr(" temporary HP"),193,256,&lv_font_montserrat_20,ivory);
    }else if(b.spMax){
        int points=session::points(b,s);
        String balance=ui::tr("Sorcery Points   ")+String(points);
        if(metamagic!=Meta::None && !error)balance+=" " LV_SYMBOL_RIGHT " "+String(session::points(b,trial))+ui::tr(" left");
        else balance+=" / "+String(b.spMax);
        text(balance,155,292,&lv_font_montserrat_18,teal);
        MetaChoices choices(r,level);
        for(int i=0;i<choices.count;i++){
            Meta opt=choices.options[i];bool selected=metamagic==opt;
            auto cost=casting::castCost(opt,b.modern,p,level);
            String title=selected?LV_SYMBOL_OK " ":"";title+=opt==Meta::Distant?ui::tr("Distant"):ui::tr("Twinned");
            String price=String(cost.points)+ui::tr(" SP");
            choices.button(i).draw(title,selected?0x13262a:points>=cost.points?ivory:0xb3bdc3,
                                  selected?teal:0x283b40,price.c_str());
        }
        String effect;
        if(metamagic==Meta::Distant){
            String from=r["range"]=="Touch"?String(ui::tr("Touch")):String(r["ft"]|0)+ui::tr(" ft");
            effect=from+"  " LV_SYMBOL_RIGHT "  "+casting::distantRange(r)+ui::tr(" ft");
        }else if(metamagic==Meta::Twinned)effect=ui::tr("+1 target   •   casts as L")+String(level+1);
        else if(ward.length())effect=ward;
        else if(choices.count)effect=points?ui::tr("Tap an option to preview"):ui::tr("Needs 1 Sorcery Point");
        else effect=ui::tr("No matching Metamagic");
        if(!castMessage.length() && !error)text(effect,choices.count?232:199,272,ward.length() && !choices.count?&lv_font_montserrat_20:&lv_font_montserrat_14,ivory,true);
    }else{
        String range=r["range"]|"";
        if((r["ft"]|0)>0)range=String(r["ft"]|0)+ui::tr(" ft");
        text(activation(r),180,246,&lv_font_montserrat_20,ivory);
        text(ward.length()?ward:range,207,265,&lv_font_montserrat_18,teal);
    }
    if(castMessage.length() || error)text(castMessage.length()?castMessage:String(ui::tr(error)),232,268,&lv_font_montserrat_14,ivory);
    confirmCast.draw(ui::tr("Cast"),error?0xb3bdc3:0x24251e,error?0x334048:gold);
}
String origin(JsonObjectConst r){
    String s=ui::tr(r["src"]|"");
    if(r["lv"].is<int>())s+=r["lv"].as<int>()?ui::tr("  •  Level ")+String(r["lv"].as<int>()):ui::tr("  •  Cantrip");
    return s;
}
String detailLines[48];
int lineCount;
void line(String s){if(s.length() && lineCount<48)detailLines[lineCount++]=s;}
void brief(const char *words){
    String part;
    while(*words){
        while(*words==' ')++words;
        const char *end=words;
        while(*end && *end!=' ')++end;
        String next;next.concat(words,end-words);
        String trial=part.length()?part+" "+next:next;
        if(part.length() && lv_txt_get_width(trial.c_str(),trial.length(),&lv_font_montserrat_14,0,LV_TEXT_FLAG_NONE)>242){
            line(part);part=next;
        }else part=trial;
        words=end;
    }
    line(part);
}
void describeEntry(const Sheet &s,JsonObjectConst r){
    lineCount=0;
    attack={};damage={};damageName=ui::tr("Base dice");
    String time=activation(r);int ft=r["ft"]|0;
    String range=ft>0?String(ft)+ui::tr(" ft"):String(r["range"]|"");
    line(time+(time.length() && range.length()?"  •  ":"")+range);
    if(r["con"]==true)line(ui::tr("Concentration"));
    if(r["protectedFire"]==true)line(ui::tr("Damage cannot break this focus"));
    if(r["ritualOnly"]==true)line(ui::tr("Ritual only"));else if(r["ritual"]==true)line(ui::tr("Ritual option"));
    if(r["prepared"].is<bool>())line(r["prepared"]==true?ui::tr("Prepared"):ui::tr("Unprepared"));
    int a=r["ability"]|0;
    bool known=a>=1 && a<=6 && s.rolls.scores[a-1];
    int base=known?dnd::proficiencyBonus(s.level)+dnd::abilityModifier(s.rolls.scores[a-1]):0;
    if(r["hit"]==true)line(known && r["atkReview"]!=true?ui::tr("Spell attack  ")+sign(base+(r["atkBonus"]|0)):ui::tr("Check spell attack"));
    auto roll=weaponBond.roll(r);
    if(roll["toHit"].is<int>())line(String(ui::tr("Attack  "))+sign(roll["toHit"].as<int>()));
    if(r["hit"]==true && known && r["atkReview"]!=true)attack={1,20,base+(r["atkBonus"]|0),true};
    if(roll["toHit"].is<int>())attack={1,20,roll["toHit"].as<int>(),true};
    int save=r["save"]|0;
    if(save>=1 && save<=6){
        String phrase=String(ui::tr(checks::shortNames[save-1]))+ui::tr(" save");
        if(r["dc"].is<int>())phrase+=ui::tr("  •  DC ")+String(r["dc"].as<int>());
        else if(known && r["dcReview"]!=true)phrase+=ui::tr("  •  DC ")+String(8+base+(r["dcBonus"]|0));
        line(phrase);
    }
    if(r["beams"].is<int>()){
        int beams=r["beams"];line(String(beams)+(beams==1?ui::tr(" beam"):ui::tr(" beams"))+ui::tr("  •  roll each attack"));
    }
    if(r["missile"]==true)line(String(slot+2)+ui::tr(" darts  •  automatic hits"));
    int idx=0;
    for(auto fx:r["effects"].as<JsonArrayConst>()){
        auto dice=casting::effect(r,s.rolls,idx++,slot);
        String value=dice.valid()?formula(dice):fx[4].is<int>()?String(casting::fixedEffect(fx,r["lv"]|0,slot)):String(ui::tr("Check sheet"));
        line(value+" "+(fx[1]|"")+(r["missile"]==true?ui::tr(" / dart"):r["beams"].is<int>()?ui::tr(" / beam"):""));
    }

    if(r["effects"].size())damage=casting::effect(r,s.rolls,effectIndex,slot,critical && attack.valid());
    if(r["die"].is<const char *>()){
        damage=moves::readDice(twoHand && r["twoHand"].is<const char *>()?r["twoHand"]:r["die"]);
        if(r["weapon"]==true){
            damageName=ui::tr("Weapon damage");damage.weaponDamage=true;damage.bonus+=roll["weaponBonus"]|0;
            if(roll.isNull()){damage={};line(ui::tr("Refresh sheet for weapon rolls"));}
            else if(roll["weaponReview"]==true){damage={};line(ui::tr("Check weapon roll on sheet"));}
            else{
                line(formula(damage)+" "+(r["type"]|""));
                line(String(ui::tr(roll["stat"]|""))+"   •   "+(roll["proficient"]==true?ui::tr("Proficient"):ui::tr("Untrained")));
            }
            if(weaponBond.owns(r))line(ui::tr("Pact weapon"));
            if(roll["hex"]==true)line(ui::tr("Hex weapon"));
        }else line(ui::tr("Base  ")+String(r["die"].as<const char *>())+" "+(r["type"]|""));
    }
    if(r["missile"]==true)damageName=ui::tr("One dart");
    if(r["beams"].is<int>())damageName=ui::tr("One beam");
    if(!lineCount)line(origin(r));
    if(r["precisionDamage"]==true || (book["guidedPrecision"]==true && attack.valid())){
        line(ui::tr("Guided Precision"));
        line(ui::tr("Once per turn: INT to one damage roll"));
        if(r["precisionDamage"]!=true)line(ui::tr("On a hit vs. your Faerie Fire target"));
    }
    if(r["brief"].is<const char *>())brief(r["brief"]);
    detailPage=std::clamp(detailPage,0,(lineCount-1)/4);
}
void load(const Sheet &sheet){
    if(loadedId!=sheet.id || loadedHash!=sheet.bookHash || !available){
        book.clear();available=readMoveBook(sheet.id,sheet.bookBank,sheet.bookHash,book);
        pendingBond=0;
        weaponBond=available?reconcilePlayState(sheet.id,book):equipment::WeaponBond{};
        armorSpell=available && casting::playable(book["schema"]|0u)?casting::atWillMageArmor(book.as<JsonObjectConst>()):-1;
        loadedId=sheet.id;loadedHash=sheet.bookHash;
    }
}
}
void open(const Sheet &sheet,bool actionList){
    defenseEntry=-1;pendingBond=0;
    character=sheet.id;actions=actionList;page=selected=detailPage=effectIndex=0;detail=false;slot=1;cast=Cast::None;critical=false;
    // The library may have removed and re-added an identical cached sheet.
    if(actionList && available && loadedId==sheet.id && loadedHash==sheet.bookHash)
        weaponBond=equipment::resolveBond(book.as<JsonObjectConst>(),session::bondChoice(sheet.id));
    load(sheet);
}
uint32_t owner(){return character;}
JsonDocument &currentBook(const Sheet &s){load(s);return book;}
void draw(const Sheet &sheet,const String &status){
    load(sheet);
    auto art=bookBackdrop();if(art){auto *im=lv_img_create(lv_scr_act());lv_img_set_src(im,art);lv_obj_set_pos(im,0,0);}
    bool defenses=actions && detail && entries()[selected]["defense"]==true;
    arcText(pendingBond?ui::tr("Weapon bond"):defenses?ui::tr("Defenses"):actions?ui::tr("Actions"):cast==Cast::Cost?ui::tr("Cast preview"):ui::tr("Spellbook"),ArcLabel::PlayTitle,gold);
    if(!available){
        text(ui::tr("Load from sheet"),145,270,&lv_font_montserrat_28,ivory);
        text(status,186,264,&lv_font_montserrat_14,teal);
        text(ui::tr("Tap to refresh"),224,210,&lv_font_montserrat_18,gold);
    }else if(pendingBond){
        auto row=entries()[selected].as<JsonObjectConst>();
        bool clear=weaponBond.owns(row);
        text(row["name"]|ui::tr("Weapon"),99,254,&lv_font_montserrat_20,ivory,true);
        text(clear?ui::tr("Currently bonded"):weaponBond.item?ui::tr("Move the bond here?"):ui::tr("Bond this weapon?"),151,276,&lv_font_montserrat_20,gold);
        text(clear?ui::tr("Clearing reopens the choice"):ui::tr("Only one weapon can be bonded"),185,278,&lv_font_montserrat_14,teal);
        if(bondWriteFailed)text(ui::tr("Could not save • tap to retry"),210,252,&lv_font_montserrat_14,gold);
        confirmBond.draw(clear?ui::tr("Clear choice"):weaponBond.item?ui::tr("Move bond"):ui::tr("Bond weapon"),0x20252b,gold);
    }else if(defenses){defenseui::draw(book,character,armorSpell);
    }else if(actions && detail && entries()[selected]["genius"]==true){
        auto b=casting::build(book);auto s=session::read(character,b);
        text(ui::tr("Flash of Genius"),92,266,&lv_font_montserrat_28,ivory);
        text(ui::tr("Failed check or save"),135,265,&lv_font_montserrat_18,teal);
        text(ui::tr("Self or visible creature within 30 ft"),162,270,&lv_font_montserrat_14,ivory);
        text(sign(b.geniusMax)+"   •   "+String(session::geniusLeft(b,s))+" / "+b.geniusMax+" "+ui::tr("uses"),194,265,&lv_font_montserrat_20,gold);
        if(castMessage.length())text(castMessage,217,250,&lv_font_montserrat_14,ivory);
        if(geniusSpentHere || session::geniusLeft(b,s))confirmCast.draw(ui::tr(geniusSpentHere?"Undo":"Use reaction"),0x20252b,gold);
    }else if(entries().size()==0)text(actions?ui::tr("No listed actions"):ui::tr("No listed spells"),166,260,&lv_font_montserrat_20,ivory);
    else if(!detail){
        page=std::clamp(page,0,pages()-1);
        if(!actions)drawSlots(64,1);
        else if(book["schema"]!=casting::version)text(ui::tr("Tap to refresh equipment"),64,230,&lv_font_montserrat_14,gold);
        for(int j=0;j<3;j++){
            int i=3*page+j;if(i>=int(entries().size()))break;
            auto row=entries()[i].as<JsonObjectConst>();String name=row["defense"]==true?ui::tr("Defenses"):(row["name"]|"?");
            text(name,96+j*66,j==0?239:286,name.length()>22?&lv_font_montserrat_14:&lv_font_montserrat_20,ivory,true);
            String sub=origin(row);
            if(weaponBond.owns(row))sub=ui::tr("Bonded weapon");
            else if(weaponBond.canChoose(row) && !weaponBond.item)sub=ui::tr("Multiple pact marks");
            if(row["defense"]==true){auto b=casting::build(book);auto s=session::read(character,b);int ac=session::armorClass(b,s);sub=ui::tr("AC ")+(ac?String(ac):String("?"))+"   •   "+s.temp+ui::tr(" temp HP");}
            text(sub,120+j*66,248,&lv_font_montserrat_14,teal,true);
        }
        text(String(LV_SYMBOL_UP "  ")+String(page+1)+" / "+String(pages())+"  " LV_SYMBOL_DOWN,269,158,&lv_font_montserrat_14,gold);
    }else if(cast==Cast::Cost){
        drawCost(entries()[selected].as<JsonObjectConst>());
    }else if(cast==Cast::Resolve){
        auto r=entries()[selected].as<JsonObjectConst>();describeEntry(sheet,r);
        text(r["name"]|"",88,250,&lv_font_montserrat_20,ivory);
        int count=casting::shots(r,slot);
        String detail=count>1?String(r["missile"]==true?ui::tr("Dart "):ui::tr("Beam "))+shot+" / "+count:poolName(poolChoices[choice]);
        if(metamagic==Meta::Distant)detail+="  •  "+String(casting::distantRange(r))+ui::tr(" ft");
        if(metamagic==Meta::Twinned)detail+=ui::tr("  •  +1 target");
        text(count>1?"<   "+detail+"   >":detail,120,242,&lv_font_montserrat_18,teal);
        auto fx=r["effects"][effectIndex];String type=fx[1]|"";
        int save=r["save"]|0;bool known;int ab=casting::spellAbility(r,sheet.rolls,known);
        if(save>=1 && save<=6){
            String prompt=String(ui::tr(checks::shortNames[save-1]))+ui::tr(" save");
            if(r["dc"].is<int>())prompt+=ui::tr("  DC ")+String(r["dc"].as<int>());
            else if(known && r["dcReview"]!=true)prompt+=ui::tr("  DC ")+String(8+dnd::proficiencyBonus(sheet.level)+ab+(r["dcBonus"]|0));
            text(prompt,150,248,&lv_font_montserrat_18,gold);
        }else text(attack.valid()?ui::tr("Spell attack ")+sign(attack.bonus):r["missile"]==true?ui::tr("Automatic hit"):ui::tr("Cast confirmed"),150,244,&lv_font_montserrat_18,gold);
        String effectText=damage.valid()?formula(damage)+" "+type:fx[4].is<int>()?String(casting::fixedEffect(fx,r["lv"]|0,slot))+" "+type:ui::tr("See spell details");
        if(r["ward"]==int(session::Ward::Agathys))effectText=ui::tr("Cold on a melee hit");
        else if(r["ward"]==int(session::Ward::MageArmor))effectText=ui::tr("8 game hours");
        else if(r["ward"]==int(session::Ward::Shield))effectText=ui::tr("Until your next turn");
        else if(r["effects"].size()>1)effectText="< "+effectText+" >";
        text(effectText,183,274,&lv_font_montserrat_18,ivory);
        if(r["ward"].as<int>()){
            auto b=casting::build(book);auto s=session::read(character,b);
            if(r["ward"]==int(session::Ward::Agathys))text(String(s.temp)+ui::tr(" temp HP   •   ")+session::retaliation(s,true)+ui::tr(" cold"),211,268,&lv_font_montserrat_14,teal);
            else text(!castSelf?ui::tr("Ally warded"):b.baseAC?ui::tr("Your AC   ")+String(session::armorClass(b,s)):ui::tr("Check AC on sheet"),211,246,&lv_font_montserrat_14,teal);
        }
        if(attack.valid())text(critical?ui::tr("Critical damage ON"):ui::tr("Tap for critical damage"),211,242,&lv_font_montserrat_14,teal);
        text(String(attack.valid()?ui::tr("Roll attack"):ui::tr("Undo cast"))+"          "+(damage.valid()?ui::tr("Roll dice"):ui::tr("Done")),249,260,&lv_font_montserrat_18,gold);
        if(attack.valid())text(ui::tr("Undo cast"),279,115,&lv_font_montserrat_14,teal);
    }else{
        auto row=entries()[selected].as<JsonObjectConst>();String name=row["defense"]==true?ui::tr("Defenses"):(row["name"]|"?");
        bool chooseBond=weaponBond.canChoose(row);
        text(name,chooseBond?83:actions?92:80,251,name.length()>17?&lv_font_montserrat_20:&lv_font_montserrat_28,ivory,true);
        String subtitle=row["twoHand"].is<const char *>()?(twoHand?ui::tr("Two hands  •  tap to switch"):ui::tr("One hand  •  tap to switch")):origin(row);
        if(chooseBond){
            if(row["twoHand"].is<const char *>())chooseHand.draw(twoHand?ui::tr("Two hands"):ui::tr("One hand"),ivory,0x283b40);
            bondButton(row).draw(bondLabel(row),weaponBond.owns(row)?0x13262a:ivory,weaponBond.owns(row)?teal:0x283b40);
        }else if(actions && pack::activation(row["time"]|0)!=pack::Timing::None && session::read(character,casting::build(book)).turn.tracking){
            auto t=pack::activation(row["time"]);auto state=session::read(character,casting::build(book));
            bool used=state.turn.used&pack::timingMask(t);
            Capsule{180,125,226,44}.draw(castMessage.length()?castMessage:String(ui::tr(used?"Spent":"Use"))+"  "+ui::tr(pack::timingName(t)),ivory,0x283b40);
        }else text(subtitle,actions?125:106,258,&lv_font_montserrat_14,teal,true);
        if(!actions){
            if(!casting::playable(book["schema"]|0u))text(ui::tr("Refresh sheet for slots"),131,258,&lv_font_montserrat_14,gold);
            else if(row["costReview"]==true)text(ui::tr("Check grant on sheet"),131,258,&lv_font_montserrat_14,gold);
            else if(slotFree(row))text(ui::tr("No slot needed"),131,258,&lv_font_montserrat_14,gold);
            else drawSlots(129,row["lv"]|1);
        }
        describeEntry(sheet,row);
        for(int j=0;j<4 && detailPage*4+j<lineCount;j++)
            text(detailLines[detailPage*4+j],154+j*23,266,&lv_font_montserrat_14,ivory,true);
        if(!actions || attack.valid() || damage.valid()){
            String left=!actions?ui::tr("Cast..."):attack.valid()?ui::tr("Roll attack"):"";
            auto chip=[&](String title,int cx){
                if(!title.length())return;
                auto *b=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(b);
                lv_obj_set_size(b,106,34);lv_obj_set_pos(b,cx-53,230);lv_obj_set_style_radius(b,17,0);
                lv_obj_set_style_bg_color(b,lv_color_hex(0x283b40),0);lv_obj_set_style_bg_opa(b,LV_OPA_90,0);
                auto *l=lv_label_create(b);lv_label_set_text(l,title.c_str());
                lv_obj_set_width(l,100);lv_label_set_long_mode(l,LV_LABEL_LONG_SCROLL_CIRCULAR);
                lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(l,lv_color_hex(gold),0);lv_obj_center(l);
            };
            chip(left,119);chip(damage.valid()?(row["weapon"]==true?ui::tr("Damage"):row["missile"]==true?ui::tr("Roll dart"):row["beams"].is<int>()?ui::tr("Roll beam"):ui::tr("Roll dice")):"",241);
        }
        if(lineCount>4)text(String(LV_SYMBOL_UP "  ")+String(detailPage+1)+" / "+String((lineCount+3)/4)+"  " LV_SYMBOL_DOWN,273,150,&lv_font_montserrat_14,teal);
    }
    returnArc(pendingBond?ui::tr("Back to weapon"):defenseEntry>=0?ui::tr("Equipment"):defenses && !defenseui::overview()?ui::tr("Back to defenses"):cast!=Cast::None?ui::tr("Back to spell"):detail?(actions?ui::tr("Back to actions"):ui::tr("Back to spells")):ui::tr("Back to sheet"));
}
bool turn(int steps){
    if(!available || !steps || pendingBond)return false;
    if(detail && entries()[selected]["defense"]==true)return defenseui::turn(steps);
    if(cast==Cast::Cost){
        if(!choiceCount)return false;
        int next=((choice+steps)%choiceCount+choiceCount)%choiceCount;
        if(next!=choice)tempChoice=session::TempChoice::Unspecified;
        choice=next;
        auto r=entries()[selected].as<JsonObjectConst>();int level=casting::castLevel(r,poolChoices[choice]);
        castMessage="";
        if(metamagic!=Meta::None && !casting::offersMeta(metamagic,r,book.as<JsonObjectConst>(),level)){
            metamagic=Meta::None;castMessage=ui::tr("Metamagic unavailable at this level");
        }
        return true;
    }
    if(cast==Cast::Resolve){int n=casting::shots(entries()[selected],slot);shot=std::clamp(shot+steps,1,n);critical=false;return true;}
    if(detail)detailPage=std::clamp(detailPage+steps,0,std::max(0,(lineCount-1)/4));
    else page=((page+steps)%pages()+pages())%pages();
    return true;
}
Event touch(const Sheet &sheet,int x,int y,int dx,int dy){
    if((dx>=42 && abs(dx)>abs(dy)) || (abs(dx)<=18 && abs(dy)<=18 && dial::inReturn(x,y))){
        if(pendingBond){pendingBond=0;return Event::Paint;}
        if(!detail)return Event::Back;
        if(entries()[selected]["defense"]==true && defenseui::back())return Event::Paint;
        if(cast!=Cast::None){leaveCast();return Event::Paint;}
        detail=false;return Event::Paint;
    }
    if(abs(dy)>=42 && abs(dy)>=abs(dx))return turn(dy<0?1:-1)?Event::Paint:Event::None;
    if(abs(dx)>18 || abs(dy)>18)return Event::None;
    if(!available)return y>110 && y<252?Event::Refresh:Event::None;
    if(pendingBond){
        auto row=entries()[selected].as<JsonObjectConst>();
        if(!confirmBond.contains(x,y))return Event::None;
        if(!weaponBond.canChoose(row) || row["itemId"].as<uint32_t>()!=pendingBond){pendingBond=0;return Event::Paint;}
        uint32_t choice=weaponBond.owns(row)?0:pendingBond;
        bondWriteFailed=!session::setBondChoice(character,choice);
        if(!bondWriteFailed){weaponBond=equipment::resolveBond(book.as<JsonObjectConst>(),choice);pendingBond=0;detailPage=0;pulse();}
        return Event::Paint;
    }
    if(actions && detail && entries()[selected]["genius"]==true){
        if(!confirmCast.contains(x,y))return Event::None;
        if(geniusSpentHere){
            if(session::undo(character)){geniusSpentHere=false;castMessage="";}
        }else{
            auto b=casting::build(book);
            auto error=session::commit(character,b,{session::Op::FlashOfGenius});
            geniusSpentHere=!error;castMessage=error?ui::tr(error):"";
        }
        return Event::Paint;
    }
    if(detail && entries()[selected]["defense"]==true){
        if(armorSpell>=0 && defenseui::castTouched(x,y)){
            defenseEntry=selected;actions=false;selected=armorSpell;
            costChoices(entries()[selected]);cast=Cast::Cost;
            return Event::Paint;
        }
        if(defenseui::healthTouched(character,x,y))return Event::Health;
        return defenseui::tap(book,character,x,y)?Event::Paint:Event::None;
    }
    if(!detail && (actions?book["schema"]!=casting::version:!casting::playable(book["schema"]|0u)) && y>=44 && y<76)return Event::Refresh;
    if(!detail && y>=75 && y<253){
        int i=page*3+(y-75)/66;
        if(i<int(entries().size())){selected=i;detail=true;geniusSpentHere=false;castMessage="";twoHand=false;defenseui::open();detailPage=effectIndex=0;slot=std::max(1,entries()[i]["lv"]|0);critical=false;return Event::Paint;}
    }
    if(cast==Cast::Cost){
        if(!choiceCount)return Event::None;
        auto chosen=entries()[selected];
        if(y>=117 && y<=144 && chosen["ward"]==int(session::Ward::MageArmor) && chosen["selfOnly"]!=true){castSelf=!castSelf;castMessage="";return Event::Paint;}
        if(y>=84 && y<=128 && choiceCount>1 && (x<158 || x>202))return turn(x<158?-1:1)?Event::Paint:Event::None;
        auto r=entries()[selected].as<JsonObjectConst>();
        int p=poolChoices[choice],level=casting::castLevel(r,p);
        auto b=casting::build(book);
        if(r["ward"]==int(session::Ward::Agathys) && (keepTemp.contains(x,y) || takeTemp.contains(x,y))){
            auto s=session::read(character,b);
            if(session::needsTempChoice(s,session::agathysOffer(s,level))){
                tempChoice=keepTemp.contains(x,y)?session::TempChoice::Keep:session::TempChoice::Take;
                castMessage="";pulse();return Event::Paint;
            }
        }
        MetaChoices choices(r,level);
        for(int i=0;b.spMax && i<choices.count;i++){
            if(!choices.button(i).contains(x,y))continue;
            Meta opt=choices.options[i];auto s=session::read(character,b);
            auto cost=casting::castCost(opt,b.modern,p,level);
            if(opt!=metamagic && cost.points>session::points(b,s))return Event::None;
            metamagic=opt==metamagic?Meta::None:opt;castMessage="";pulse();return Event::Paint;
        }
        if(confirmCast.contains(x,y)){
            auto change=castChange(r,b,p);
            castRecorded=!freeCast(change) || (change.timing!=pack::Timing::None && session::read(character,b).turn.tracking);
            const char *error=castRecorded?session::commit(character,b,change):nullptr;
            if(error)castMessage=ui::tr(error);
            else{slot=level+(metamagic==Meta::Twinned);cast=Cast::Resolve;shot=1;effectIndex=0;critical=false;pulse();}
        }
        return Event::Paint;
    }
    if(cast==Cast::Resolve){
        auto r=entries()[selected].as<JsonObjectConst>();describeEntry(sheet,r);
        if(y>=99 && y<=138){turn(x<180?-1:1);return Event::Paint;}
        if(y>=165 && y<=201 && !r["ward"].as<int>() && r["effects"].size()>1){int n=r["effects"].size();effectIndex=(effectIndex+(x<180?-1:1)+n)%n;return Event::Paint;}
        if(y>=202 && y<=225 && attack.valid()){critical=!critical;return Event::Paint;}
        bool undo=attack.valid()?y>=266 && y<=289:y>=227 && y<=267 && x<178;
        if(undo){session::Op op;if(!castRecorded || (session::last(character,op) && op==session::Op::Cast && session::undo(character)))leaveCast();return Event::Paint;}
        if(y>=227 && y<=267){
            auto roll=x<180?attack:damage;
            if(roll.valid()){play::openMove(character,r["name"]|"",x<180?ui::tr("Spell attack"):damageName,roll);return Event::Roll;}
            if(x>=180){leaveCast();return Event::Paint;}
        }
        return Event::None;
    }
    if(detail && actions && !weaponBond.canChoose(entries()[selected]) && Capsule{180,125,226,44}.contains(x,y)){
        auto t=pack::activation(entries()[selected]["time"]|0);auto b=casting::build(book);
        if(t!=pack::Timing::None && session::read(character,b).turn.tracking){
            session::Change c{session::Op::SpendTurn};c.timing=t;
            auto err=session::commit(character,b,c);castMessage=err?ui::tr(err):"";if(!err)pulse();
            return Event::Paint;
        }
    }
    if(detail && weaponBond.canChoose(entries()[selected])){
        auto row=entries()[selected].as<JsonObjectConst>();
        if(bondButton(row).contains(x,y)){pendingBond=row["itemId"].as<uint32_t>();bondWriteFailed=false;return Event::Paint;}
        if(row["twoHand"].is<const char *>() && chooseHand.contains(x,y)){twoHand=!twoHand;return Event::Paint;}
    }else if(detail && entries()[selected]["twoHand"].is<const char *>() && y>=104 && y<=146){twoHand=!twoHand;return Event::Paint;}
    if(detail && y>=225 && y<=269){
        auto row=entries()[selected].as<JsonObjectConst>();
        describeEntry(sheet,row);
        if(x>=62 && x<=176 && !actions){
            costChoices(row);cast=Cast::Cost;return Event::Paint;
        }
        if(x>=62 && x<=176 && attack.valid()){
            const char *kind=row["weapon"]==true?ui::tr("Weapon attack"):ui::tr("Base attack");
            play::openMove(character,row["name"]|"",kind,attack);return Event::Roll;
        }
        if(x>=184 && x<=298 && damage.valid()){
            play::openMove(character,row["name"]|"",damageName,damage);return Event::Roll;
        }
    }
    if(y>=254 && y<=288)return turn(x<180?-1:1)?Event::Paint:Event::None;
    return Event::None;
}
void describe(){
    const char *name=available && entries().size()?entries()[detail?selected:page*3]["name"]|"": "";
    Serial.printf("BOOK id=%u available=%d actions=%d entries=%u page=%d detail=%d slot=%d name=%s\n",character,available,actions,unsigned(entries().size()),page,detail,slot,name);
    if(actions && book["gear"].is<JsonObject>()){
        if(detail && entries()[selected]["weapon"]==true){
            auto row=entries()[selected].as<JsonObjectConst>();
            auto roll=weaponBond.roll(row);
            Serial.printf("BOND item=%u active=%u bonded=%d marked=%u preview=%u selectable=%d saved=%u stat=%s hit=%d damageBonus=%d\n",
                row["itemId"].as<uint32_t>(),weaponBond.item,weaponBond.owns(row),weaponBond.marked,pendingBond,
                weaponBond.canChoose(row),session::bondChoice(character),roll["stat"]|"?",roll["toHit"]|-99,roll["weaponBonus"]|-99);
        }
        auto b=casting::build(book);auto s=session::read(character,b);
        int frostLevel=session::agathysLevel(s);
        Serial.printf("DEFENSE ac=%d temp=%u frost=%u mage=%u cold=%u shield=%d\n",session::armorClass(b,s),s.temp,frostLevel?s.agathysRounds[frostLevel-1]:0,s.mageRounds,session::retaliation(s,true),s.shieldSpell);
    }
    if(cast!=Cast::None){
        int p=choiceCount?poolChoices[choice]:-2;
        auto r=entries()[selected].as<JsonObjectConst>();auto b=casting::build(book);auto s=session::read(character,b),trial=s;
        auto level=casting::castLevel(r,p);
        if(choiceCount && cast==Cast::Cost)session::apply(b,trial,castChange(r,b,p));
        session::Op last;
        Serial.printf("CAST stage=%d choices=%d pool=%d shot=%d options=%d meta=%d sp=%d after=%d left=%d undo=%d\n",
            int(cast),choiceCount,p,shot,choiceCount?MetaChoices(r,level).count:0,int(metamagic),session::points(b,s),
            session::points(b,trial),session::remaining(b,s,p),session::last(character,last));
    }
}
}
