#include "../firmware/Companion/sheet.h"
#include "../firmware/Companion/spell_rules.h"
#include "../firmware/Companion/session_store.h"
#include <Preferences.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace session;

// Synthetic public fixture. It goes through the same bounded filter as a linked sheet.
static const char *fixture=R"({
 "success":true,"data":{"id":1,"name":"Mapmaker","modifiers":{"class":[]},"stats":[
 {"id":1,"value":10},{"id":2,"value":12},{"id":3,"value":14},
 {"id":4,"value":18},{"id":5,"value":10},{"id":6,"value":8}],
 "classes":[{"id":10,"level":20,"definition":{"name":"Artificer","id":2656866,"spellCastingAbilityId":4},
 "classFeatures":[{"definition":{"id":12497132,"name":"Flash of Genius","requiredLevel":7}},
 {"definition":{"id":12497135,"name":"Advanced Artifice","requiredLevel":14}},
 {"definition":{"id":12497141,"name":"Soul of Artifice","requiredLevel":20}},
 {"definition":{"id":12497181,"name":"Cartographer Spells","requiredLevel":3}},
 {"definition":{"id":12497183,"name":"Mapping Magic","requiredLevel":3}}]}],
 "actions":{"class":[{"name":"Flash of Genius","componentId":12497132,"limitedUse":{"numberUsed":2,"resetType":2}}]},
 "spells":{"class":[{"definition":{"id":2618858,"name":"Faerie Fire","level":1},"componentId":12497181,"alwaysPrepared":true,"usesSpellSlot":true},
 {"definition":{"id":2618858,"name":"Faerie Fire","level":1},"componentId":12497183,"alwaysPrepared":true,"usesSpellSlot":false,"limitedUse":{"resetType":2,"numberUsed":1}}]},
 "inventory":[{"isAttuned":true,"definition":{"canAttune":true}}]}})";

