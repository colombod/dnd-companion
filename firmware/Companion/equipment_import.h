#pragma once
#include "check_import.h"
#include <cctype>
namespace equipment {
// A saved choice only resolves conflicting marks in the accepted character book.
struct WeaponBond {
    uint32_t item{};
    unsigned marked{};
    bool canChoose(JsonObjectConst row) const {
        return marked>1 && row["itemId"].as<uint32_t>() && row["pactRoll"].is<JsonObjectConst>();
    }
    bool owns(JsonObjectConst row) const {
        return item && row["pactRoll"].is<JsonObjectConst>() && row["itemId"].as<uint32_t>()==item;
    }
    JsonObjectConst roll(JsonObjectConst row) const {
        // Older books sometimes baked the pact ability into the only saved roll.
        if(row["pact"]=="Bonded")return {};
        return owns(row)?row["pactRoll"].as<JsonObjectConst>():row;
    }
};
inline WeaponBond resolveBond(JsonObjectConst book,uint32_t savedItem){
    WeaponBond bond;
    if(book["gear"]["pactEnabled"]!=true)return bond;
    bond.marked=book["gear"]["pactMarks"]|0u;
    for(auto row:book["actions"].as<JsonArrayConst>()){
        uint32_t id=row["itemId"].as<uint32_t>();
        if(row["weapon"]==true && row["pactRoll"].is<JsonObjectConst>() &&
           (bond.marked==1 || (bond.marked>1 && id==savedItem)))bond.item=id;
    }
    return bond;
}
inline void filter(JsonDocument &f){
    auto d=f["data"];d["temporaryHitPoints"]=true;
    auto item=d["inventory"][0];item["id"]=true;
    auto w=item["definition"];
    for(auto k:{"type","categoryId","attackType","armorTypeId","armorClass","filterType","name","damageType","range"})w[k]=true;
    w["properties"][0]["name"]=w["properties"][0]["notes"]=true;
    auto m=w["grantedModifiers"][0];m["type"]=m["subType"]=m["value"]=m["restriction"]=true;
    for(auto group:checks::groups){
        auto mod=d["modifiers"][group][0];
        mod["entityId"]=mod["componentTypeId"]=mod["isGranted"]=true;
    }
}
inline bool trained(JsonObjectConst d,const char *name){
    for(auto group:checks::groups)for(auto m:d["modifiers"][group].as<JsonArrayConst>())
        if(m["type"]=="proficiency" && m["subType"]==name && checks::active(d,group,m))return true;
    return false;
}
inline bool marked(JsonObjectConst d,JsonObjectConst item,int type){
    for(auto v:d["characterValues"].as<JsonArrayConst>())
        if(v["typeId"]==type && v["valueId"].as<uint32_t>()==item["id"].as<uint32_t>() && v["value"]==true)return true;
    return false;
}
inline bool modernClass(JsonObjectConst d,bool warlockOnly){
    for(auto c:d["classes"].as<JsonArrayConst>()){
        if(warlockOnly?c["definition"]["name"]!="Warlock":c["isStartingClass"]!=true)continue;
        return (c["definition"]["id"]|0)>2000000;
    }
    return false;
}
struct WeaponGrant {
    bool enabled{};
    unsigned abilities{};
};
inline bool unconditionalGrant(JsonObjectConst d,const char *group,JsonObjectConst mod){
    return mod["isGranted"]!=false && !(mod["restriction"]|"")[0] &&
           !mod["tagConstraints"].size() && checks::active(d,group,mod);
}
inline WeaponGrant weaponGrant(JsonObjectConst d,const char *subtype){
    WeaponGrant grant;
    for(auto group:checks::groups){
        auto mods=d["modifiers"][group].as<JsonArrayConst>();
        for(JsonObjectConst enable:mods){
            if(enable["type"]!="enable-feature" || enable["subType"]!=subtype || !unconditionalGrant(d,group,enable))continue;
            grant.enabled=true;
            // Beyond associates replacement abilities with the same granting feature.
            int origin=enable["componentId"]|0;
            if(!origin)continue;
            for(JsonObjectConst mod:mods){
                if(mod["type"]!="replace-weapon-ability" || mod["componentId"]!=origin ||
                   mod["componentTypeId"]!=enable["componentTypeId"] || !unconditionalGrant(d,group,mod))continue;
                int stat=mod["entityId"]|0;
                if(stat>=1 && stat<=6)grant.abilities|=1u<<(stat-1);
            }
        }
    }
    return grant;
}
inline void import(JsonObjectConst d,JsonDocument &book,JsonArray actions,const checks::Snapshot &scores,int level){
    auto gear=book["gear"].to<JsonObject>();
    bool hasDex=scores.scores[1]>0,acKnown=hasDex && d["inventory"].is<JsonArrayConst>();
    int dex=hasDex?dnd::abilityModifier(scores.scores[1]):0;
    int base=10+dex,otherBase=base,bonus=0,shield=0,bodies=0,shields=0,pactTags=0,hexTags=0;
    const auto pact=weaponGrant(d,"enable-pact-weapon");
    const auto hex=weaponGrant(d,"enable-hex-weapon");
    gear["pactEnabled"]=pact.enabled;
    auto defense=actions.add<JsonObject>();defense["name"]="Defenses";defense["defense"]=true;
    for(auto item:d["inventory"].as<JsonArrayConst>()){
        if(item["definition"]["filterType"]=="Weapon" && item["id"].as<uint32_t>()){
            if(pact.enabled && marked(d,item,28))++pactTags;
            if(hex.enabled && marked(d,item,29))++hexTags;
        }
        if(item["equipped"]!=true)continue;
        auto w=item["definition"];int kind=w["armorTypeId"]|0;
        if(w["filterType"]!="Armor")continue;
        int ac=w["armorClass"]|0;if(!ac || kind<1 || kind>4)acKnown=false;
        if(kind==4){
            ++shields;gear["shield"]=w["name"];
            if(!modernClass(d,false) || trained(d,"shields") || trained(d,"shield"))shield=ac;
            else gear["untrainedShield"]=true;
        }else{
            ++bodies;gear["armor"]=w["name"];
            base=ac+(kind==1?dex:kind==2?std::min(2,dex):0);
        }
    }
    if(pactTags)gear["pactMarks"]=pactTags;
    if(shields>1 || bodies>1)acKnown=false;
    for(auto group:checks::groups)for(auto m:d["modifiers"][group].as<JsonArrayConst>()){
        if(!checks::active(d,group,m))continue;
        if(m["subType"]=="unarmored-armor-class" && !bodies && m["type"]=="set"){
            if(m["value"].is<int>() && !(m["restriction"]|"")[0])otherBase=std::max(otherBase,10+dex+(m["value"]|0));
            else acKnown=false;
        }else if(m["subType"]=="armor-class" || (!bodies && m["subType"]=="unarmored-armor-class")){
            if(m["type"]=="bonus" && m["value"].is<int>() && !(m["restriction"]|"")[0] && !m["tagConstraints"].size())bonus+=m["value"].as<int>();
            else acKnown=false;
        }
    }
    int misc=0;
    for(auto v:d["characterValues"].as<JsonArrayConst>())if(v["valueId"].isNull() && v["value"].is<int>()){
        int type=v["typeId"]|0;
        if(type==3)misc+=v["value"].as<int>();
        if(type==2)bonus+=v["value"].as<int>();
        if(type==1 || type==4){gear["customAC"]=v["value"];acKnown=false;}
    }
    gear["misc"]=misc;gear["shieldBonus"]=shield;gear["armored"]=bool(bodies);
    gear["ac"]=acKnown?(bodies?base:otherBase)+shield+bonus:0;
    gear["mageAC"]=acKnown && !bodies?13+dex+shield+bonus:0;
    gear["temp"]=d["temporaryHitPoints"]|0;
    for(auto item:d["inventory"].as<JsonArrayConst>()){
        auto w=item["definition"];
        if(item["equipped"]!=true || w["filterType"]!="Weapon")continue;
        auto row=actions.add<JsonObject>();
        row["weapon"]=true;row["itemId"]=item["id"];row["name"]=w["name"];row["src"]="Equipped weapon";
        row["die"]=w["damage"]["diceString"];row["type"]=w["damageType"];
        int ab=w["attackType"]==2?1:0,magic=0;
        bool twoHanded=false;
        bool known=w["attackType"]==1 || w["attackType"]==2;
        for(auto p:w["properties"].as<JsonArrayConst>()){
            if(p["name"]=="Two-Handed")twoHanded=true;
            if(p["name"]=="Finesse" && scores.scores[1]>scores.scores[0])ab=1;
            if(!shields && p["name"]=="Versatile" && p["notes"].is<const char *>())row["twoHand"]=p["notes"];
        }
        for(auto m:w["grantedModifiers"].as<JsonArrayConst>())if(m["subType"]=="magic" && m["type"]=="bonus" &&
            (w["canAttune"]!=true || item["isAttuned"]==true)){
            if(!(m["restriction"]|"")[0] && m["value"].is<int>())magic+=m["value"].as<int>();else known=false;
        }
        char slug[64]{};const char *type=w["type"]|"";size_t i=0;
        for(;type[i] && i+1<sizeof(slug);i++)slug[i]=type[i]==' '?'-':std::tolower(static_cast<unsigned char>(type[i]));
        bool prof=trained(d,slug) || (w["categoryId"]==1 && trained(d,"simple-weapons")) ||
                  (w["categoryId"]==2 && trained(d,"martial-weapons"));
        int hitBonus=0,damageBonus=0,hitOverride=0;bool fixedHit=false;
        int profOverride=-1;
        unsigned otherAbilities=0;
        for(auto v:d["characterValues"].as<JsonArrayConst>()){
            if(v["valueId"].as<uint32_t>()!=item["id"].as<uint32_t>())continue;
            int t=v["typeId"]|0;
            if(t==17)profOverride=v["value"].as<bool>();
            if(t==12)hitBonus+=v["value"]|0;
            if(t==13){hitOverride=v["value"]|0;fixedHit=true;}
            if(t==10)damageBonus+=v["value"]|0;
            // Weapon ability checkboxes can outlive their grant too.
            if(t>=51 && t<=56 && v["value"]==true){
                int ability=t-51;
                char feature[32];
                snprintf(feature,sizeof(feature),"enable-%s",checks::slugs[ability]);
                otherAbilities|=weaponGrant(d,feature).abilities & (1u<<ability);
            }
            if(t==11 || t==30 || t==31)known=false;
        }
        for(auto group:checks::groups)for(auto m:d["modifiers"][group].as<JsonArrayConst>()){
            if(!checks::active(d,group,m) || m["type"]!="bonus")continue;
            if(m["subType"]=="weapon-attacks" || (w["attackType"]==1 && m["subType"]=="melee-weapon-attacks") ||
               (w["attackType"]==2 && m["subType"]=="ranged-weapon-attacks")){
                if(!(m["restriction"]|"")[0] && m["value"].is<int>() && !m["tagConstraints"].size())hitBonus+=m["value"].as<int>();
                else known=false;
            }
        }
        if(shields && twoHanded)known=false;
        bool hexMarked=hex.enabled && !twoHanded && marked(d,item,29);
        auto writeRoll=[&](JsonObject target,bool bonded){
            bool proficient=profOverride>=0?bool(profOverride):prof || bonded;
            bool hexWeapon=hexMarked && proficient;
            if(hexWeapon && hexTags==1)target["hex"]=true;
            unsigned allowed=otherAbilities | (bonded?pact.abilities:0);
            if(hexWeapon && hexTags==1)allowed|=hex.abilities;
            int stat=ab;
            for(int i=0;i<6;i++)if((allowed&(1u<<i)) && scores.scores[i]>scores.scores[stat])stat=i;
            if(!known || !scores.scores[stat] || (hexWeapon && hexTags>1)){target["weaponReview"]=true;return;}
            int mod=dnd::abilityModifier(scores.scores[stat]);
            target["toHit"]=fixedHit?hitOverride:mod+magic+hitBonus+(proficient?dnd::proficiencyBonus(level):0);
            target["weaponBonus"]=mod+magic+damageBonus;
            target["stat"]=checks::shortNames[stat];target["proficient"]=proficient;
        };
        writeRoll(row,false);
        if(pact.enabled && marked(d,item,28) && item["id"].as<uint32_t>())writeRoll(row["pactRoll"].to<JsonObject>(),true);
    }
}
}
