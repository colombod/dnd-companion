#pragma once
#include "move_roll.h"
#include "checks.h"
#include "session_rules.h"
#include <ArduinoJson.h>
namespace casting {
enum class Meta { None, Distant, Twinned };
inline const char *metaName(Meta option){
    if(option==Meta::Distant)return "Distant Spell";
    if(option==Meta::Twinned)return "Twinned Spell";
    return "";
}
inline int distantRange(JsonObjectConst spell){
    if(spell["range"]=="Touch")return 30;
    int feet=spell["ft"]|0;
    return spell["range"]=="Ranged" && feet>=5?2*feet:0;
}
inline bool offersMeta(Meta option,JsonObjectConst spell,JsonObjectConst book,int castLevel){
    if(option==Meta::Distant && (spell["selfOnly"]==true || !distantRange(spell)))return false;
    if(option==Meta::Twinned && (book["modern"]!=true || spell["extraTarget"]!=true || castLevel<1 || castLevel>=9))return false;
    for(auto learned:book["metamagic"].as<JsonArrayConst>())
        if(learned==metaName(option))return true;
    return false;
}
inline int castLevel(JsonObjectConst spell,int pool){
    return pool<0?(spell["fixedLevel"]|(spell["lv"]|0)):session::level(pool);
}
inline session::Change castCost(Meta option,bool modern,int pool,int level){
    return {session::Op::Cast,pool,session::metamagicCost(metaName(option),modern,level)};
}
inline session::Change spellCast(JsonObjectConst spell,Meta meta,bool modern,int pool,bool self){
    int level=castLevel(spell,pool);
    auto change=castCost(meta,modern,pool,level);
    change.ward=session::Ward(spell["ward"]|0);
    if(change.ward==session::Ward::MageArmor && !self)change.ward=session::Ward::None;
    change.timing=spell["ritualOnly"]==true?pack::Timing::None:pack::activation(spell["time"]|0);
    change.amount=level;
    change.mappingMagic=spell["mappingMagic"]==true;
    return change;
}
inline int atWillMageArmor(JsonObjectConst book){
    if(book["gear"]["armored"]==true)return -1;
    int i=0;
    for(auto spell:book["spells"].as<JsonArrayConst>()){
        if(spell["ward"]==int(session::Ward::MageArmor) && spell["selfOnly"]==true &&
           spell["usesSlot"]==false && spell["prepared"]!=false && spell["costReview"]!=true)return i;
        ++i;
    }
    return -1;
}
inline int spellAbility(JsonObjectConst r,const checks::Snapshot &s,bool &known){
    int a=r["ability"]|0;known=a>=1 && a<=6 && s.scores[a-1]>=1;
    return known?dnd::abilityModifier(s.scores[a-1]):0;
}
inline moves::Roll effect(JsonObjectConst r,const checks::Snapshot &s,int index,int slot,bool critical=false){
    auto e=r["effects"][index];auto roll=moves::readDice(e[0]|"");
    if(!roll.valid() || r["damageReview"]==true)return {};
    if(e[2]==true){bool known;roll.bonus+=spellAbility(r,s,known);if(!known)return {};}
    roll.bonus+=r["damageBonus"]|0;
    int up=std::max(0,slot-(r["lv"]|0));
    if(up && e[3].is<const char *>()){
        auto add=moves::readDice(e[3]);
        if(!add.valid() || add.sides!=roll.sides)return {};
        roll.count+=up*add.count;roll.bonus+=up*add.bonus;
    }
    if(critical && !(r["witch"]==true && index>0))roll.count*=2;
    if(r["burst"]==true){bool known;int mod=spellAbility(r,s,known);if(!known)return {};roll.extraOnMax=std::max(0,mod);}
    roll.leapOnPair=r["orb"]==true;roll.halfOnSave=r["half"]==true;
    return roll.count<=100?roll:moves::Roll{};
}
inline int fixedEffect(JsonVariantConst effect,int baseLevel,int slot){
    return (effect[4]|0)+std::max(0,slot-baseLevel)*(effect[5]|0);
}
inline int shots(JsonObjectConst r,int level){return r["missile"]==true?2+level:r["beams"]|1;}
}
