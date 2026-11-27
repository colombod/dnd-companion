#pragma once
#include "checks.h"
#include "cast_import.h"
#include "artificer_import.h"
#include "equipment_import.h"
#include <ArduinoJson.h>

namespace moves {
inline constexpr const char *sources[]={"class","race","feat","background","item"};
inline void filterSpell(JsonVariant f){
    for(auto k:{"spellCastingAbilityId","overrideSaveDc","prepared","alwaysPrepared","usesSpellSlot","castOnlyAsRitual","castAtLevel","limitedUse","componentId","restriction"})f[k]=true;
    for(auto k:{"id","name","description","school","level","concentration","ritual","requiresAttackRoll","requiresSavingThrow","saveDcAbilityId","isHomebrew"})f["definition"][k]=true;
    f["definition"]["activation"]["activationType"]=f["definition"]["activation"]["activationTime"]=true;
    f["definition"]["range"]["rangeValue"]=f["definition"]["range"]["origin"]=true;
    auto m=f["definition"]["modifiers"][0];
    m["type"]=m["subType"]=m["usePrimaryStat"]=m["die"]["diceString"]=m["die"]["fixedValue"]=true;
    auto scale=m["atHigherLevels"]["higherLevelDefinitions"][0];
    scale["level"]=scale["typeId"]=scale["value"]=scale["dice"]["diceString"]=true;
    auto target=f["definition"]["atHigherLevels"]["higherLevelDefinitions"][0];
    target["level"]=target["typeId"]=target["value"]=true;
}
inline void filter(JsonDocument &f){
    casting::filter(f);equipment::filter(f);pack::filter(f);
    auto d=f["data"];
    for(auto key:{"overrideHitPoints","baseHitPoints","bonusHitPoints","removedHitPoints"})d[key]=true;
    d["deathSaves"]=true;
    d["classes"][0]["id"]=d["classes"][0]["definition"]["spellCastingAbilityId"]=true;
    d["classes"][0]["definition"]["spellPrepareType"]=d["classes"][0]["definition"]["knowsAllSpells"]=true;
    d["classSpells"][0]["characterClassId"]=true;
    filterSpell(d["classSpells"][0]["spells"][0].to<JsonObject>());
    for(auto src:sources){
        filterSpell(d["spells"][src][0].to<JsonObject>());
        auto a=d["actions"][src][0];
        for(auto k:{"name","snippet","fixedSaveDc","saveStatId","fixedToHit","damageTypeId"})a[k]=true;
        a["activation"]["activationType"]=a["activation"]["activationTime"]=a["dice"]["diceString"]=true;
        a["range"]["range"]=true;
    }
    auto w=d["inventory"][0]["definition"];
    for(auto k:{"name","filterType","damageType","range"})w[k]=true;
    w["damage"]["diceString"]=true;
}
inline const char *sourceName(const char *src){
    if(!strcmp(src,"class"))return "Class feature";
    if(!strcmp(src,"race"))return "Species";
    if(!strcmp(src,"background"))return "Background";
    if(!strcmp(src,"item"))return "Item";
    return "Feat";
}
inline void addBrief(JsonObject row,const char *p){
    char buf[245]{};size_t n=0,lastSpace=0;
    while(*p && n<sizeof(buf)-5){
        if(*p=='<' || *p=='['){
            const char *end=strchr(p,*p=='<'?'>':']');
            if(!end || !strncmp(p,"[rollable]",10))break;
            if(!strncmp(p,"</p",3) && n>=60)break;
            p=end+1;continue;
        }
        if(*p=='{' || *p=='&')break;
        if((unsigned char)*p<=32){
            ++p;if(!n || buf[n-1]==' ')continue;
            lastSpace=n;buf[n++]=' ';
        }else buf[n++]=*p++;
    }
    if(*p && n){
        if(n>=sizeof(buf)-5)n=lastSpace;
        while(n && buf[n-1]==' ')--n;
        if(n && !strchr(".!?",buf[n-1])){buf[n++]='.';buf[n++]='.';buf[n++]='.';}
    }
    buf[n]=0;
    if(n>10)row["brief"]=buf;
}
inline void addSpell(JsonArray list,JsonObjectConst record,const char *source,int ability,int level,bool needsPreparation){
    auto def=record["definition"];
    if(!def["name"].is<const char *>())return;
    auto row=list.add<JsonObject>();
    row["spellId"]=def["id"];row["name"]=def["name"];row["src"]=source;row["lv"]=def["level"];row["school"]=def["school"];
    row["time"]=def["activation"]["activationType"];row["timeN"]=def["activation"]["activationTime"];
    row["ft"]=def["range"]["rangeValue"];row["range"]=def["range"]["origin"];
    if(def["concentration"]==true)row["con"]=true;
    if(record["castOnlyAsRitual"]==true)row["ritualOnly"]=true;
    else if(def["ritual"]==true)row["ritual"]=true;
    if(needsPreparation && def["level"].as<int>()>0 && !record["alwaysPrepared"].as<bool>())row["prepared"]=record["prepared"].as<bool>();
    if(record["alwaysPrepared"]==true)row["prepared"]=true;
    row["usesSlot"]=record["usesSpellSlot"]==true;
    if(record["castAtLevel"].is<int>())row["fixedLevel"]=record["castAtLevel"];
    if(def["level"].as<int>()>0 && !record["usesSpellSlot"].is<bool>() && record["castOnlyAsRitual"]!=true)
        row["costReview"]=true;
    if(!record["limitedUse"].isNull() || (record["restriction"]|"")[0])row["costReview"]=true;
    row["ability"]=record["spellCastingAbilityId"]|ability;
    if(def["requiresAttackRoll"]==true)row["hit"]=true;
    if(def["requiresSavingThrow"]==true)row["save"]=def["saveDcAbilityId"];
    if(record["overrideSaveDc"].is<int>())row["dc"]=record["overrideSaveDc"];
    auto effects=row["effects"].to<JsonArray>();
    for(JsonObjectConst m:def["modifiers"].as<JsonArrayConst>()){
        auto dice=m["die"]["diceString"];
        if(!dice.is<const char *>() && !m["die"]["fixedValue"].is<int>())continue;
        if(def["level"]==0){
            int tier=0;
            for(auto t:m["atHigherLevels"]["higherLevelDefinitions"].as<JsonArrayConst>())
                if(t["typeId"]==15 && t["level"].as<int>()<=level && t["level"].as<int>()>tier && t["dice"]["diceString"].is<const char *>()){
                    tier=t["level"];dice=t["dice"]["diceString"];
                }
        }
        auto e=effects.add<JsonArray>();e.add(dice);e.add(m["subType"]);e.add(m["usePrimaryStat"]==true);
        JsonObjectConst increment;
        if(def["level"].as<int>()>0)for(auto t:m["atHigherLevels"]["higherLevelDefinitions"].as<JsonArrayConst>())
            if(t["typeId"]==15 && t["level"]==1){increment=t;break;}
        e.add(increment["dice"]["diceString"]);e.add(m["die"]["fixedValue"]);e.add(increment["value"]);
    }
    int id=def["id"]|0;
    if(id==2619006 || id==2172)row["ward"]=int(session::Ward::MageArmor);
    if(id==2618870)row["ward"]=int(session::Ward::Agathys);
    if(id==2619019 || id==2247)row["ward"]=int(session::Ward::Shield);
    if(id==2619161 || id==2082)row["beams"]=1+(level>=5)+(level>=11)+(level>=17);
    if(id==2619022 || id==2176)row["missile"]=true;
    if(id==2618839)row["burst"]=true;
    if(id==2618966)row["orb"]=true;
    if(id==2619215)row["witch"]=true;
    const char *desc=def["description"]|"";
    if(def["requiresSavingThrow"]==true && (strstr(desc,"On a successful save, a target takes half") ||
       strstr(desc,"half as much damage on a successful") || strstr(desc,"half as much damage on a success")))row["half"]=true;
    for(auto t:def["atHigherLevels"]["higherLevelDefinitions"].as<JsonArrayConst>())
        if(t["typeId"]==1 && t["value"]==1 && t["level"]==1)row["extraTarget"]=true;
    addBrief(row,def["description"]|"");
}
inline void importHealth(JsonObjectConst d,JsonDocument &out,const checks::Snapshot &sheet,int level){
    // The user confirms the suggested maximum. A potion or a conditional heal is not a maximum-HP bonus.
    auto hp=out["health"].to<JsonObject>();
    if((d["overrideHitPoints"]|0)>0)hp["hint"]=d["overrideHitPoints"];
    else if(d["baseHitPoints"].is<int>() && sheet.scores[2]){
        int64_t maximum=int64_t(d["baseHitPoints"]|0)+(d["bonusHitPoints"]|0)+level*dnd::abilityModifier(sheet.scores[2]);
        for(auto group:checks::groups)for(auto mod:d["modifiers"][group].as<JsonArrayConst>()){
            if(mod["type"]!="bonus" || mod["subType"]!="hit-points-per-level" || !mod["value"].is<int>() ||
               !equipment::unconditionalGrant(d,group,mod))continue;
            int levels=level;
            if(!strcmp(group,"class")){
                levels=0;
                for(auto cls:d["classes"].as<JsonArrayConst>())
                    for(auto feature:cls["classFeatures"].as<JsonArrayConst>())
                        if(feature["definition"]["id"]==mod["componentId"])levels=cls["level"]|0;
            }
            maximum+=int64_t(levels)*mod["value"].as<int>();
        }
        hp["hint"]=std::clamp<int64_t>(maximum,1,vitals::limit);
    }
    hp["removed"]=std::clamp(d["removedHitPoints"]|0,0,vitals::limit);
    hp["saves"]=d["deathSaves"]["successCount"]|0;hp["fails"]=d["deathSaves"]["failCount"]|0;
    hp["stable"]=d["deathSaves"]["isStabilized"]==true;
    hp["revisedRest"]=equipment::modernClass(d,false);
}
inline bool extract(JsonObjectConst d,JsonDocument &out,int level){
    if(!d["classSpells"].is<JsonArrayConst>() && !d["spells"].is<JsonObjectConst>() && !d["actions"].is<JsonObjectConst>())return false;
    auto spells=out["spells"].to<JsonArray>();
    auto actions=out["actions"].to<JsonArray>();
    checks::Snapshot sheet{};checks::importRolls(d,sheet);
    importHealth(d,out,sheet,level);
    pack::import(d,out);
    equipment::import(d,out,actions,sheet,level);
    artificer::import(d,sheet,out);
    for(auto book:d["classSpells"].as<JsonArrayConst>()){
        JsonObjectConst cls;
        for(auto c:d["classes"].as<JsonArrayConst>())if(c["id"]==book["characterClassId"]){cls=c;break;}
        for(auto s:book["spells"].as<JsonArrayConst>())addSpell(spells,s,cls["definition"]["name"]|"Class",cls["definition"]["spellCastingAbilityId"]|0,level,cls["definition"]["knowsAllSpells"]==true);
    }
    for(auto src:sources){
        for(auto s:d["spells"][src].as<JsonArrayConst>()){
            auto size=spells.size();
            auto grant=!strcmp(src,"class")?casting::classGrant(d,s["componentId"]|0):casting::Grant{};
            if(grant.locked)continue;
            addSpell(spells,s,grant.name?grant.name:sourceName(src),grant.ability,level,false);
            if(spells.size()>size && !strcmp(src,"item") && (s["componentId"]|0u)){
                for(auto i:d["inventory"].as<JsonArrayConst>())if(pack::consumable(i["definition"]) &&
                   (i["definition"]["id"]==s["componentId"] || i["id"]==s["componentId"])){
                    spells[size]["costReview"]=true;break;
                }
            }
            if(spells.size()>size && !strcmp(src,"class") && out["inkMax"].as<int>() && artificer::inkSpell(s)){
                auto row=spells[size].as<JsonObject>();
                row.remove("costReview");row["mappingMagic"]=true;
            }
            if(spells.size()==size || spells[size]["ward"]!=int(session::Ward::MageArmor) || strcmp(src,"class"))continue;
            for(auto opt:d["options"]["class"].as<JsonArrayConst>())
                if(opt["definition"]["id"]==s["componentId"] && opt["definition"]["name"]=="Armor of Shadows"){
                    spells[size]["selfOnly"]=true;spells[size]["src"]="Armor of Shadows";break;
                }
        }
        for(auto a:d["actions"][src].as<JsonArrayConst>()){
            if(!a["name"].is<const char *>())continue;
            auto grant=!strcmp(src,"class")?casting::classGrant(d,a["componentId"]|0):casting::Grant{};
            if(grant.locked)continue;
            auto row=actions.add<JsonObject>();row["name"]=a["name"];row["src"]=grant.name?grant.name:sourceName(src);
            if(!strcmp(src,"class") && a["componentId"]==artificer::genius && out["geniusMax"].as<int>())row["genius"]=true;
            row["time"]=a["activation"]["activationType"];row["timeN"]=a["activation"]["activationTime"];
            row["ft"]=a["range"]["range"];row["die"]=a["dice"]["diceString"];
            if(a["fixedToHit"].is<int>())row["toHit"]=a["fixedToHit"];
            if(a["fixedSaveDc"].is<int>())row["dc"]=a["fixedSaveDc"];
            if(a["saveStatId"].is<int>())row["save"]=a["saveStatId"];
            addBrief(row,a["snippet"]|"");
        }
    }
    if(out["guidedPrecision"]==true){
        // Match the spell itself, including a second grant of a listed spell.
        for(auto row:spells){
            for(auto listed:d["spells"]["class"].as<JsonArrayConst>()){
                if(listed["componentId"]!=artificer::cartographerSpells)continue;
                auto def=listed["definition"];
                if(row["spellId"]!=def["id"])continue;
                if(def["id"]==artificer::faerieFire)row["protectedFire"]=true;
                for(auto m:def["modifiers"].as<JsonArrayConst>())
                    if(m["type"]=="damage"){row["precisionDamage"]=true;break;}
                break;
            }
        }
    }
    for(auto value:spells){
        auto row=value.as<JsonObject>();
        row.remove("spellId");
        casting::enrich(d,row,sheet);
    }
    casting::resources(d,out);
    return !out.overflowed();
}
}
