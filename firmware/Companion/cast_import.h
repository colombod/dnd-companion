#pragma once
#include "check_import.h"
#include "session_rules.h"
#include "pack_import.h"
namespace casting {
constexpr unsigned version=7;
inline bool playable(unsigned schema){
    return schema>=3 && schema<=version;
}
inline void filter(JsonDocument &f){
    auto d=f["data"];
    for(auto k:{"spellSlots","pactMagic"}){auto p=d[k][0];p["level"]=p["used"]=p["available"]=true;}
    auto cls=d["classes"][0]["definition"];cls["id"]=true;
    for(auto k:{"definition","subclassDefinition"}){
        d["classes"][0][k]["canCastSpells"]=true;
        auto rules=d["classes"][0][k]["spellRules"];
        rules["levelSpellSlots"]=rules["multiClassSpellSlotDivisor"]=rules["multiClassSpellSlotRounding"]=true;
    }
    auto opt=d["options"]["class"][0];opt["componentId"]=opt["definition"]["id"]=opt["definition"]["name"]=true;
    auto a=d["actions"]["class"][0];a["componentId"]=a["limitedUse"]["resetType"]=a["limitedUse"]["numberUsed"]=a["limitedUse"]["maxUses"]=true;
}
struct Grant {const char *name=nullptr; int ability=0; bool locked=false;};
inline Grant classGrant(JsonObjectConst d,int component){
    if(!component)return {};
    const char *option=nullptr;
    for(auto o:d["options"]["class"].as<JsonArrayConst>())
        if(o["definition"]["id"]==component){option=o["definition"]["name"];component=o["componentId"]|0;break;}
    for(auto c:d["classes"].as<JsonArrayConst>())for(auto f:c["classFeatures"].as<JsonArrayConst>())
        if(f["definition"]["id"]==component){
            auto def=f["definition"];
            return {option?option:def["name"].as<const char *>(),c["definition"]["spellCastingAbilityId"]|0,
                    (def["requiredLevel"]|0)>(c["level"]|0)};
        }
    return {};
}
inline int bonusValue(JsonObjectConst m,const checks::Snapshot &s,bool &known){
    if((m["restriction"]|"")[0] || m["tagConstraints"].size() || !m["dice"].isNull()){known=false;return 0;}
    if(m["value"].is<int>())return m["value"];
    int a=m["statId"]|0;if(a>=1 && a<=6 && s.scores[a-1])return dnd::abilityModifier(s.scores[a-1]);
    known=false;return 0;
}
inline void enrich(JsonObjectConst d,JsonObject row,const checks::Snapshot &s){
    int atk=0,dc=0,damage=0;bool atkOK=true,dcOK=true,damageOK=true;
    for(auto g:checks::groups)for(auto m:d["modifiers"][g].as<JsonArrayConst>()){
        const char *t=m["type"]|"",*sub=m["subType"]|"";
        if(!checks::active(d,g,m))continue;
        if(!strcmp(t,"eldritch-blast") && !strcmp(sub,"bonus-damage") && row["beams"].is<int>())damage+=bonusValue(m,s,damageOK);
        if(strcmp(t,"bonus"))continue;
        if(!strcmp(sub,"spell-attacks"))atk+=bonusValue(m,s,atkOK);
        if(!strcmp(sub,"spell-save-dc"))dc+=bonusValue(m,s,dcOK);
    }
    if(atk)row["atkBonus"]=atk;if(dc)row["dcBonus"]=dc;if(damage)row["damageBonus"]=damage;
    if(!atkOK)row["atkReview"]=true;if(!dcOK)row["dcReview"]=true;if(!damageOK)row["damageReview"]=true;
}
inline void resources(JsonObjectConst d,JsonDocument &book){
    static constexpr uint8_t slots[][9]={
      {0},{2},{3},{4,2},{4,3},{4,3,2},{4,3,3},{4,3,3,1},{4,3,3,2},{4,3,3,3,1},{4,3,3,3,2},
      {4,3,3,3,2,1},{4,3,3,3,2,1},{4,3,3,3,2,1,1},{4,3,3,3,2,1,1},{4,3,3,3,2,1,1,1},
      {4,3,3,3,2,1,1,1},{4,3,3,3,2,1,1,1,1},{4,3,3,3,3,1,1,1,1},{4,3,3,3,3,2,1,1,1},{4,3,3,3,3,2,2,1,1}};
    session::Build b;int spellcasters=0,casterLevel=0;JsonArrayConst single;
    for(auto c:d["classes"].as<JsonArrayConst>()){
        auto def=c["definition"];const char *name=def["name"]|"";int lv=c["level"]|0;
        if(!strcmp(name,"Sorcerer")){b.sorcerer=lv;b.modern=def["id"]==2190884;b.spMax=lv>=2?lv:0;}
        auto subclass=c["subclassDefinition"];
        if(def["canCastSpells"]==false && subclass["canCastSpells"]!=true)continue;
        auto rules=def["spellRules"];
        auto subRules=subclass["spellRules"];
        if(!rules["levelSpellSlots"].is<JsonArrayConst>() ||
           (def["canCastSpells"]==false && subRules["levelSpellSlots"].is<JsonArrayConst>()))rules=subRules;
        auto row=rules["levelSpellSlots"][lv].as<JsonArrayConst>();if(!row.size())continue;
        if(!strcmp(name,"Warlock")){
            for(int i=0;i<9;i++)b.limit[i+9]=row[i]|0;
            continue;
        }
        bool active=false;for(auto n:row)if(n.as<int>())active=true;
        if(!active)continue;
        ++spellcasters;single=row;
        int divisor=rules["multiClassSpellSlotDivisor"]|0;
        if(divisor==1)casterLevel+=lv;
        else if(divisor==2 || divisor==3){
            int rounding=rules["multiClassSpellSlotRounding"]|0;
            if(rounding!=1 && rounding!=2)b.known=false;
            else casterLevel+=(lv+(rounding==2?divisor-1:0))/divisor;
        }else if(spellcasters>0) b.known=false;
    }
    if(spellcasters==1){for(int i=0;i<9;i++)b.limit[i]=single[i]|0;b.known=true;}
    else if(spellcasters>1 && casterLevel>=0 && casterLevel<=20)memcpy(b.limit,slots[casterLevel],9);
    for(auto key:{"spellSlots","pactMagic"})for(auto p:d[key].as<JsonArrayConst>()){
        int lv=p["level"]|0;if(lv<1 || lv>9)continue;int i=lv-1+(!strcmp(key,"pactMagic")?9:0);
        b.spent[i]=std::clamp(p["used"]|0,0,255);
    }
    auto meta=book["metamagic"].to<JsonArray>();
    for(auto a:d["actions"]["class"].as<JsonArrayConst>()){
        const char *name=a["name"]|"";
        if(!strcmp(name,"Font of Magic: Sorcery Points"))b.spUsed=std::clamp(a["limitedUse"]["numberUsed"]|0,0,int(b.spMax));
        if(!strncmp(name,"Metamagic: ",11))meta.add(name+11);
    }
    book["sorc"]=b.sorcerer;book["modern"]=b.modern;book["spMax"]=b.spMax;book["spUsed"]=b.spUsed;book["poolsKnown"]=b.known;
    auto cap=book["limits"].to<JsonArray>(); auto spent=book["spent"].to<JsonArray>();
    for(int i=0;i<18;i++){cap.add(b.limit[i]);spent.add(b.spent[i]);}
    book["schema"]=version;
}
inline session::Build build(JsonDocument &book){
    session::Build b;
    b.bag=pack::source(book);
    b.forgeKnown=(book["schema"]|0)>=5;
    b.geniusUnresolved=book["geniusUnresolved"]==true;
    b.inkUnresolved=book["inkUnresolved"]==true;
    b.geniusMax=book["geniusMax"]|0;b.geniusUsed=book["geniusUsed"]|0;b.geniusRecovery=book["geniusRecovery"]|0;
    b.inkMax=book["inkMax"]|0;b.inkUsed=book["inkUsed"]|0;
    auto hp=book["health"];b.hpHint=hp["hint"]|0;b.removedHP=hp["removed"]|0;
    b.sourceSuccess=hp["saves"]|0;b.sourceFail=hp["fails"]|0;b.sourceStable=hp["stable"]==true;
    b.revisedRest=hp["revisedRest"]==true;
    auto gear=book["gear"];b.miscAC=gear["misc"]|0;b.baseAC=gear["ac"]|0;b.mageAC=gear["mageAC"]|0;b.sourceTemp=gear["temp"]|0;b.armored=gear["armored"]==true;
    b.known=book["poolsKnown"]==true;b.sorcerer=book["sorc"]|0;b.modern=book["modern"]==true;b.spMax=book["spMax"]|0;b.spUsed=book["spUsed"]|0;
    for(int i=0;i<18;i++){b.limit[i]=book["limits"][i]|0;b.spent[i]=book["spent"][i]|0;}
    return b;
}
}
