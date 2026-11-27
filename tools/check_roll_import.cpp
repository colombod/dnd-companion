// Exercise the firmware importer with synthetic data. Optional private live files stay outside Git.
#include "../firmware/Companion/sheet.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace checks;
static int mod(const Sheet &s,int row){
    Prepared p(s.rolls,s.level,row);int value;
    assert(p.modifier(value));return value;
}
static void checkLive(const char *path,uint32_t id,const int (&expected)[30]){
    std::ifstream f(path);Sheet s{};assert(f && !parseSheet(f,id,s) && s.rolls.version==1);
    for(int i=0;i<count;i++) {
        int actual=mod(s,i);
        if(actual!=expected[i]){std::cerr<<s.name<<" "<<name(i)<<": "<<actual<<" expected "<<expected[i]<<"\n";std::abort();}
    }
    std::cout<<s.name<<": all 30 modifiers match\n";
}
int main(int argc,char **argv){
    JsonDocument doc;
    auto d=doc.to<JsonObject>();
    for(int i=1;i<=6;i++){auto v=d["stats"].add<JsonObject>();v["id"]=i;v["value"]=10;}
    auto m=d["modifiers"]["feat"].add<JsonObject>();m["type"]="half-proficiency";m["subType"]="ability-checks";
    auto ex=d["modifiers"]["feat"].add<JsonObject>();ex["type"]="expertise";ex["subType"]="history";
    auto condition=d["modifiers"]["feat"].add<JsonObject>();condition["type"]="advantage";
    condition["subType"]="constitution-saving-throws";condition["restriction"]="to maintain Concentration.";
    auto item=d["inventory"].add<JsonObject>();item["definition"]["id"]=22;item["definition"]["canAttune"]=true;
    item["equipped"]=true;item["isAttuned"]=false;
    auto itemMod=d["modifiers"]["item"].add<JsonObject>();itemMod["componentId"]=22;
    itemMod["type"]="bonus";itemMod["subType"]="saving-throws";itemMod["value"]=1;
    auto v=d["characterValues"].add<JsonObject>();v["typeId"]=23;v["valueId"]="6";v["value"]=0;
    Sheet s{};s.level=5;importRolls(d,s.rolls);
    assert(mod(s,0)==1 && mod(s,5)==6 && mod(s,2)==0 && mod(s,saves+2)==0);
    assert((s.rolls.rows[saves+2].flags&Concentration) && !(s.rolls.rows[saves+2].flags&Advantage));
    item["isAttuned"]=true;
    auto unclear=d["modifiers"]["feat"].add<JsonObject>();unclear["type"]="bonus";unclear["subType"]="stealth";unclear["dice"]["diceCount"]=1;
    s.rolls={};importRolls(d,s.rolls);int result=0;
    assert(mod(s,saves)==1 && !Prepared(s.rolls,5,16).modifier(result));
    std::cout<<"Overrides, expertise, half proficiency, attunement, situational advantage and unresolved bonus passed.\n";
    if(argc==5){
        // Inputs are explicitly paired with independent expected vectors by the caller.
        for(int j=1;j<5;j+=2){
            std::ifstream expected(argv[j+1]);uint32_t id=0;int values[30]{};
            assert(expected>>id);for(auto &n:values)assert(expected>>n);
            checkLive(argv[j],id,values);
        }
    }
}
