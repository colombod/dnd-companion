#pragma once
#include "check_import.h"

// IDs from the published Forge Artificer and Cartographer on D&D Beyond.
// Require the owning class and an unlocked feature before enabling its rule.
namespace artificer {
constexpr int forgeClass=2656866, genius=12497132, advanced=12497135, soul=12497141,
              cartographerSpells=12497181, mapping=12497183, precision=12497184, faerieFire=2618858;
inline bool hasFeature(JsonObjectConst cls,int feature,int minimumLevel){
    int level=cls["level"]|0;
    if(cls["definition"]["id"]!=forgeClass || level<minimumLevel)return false;
    for(auto entry:cls["classFeatures"].as<JsonArrayConst>()){
        auto def=entry["definition"];
        if(def["id"]==feature && (def["requiredLevel"]|minimumLevel)<=level)return true;
    }
    return false;
}
inline bool inkSpell(JsonObjectConst spell){
    return spell["componentId"]==mapping && spell["definition"]["id"]==faerieFire &&
           spell["usesSpellSlot"]==false && (spell["restriction"]|"")[0]==0 &&
           spell["limitedUse"]["resetType"]==2 && spell["limitedUse"]["numberUsed"].is<int>();
}
inline void import(JsonObjectConst d,const checks::Snapshot &stats,JsonDocument &book){
    for(auto c:d["classes"].as<JsonArrayConst>()){
        if(c["definition"]["id"]!=forgeClass)continue;
        bool ownsGenius=hasFeature(c,genius,7),ownsMapping=hasFeature(c,mapping,3);
        if(!stats.scores[3]){
            if(ownsGenius)book["geniusUnresolved"]=true;
            if(ownsMapping)book["inkUnresolved"]=true;
            continue;
        }
        if(hasFeature(c,precision,5))book["guidedPrecision"]=true;
        int capacity=std::max(1,dnd::abilityModifier(stats.scores[3]));
        if(ownsGenius){
            for(auto a:d["actions"]["class"].as<JsonArrayConst>()){
                if(a["componentId"]!=genius || a["limitedUse"]["resetType"]!=2 ||
                   !a["limitedUse"]["numberUsed"].is<int>())continue;
                book["geniusMax"]=capacity;
                book["geniusUsed"]=std::clamp(a["limitedUse"]["numberUsed"].as<int>(),0,capacity);
                int recover=hasFeature(c,advanced,14)?1:0;
                if(hasFeature(c,soul,20))for(auto i:d["inventory"].as<JsonArrayConst>())
                    if(i["isAttuned"]==true && i["definition"]["canAttune"]==true){recover=capacity;break;}
                book["geniusRecovery"]=recover;
                break;
            }
        }
        if(ownsMapping)for(auto spell:d["spells"]["class"].as<JsonArrayConst>()){
            if(!inkSpell(spell))continue;
            book["inkMax"]=capacity;
            book["inkUsed"]=std::clamp(spell["limitedUse"]["numberUsed"].as<int>(),0,capacity);
            break;
        }
        if(ownsGenius && !book["geniusMax"].is<int>())book["geniusUnresolved"]=true;
        if(ownsMapping && !book["inkMax"].is<int>())book["inkUnresolved"]=true;
    }
}
}
