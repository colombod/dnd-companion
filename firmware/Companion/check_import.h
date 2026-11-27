#pragma once
#include "checks.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cstdio>

namespace checks {
inline constexpr const char *groups[]={"race","class","background","feat","item","condition"};
inline void filterRollData(JsonDocument &filter) {
    auto d=filter["data"];
    for (auto key:{"stats","bonusStats","overrideStats"}) {
        d[key][0]["id"]=true;d[key][0]["value"]=true;
    }
    auto c=d["classes"][0];
    c["isStartingClass"]=true;
    auto feature=c["classFeatures"][0]["definition"];
    feature["id"]=feature["name"]=feature["requiredLevel"]=true;
    auto item=d["inventory"][0];item["equipped"]=item["isAttuned"]=true;
    item["definition"]["id"]=item["definition"]["canAttune"]=true;
    auto custom=d["characterValues"][0];
    custom["typeId"]=custom["valueId"]=custom["value"]=true;
    for (auto group:groups) for (auto key:{"type","subType","statId","value","restriction","availableToMulticlass",
            "componentId","requiresAttunement","dice","bonusTypes","tagConstraints"}) d["modifiers"][group][0][key]=true;
}
inline bool is(const char *s,const char *other) {return strcmp(s,other)==0;}
inline int statValue(JsonObjectConst d,const char *key,int id,int missing) {
    for (JsonObjectConst v:d[key].as<JsonArrayConst>()) if (v["id"]==id && v["value"].is<int>()) return v["value"];
    return missing;
}
inline bool active(JsonObjectConst d,const char *group,JsonObjectConst mod) {
    int component=mod["componentId"]|0;
    if (is(group,"item")) {
        for(JsonObjectConst i:d["inventory"].as<JsonArrayConst>()) if(i["definition"]["id"]==component && i["equipped"]==true &&
            (!(mod["requiresAttunement"]==true || i["definition"]["canAttune"]==true) || i["isAttuned"]==true)) return true;
        return false;
    }
    if (is(group,"class")) for(JsonObjectConst c:d["classes"].as<JsonArrayConst>()) {
        for(JsonObjectConst f:c["classFeatures"].as<JsonArrayConst>()) {
            auto def=f["definition"];
            if(def["id"]!=component) continue;
            if ((def["requiredLevel"]|0)>(c["level"]|0)) return false;
            const char *n=def["name"]|"";
            bool startingPackage=is(n,"Proficiencies") || strncmp(n,"Core ",5)==0;
            if(startingPackage && c["isStartingClass"]==false && mod["availableToMulticlass"]==false) return false;
            return true;
        }
    }
    return true;
}
inline bool applies(const char *sub,int i,const Entry &e) {
    if(i<plain && is(sub,skills[i].slug)) return true;
    if(i>=saves && is(sub,"saving-throws")) return true;
    if(i<saves && is(sub,"ability-checks")) return true;
    if(i<plain && is(sub,"skill-checks")) return true;
    char typed[40];
    snprintf(typed,sizeof(typed),"%s-%s",slugs[e.ability],i>=saves?"saving-throws":"ability-checks");
    return is(sub,typed);
}
inline void collectScore(JsonObjectConst d,Snapshot &s) {
    for(int a=0;a<6;a++){
        int base=statValue(d,"stats",a+1,0),bonus=0,minimum=0,cap=20;
        bool known=base>=1 && base<=30;
        char key[28];snprintf(key,sizeof(key),"%s-score",slugs[a]);
        for(auto g:groups) for(JsonObjectConst m:d["modifiers"][g].as<JsonArrayConst>()){
            const char *sub=m["subType"]|"",*type=m["type"]|"",*restriction=m["restriction"]|"";
            if (!is(sub,key) && !(is(sub,"ability-score-maximum") && m["statId"]==a+1)) continue;
            if (!active(d,g,m)) continue;
            if (!m["value"].is<int>() || (*restriction && !(is(type,"set") && is(restriction,"if not already higher")))) {known=false;continue;}
            int value=m["value"];
            if (value < -30 || value>30) {known=false;continue;}
            if(is(sub,"ability-score-maximum")) cap+=value;
            else if(is(type,"bonus")) bonus+=value;
            else if(is(type,"set")) minimum=std::max(value,minimum);
            else known=false;
        }
        int total=std::max(minimum,std::min(cap,base+bonus)+statValue(d,"bonusStats",a+1,0));
        int override=statValue(d,"overrideStats",a+1,0);
        if(override){total=override;known=true;}
        s.scores[a]=known && total>=1 && total<=30?total:0;
    }
}
inline void addBonus(Entry &e,int n) {
    if (n < -1000 || n>1000 || int(e.bonus)+n < -3000 || int(e.bonus)+n>3000) e.flags|=Unresolved;
    else e.bonus+=n;
}
inline void applyModifier(Entry &e,JsonObjectConst m,const Snapshot &s){
    const char *t=m["type"]|"",*restriction=m["restriction"]|"";
    if(*restriction || m["tagConstraints"].size()){
        e.flags|=Conditional;
        if(is(t,"advantage") && (strstr(restriction,"Concentration") || strstr(restriction,"concentration"))) e.flags|=Concentration;
        return;
    }
    if(is(t,"advantage")) e.flags|=Advantage;
    else if(is(t,"disadvantage")) e.flags|=Disadvantage;
    else if(is(t,"expertise")) e.training=3;
    else if(is(t,"proficiency")) e.training=std::max<int>(e.training,2);
    else if(is(t,"half-proficiency")) e.training=std::max<int>(e.training,1);
    else if(is(t,"bonus")){
        int a=m["statId"]|0;
        if (!m["dice"].isNull() || m["bonusTypes"].size()) e.flags|=Unresolved;
        else if(m["value"].is<int>()) addBonus(e,m["value"].as<int>());
        else if(a>0 && a<=6 && s.scores[a-1]) addBonus(e,dnd::abilityModifier(s.scores[a-1]));
        else e.flags|=Unresolved;
    } else e.flags|=Unresolved;
}
inline int customRow(int type,int id) {
    if(type>=38 && type<=41 && id>=1 && id<=6) return saves+id-1;
    if(type>=23 && type<=27) for(int i=0;i<plain;i++) if(skills[i].sourceId==id) return i;
    return -1;
}
inline void importRolls(JsonObjectConst d,Snapshot &s){
    if (!d["stats"].is<JsonArrayConst>() || !d["modifiers"].is<JsonObjectConst>()) return;
    s.version=1;
    collectScore(d,s);
    for(int i=0;i<count;i++) s.rows[i].ability=i<plain?skills[i].ability:(i-plain)%6;
    // An ability override changes which ability-wide modifiers apply to the skill.
    for(JsonObjectConst v:d["characterValues"].as<JsonArrayConst>()){
        int type=v["typeId"]|0,i=customRow(type,v["valueId"].as<int>());
        if(i<0 || type!=27 || v["value"].isNull()) continue;
        int a=v["value"].as<int>();
        if(a>=1 && a<=6) s.rows[i].ability=a-1; else s.rows[i].flags|=Unresolved;
    }
    for(auto g:groups) for(JsonObjectConst m:d["modifiers"][g].as<JsonArrayConst>()){
        const char *sub=m["subType"]|"";
        if(!active(d,g,m)) continue;
        for(int i=0;i<count;i++) if(applies(sub,i,s.rows[i])) applyModifier(s.rows[i],m,s);
    }
    for(JsonObjectConst v:d["characterValues"].as<JsonArrayConst>()){
        int type=v["typeId"]|0,i=customRow(type,v["valueId"].as<int>());
        if(i<0 || v["value"].isNull()) continue;
        auto &e=s.rows[i];int value=v["value"].as<int>();
        if(type==23 || type==38){
            if(value>=-1000 && value<=1000){e.flags|=Override;e.overrideTotal=value;}
            else e.flags|=Unresolved;
        }else if(type==24 || type==25 || type==39 || type==40) addBonus(e,value);
        else if(type==26 && value>=1 && value<=4) e.training=value-1;
        else if(type==41 && value==1) e.training=0;
        else if(type==26 || type==41) e.flags|=Unresolved;
    }
    // An explicit total takes precedence over otherwise unresolved arithmetic.
    for(auto &e:s.rows) if(e.flags&Override) e.flags &= ~Unresolved;
}
}
