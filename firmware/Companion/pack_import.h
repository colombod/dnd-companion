#pragma once
#include "pack_rules.h"
#include <ArduinoJson.h>
#include <cstring>
namespace pack {
inline void filter(JsonDocument &f){
    auto d=f["data"];auto i=d["inventory"][0];i["quantity"]=i["entityTypeId"]=true;
    d["characterValues"][0]["valueTypeId"]=true;
    auto w=i["definition"];w["id"]=w["isHomebrew"]=true;
    auto m=w["grantedModifiers"][0];
    m["type"]=m["subType"]=m["restriction"]=m["isGranted"]=m["tagConstraints"]=true;
    auto dice=m["dice"];dice["diceCount"]=dice["diceValue"]=dice["fixedValue"]=true;
    for(auto key:coins)d["currencies"][key]=true;
}
inline bool consumable(JsonVariantConst def){return def["filterType"]=="Potion" || def["filterType"]=="Scroll";}
inline void import(JsonObjectConst data,JsonDocument &book){
    if(data["currencies"].is<JsonObjectConst>()){
        auto purse=book["purse"].to<JsonObject>();
        for(auto key:coins){
            auto n=data["currencies"][key];
            if(n.is<int>() && n.as<int>()>=0)purse[key]=n;
        }
    }
    if(!data["inventory"].is<JsonArrayConst>())return;
    auto items=book["pack"].to<JsonArray>();
    for(auto i:data["inventory"].as<JsonArrayConst>()){
        auto def=i["definition"];
        if(!consumable(def) || !i["id"].is<uint32_t>() || !i["id"].as<uint32_t>() ||
           !i["quantity"].is<int>() || i["quantity"].as<int>()<0 || !def["name"].is<const char*>())continue;
        auto row=items.add<JsonObject>();row["id"]=i["id"];row["name"]=def["name"];row["qty"]=i["quantity"];
        for(auto v:data["characterValues"].as<JsonArrayConst>()){
            if(v["typeId"]!=8 || v["valueId"].as<uint32_t>()!=i["id"].as<uint32_t>() ||
               v["valueTypeId"].as<uint32_t>()!=i["entityTypeId"].as<uint32_t>())continue;
            const char *label=v["value"]|"";
            if(*label){row["name"]=label;break;}
        }
        row["scroll"]=def["filterType"]=="Scroll";
        if(def["isHomebrew"]!=false)continue;
        if(row["scroll"]==true){
            if(def["id"]==8960642 || def["id"]==8960643)row["basicScroll"]=true;
            continue;
        }
        const char *name=def["name"];
        // Only standard healing potions with a matching published formula get an automatic HP transaction.
        for(auto m:def["grantedModifiers"].as<JsonArrayConst>()){
            auto dice=m["dice"];int n=dice["diceCount"]|0, bonus=dice["fixedValue"]|0;
            if(m["type"]!="bonus" || m["subType"]!="hit-points" || m["isGranted"]==false ||
               (m["restriction"]|"")[0] || m["tagConstraints"].size() || dice["diceValue"]!=4)continue;
            if((n==2 && bonus==2 && !strcmp(name,"Potion of Healing")) ||
               (n==4 && bonus==4 && (!strcmp(name,"Potion of Greater Healing") || !strcmp(name,"Potion of Healing (Greater)"))) ||
               (n==8 && bonus==8 && (!strcmp(name,"Potion of Superior Healing") || !strcmp(name,"Potion of Healing (Superior)"))) ||
               (n==10 && bonus==20 && (!strcmp(name,"Potion of Supreme Healing") || !strcmp(name,"Potion of Healing (Supreme)")))){
                row["dice"]=n;row["bonus"]=bonus;
            }
        }
    }
}
inline Item item(JsonVariantConst r){return {r["id"]|0u,r["qty"]|0,r["dice"]|0,r["bonus"]|0};}
inline bool lookup(const void *ptr,uint32_t id,Item &it){
    auto &book=*static_cast<const JsonDocument*>(ptr);
    for(auto r:book["pack"].as<JsonArrayConst>())if(r["id"]==id){it=item(r);return true;}
    return false;
}
inline Source source(const JsonDocument &book){
    Source s;
    if(book["pack"].is<JsonArrayConst>()){s.book=&book;s.find=lookup;}
    for(int i=0;i<coinCount;++i)s.money[i]=book["purse"][coins[i]]|-1;
    return s;
}
}