static Build parse(JsonDocument &input,JsonDocument &book){
    std::stringstream stream;serializeJson(input,stream);
    Sheet sheet{};book.clear();
    assert(!parseSheet(stream,1,sheet,&book));
    return casting::build(book);
}
static void grantRules(){
    JsonDocument src,book;assert(!deserializeJson(src,fixture));
    auto d=src["data"];auto cls=d["classes"][0];
    auto b=parse(src,book);
    assert(b.geniusMax==4 && b.geniusRecovery==4 && b.inkMax==4);
    assert(initial(b).geniusSpent==2 && initial(b).inkSpent==1);
    auto ordinary=book["spells"][0],free=book["spells"][1];
    assert(ordinary["src"]=="Cartographer Spells" && ordinary["ability"]==4 && ordinary["mappingMagic"].isNull());
    assert(free["src"]=="Mapping Magic" && free["mappingMagic"]==true && free["costReview"].isNull());
    auto cast=casting::spellCast(free,casting::Meta::None,false,-1,true);
    auto s=initial(b);assert(cast.mappingMagic && !apply(b,s,cast) && s.inkSpent==2 && !s.spent[0]);
    b.limit[0]=2;auto paid=casting::spellCast(ordinary,casting::Meta::None,false,0,true);
    assert(!paid.mappingMagic && !apply(b,s,paid) && s.inkSpent==2 && s.spent[0]==1);
    assert(!apply(b,s,cast) && !apply(b,s,cast) && apply(b,s,cast));
    assert(!apply(b,s,{Op::FlashOfGenius}) && !apply(b,s,{Op::FlashOfGenius}) && apply(b,s,{Op::FlashOfGenius}));
    assert(!apply(b,s,{Op::ShortRest}) && s.geniusSpent==0 && s.inkSpent==4 && s.spent[0]==1);
    assert(!apply(b,s,{Op::LongRest}) && s.inkSpent==0 && s.spent[0]==0);
    d["inventory"][0]["isAttuned"]=false;b=parse(src,book);assert(b.geniusRecovery==1);
    d["inventory"][0]["isAttuned"]=true;cls["level"]=14;b=parse(src,book);assert(b.geniusRecovery==1);
    s=initial(b);assert(!apply(b,s,{Op::ShortRest}) && s.geniusSpent==1);
    cls["level"]=7;b=parse(src,book);assert(!b.geniusRecovery && b.geniusMax==4);
    cls["level"]=6;b=parse(src,book);assert(!b.geniusMax && b.inkMax==4);
    cls["level"]=2;b=parse(src,book);assert(!b.inkMax && book["spells"].size()==0);
    cls["level"]=7;d["stats"][3]["value"]=8;b=parse(src,book);assert(b.geniusMax==1 && b.inkMax==1);
    cls["definition"]["id"]=252717;b=parse(src,book);
    assert(!b.geniusMax && !b.inkMax && book["spells"][1]["costReview"]==true);
    cls["definition"]["id"]=2656866;d["spells"]["class"][1]["restriction"]="Check table";
    b=parse(src,book);assert(!b.inkMax && b.inkUnresolved && book["spells"][1]["costReview"]==true);
    d["spells"]["class"][0].remove("usesSpellSlot");parse(src,book);assert(book["spells"][0]["costReview"]==true);
}
static void progression(){
    JsonDocument d,book;
    auto classes=d["classes"].to<JsonArray>();
    auto caster=[&](const char *name,int lv,int divisor,int rounding){
        auto c=classes.add<JsonObject>();c["level"]=lv;
        auto def=c["definition"];def["name"]=name;def["canCastSpells"]=true;
        auto rules=def["spellRules"];rules["multiClassSpellSlotDivisor"]=divisor;rules["multiClassSpellSlotRounding"]=rounding;
        rules["levelSpellSlots"][lv][0]=2;return def.as<JsonObject>();
    };
    auto half=caster("Artificer",3,2,2);
    caster("Wizard",2,1,1);
    auto pact=caster("Warlock",3,1,1);pact["spellRules"]["levelSpellSlots"][3][0]=0;pact["spellRules"]["levelSpellSlots"][3][1]=2;
    auto build=[&](){book.clear();casting::resources(d.as<JsonObjectConst>(),book);return casting::build(book);};
    auto b=build();assert(b.known && b.limit[0]==4 && b.limit[1]==3 && b.limit[10]==2);
    half["name"]="Paladin";half["spellRules"]["multiClassSpellSlotRounding"]=1;
    b=build();assert(b.known && b.limit[1]==2);
    half["spellRules"]["multiClassSpellSlotRounding"]=2;b=build();assert(b.limit[1]==3);
    half["spellRules"]["multiClassSpellSlotRounding"]=9;b=build();assert(!b.known);
    half["canCastSpells"]=false;b=build();assert(b.known && b.limit[0]==2 && !b.limit[1] && b.limit[10]==2);
}
static void storage(){
    struct OldRecord{uint32_t version;StoredStateV3 current,previous;bool canUndo;uint8_t op;};
    static_assert(sizeof(OldRecord)==112);
    OldRecord old{};old.version=3;old.current.spent[0]=1;old.previous.temp=5;old.previous.tempKnown=true;old.canUndo=true;
    auto &raw=Preferences::data["dnd-session/s55"];auto p=reinterpret_cast<const uint8_t *>(&old);raw.assign(p,p+sizeof(old));
    Build b;b.forgeKnown=true;b.geniusMax=b.inkMax=4;b.geniusUsed=1;b.inkUsed=2;b.limit[0]=3;
    auto s=read(55,b);assert(s.spent[0]==1 && s.geniusSpent==1 && s.inkSpent==2);
    reconcile(55,b);assert(undo(55) && read(55,b).temp==5);
    assert(!commit(55,b,{Op::FlashOfGenius}) && read(55,b).geniusSpent==2);
    b.geniusUsed=0;reconcile(55,b);assert(read(55,b).geniusSpent==2);
    assert(!commit(55,b,{Op::LongRest}) && !read(55,b).inkSpent && !read(55,b).geniusSpent);
    assert(undo(55) && read(55,b).geniusSpent==2 && read(55,b).inkSpent==2);
    assert(!read(56,b).geniusSpent);
    Build partial=b;partial.geniusMax=0;partial.geniusUnresolved=true;
    reconcile(55,partial);assert(!geniusLeft(partial,read(55,partial)) && read(55,b).geniusSpent==2);
    partial.geniusUnresolved=false;reconcile(55,partial);assert(!read(55,b).geniusSpent);
}
int main(int argc,char **argv){
    grantRules();progression();storage();
    if(argc==2){
        std::ifstream source(argv[1]);Sheet s{};JsonDocument book;
        assert(!parseSheet(source,156420740,s,&book));
        auto b=casting::build(book);
        assert(s.level==20 && b.geniusMax==5 && b.inkMax==5 && b.geniusRecovery==5);
        assert(b.limit[0]==4 && b.limit[1]==3 && b.limit[4]==2 && !b.limit[5]);
        assert(book["guidedPrecision"]==true);
        int slot=0,free=0,genius=0;
        for(auto spell:book["spells"].as<JsonArrayConst>()){
            if(spell["name"]=="Guiding Bolt")assert(spell["precisionDamage"]==true);
            if(spell["name"]=="Healing Word")assert(spell["precisionDamage"].isNull());
        }
        for(auto spell:book["spells"].as<JsonArrayConst>())if(spell["name"]=="Faerie Fire"){
            assert(spell["ability"]==4 && spell["protectedFire"]==true);
            slot+=spell["usesSlot"]==true;free+=spell["mappingMagic"]==true;
        }
        for(auto action:book["actions"].as<JsonArrayConst>())genius+=action["genius"]==true;
        assert(slot==1 && free==1 && genius==1);
    }
    std::cout<<"Artificer import, eligibility, rests, edition-specific rounding and saved-state migration passed.\n";
}
