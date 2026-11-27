#include "../firmware/Companion/sheet.h"
#include "../firmware/Companion/move_roll.h"
#include "../firmware/Companion/spell_rules.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
int main(int argc,char **argv){
    auto dart=moves::readDice("1d4 + 1");
    assert(dart.count==1 && dart.sides==4 && dart.bonus==1);
    assert(moves::readDice("2d10-3").bonus==-3);
    for(auto invalid:{"0d4","1d200","1d4 + 1d6","1d6 +","1d6 + spell mod","101d8"})
        assert(!moves::readDice(invalid).valid());
    {
        checks::Snapshot s{};s.scores[5]=18;
        JsonDocument test;
        deserializeJson(test,R"({"lv":1,"ability":6,"effects":[["2d8","hit-points",true,"2d8",null,null]]})");
        auto cure=casting::effect(test.as<JsonObjectConst>(),s,0,2);
        assert(cure.count==4 && cure.sides==8 && cure.bonus==4);
        deserializeJson(test,R"({"beams":2,"lv":0,"ability":6,"damageBonus":4,"effects":[["1d10","force",false,null,null,null]]})");
        auto crit=casting::effect(test.as<JsonObjectConst>(),s,0,0,true);
        assert(crit.count==2 && crit.sides==10 && crit.bonus==4);
        deserializeJson(test,R"({"missile":true,"lv":1,"effects":[["1d4 + 1","force",false,null,1,null]]})");
        assert(casting::shots(test.as<JsonObjectConst>(),2)==4 && casting::effect(test.as<JsonObjectConst>(),s,0,2).bonus==1);
    }
    {
        using casting::Meta;
        JsonDocument profile,spell;
        deserializeJson(profile,R"({"metamagic":["Twinned Spell","Distant Spell"],"modern":true})");
        deserializeJson(spell,R"({"range":"Ranged","ft":30,"lv":1,"extraTarget":true})");
        auto r=spell.as<JsonObjectConst>();
        auto offers=[&](Meta option,int level){return casting::offersMeta(option,r,profile.as<JsonObjectConst>(),level);};
        assert(offers(Meta::Distant,1) && offers(Meta::Twinned,2) && casting::distantRange(r)==60);
        assert(casting::castLevel(r,10)==2 && casting::castLevel(r,-1)==1);
        assert(!offers(Meta::Twinned,9));
        profile["modern"]=false;assert(!offers(Meta::Twinned,2));
        profile["modern"]=true;spell["extraTarget"]=false;assert(!offers(Meta::Twinned,2));
        spell["range"]="Touch";assert(casting::distantRange(r)==30);
        spell["range"]="Self";assert(!offers(Meta::Distant,1));
        spell["range"]="Ranged";spell["ft"]=4;assert(!offers(Meta::Distant,1));
        profile["metamagic"].clear();spell["ft"]=120;assert(!offers(Meta::Distant,1));
        auto cost=casting::castCost(Meta::Distant,true,-1,0);assert(cost.pool==-1 && cost.points==1);
        cost=casting::castCost(Meta::Twinned,true,10,2);assert(cost.pool==10 && cost.points==1);
        assert(casting::castCost(Meta::None,true,10,2).points==0);
    }
    {
        JsonDocument source,book;
        deserializeJson(source,R"({"classes":[{"isStartingClass":true,"definition":{"name":"Warlock","id":2190885}}],"options":{"class":[{"definition":{"name":"Pact of the Blade"}}]},"inventory":[{"id":7,"equipped":true,"definition":{"filterType":"Weapon","name":"Test blade","type":"Dagger","attackType":1,"categoryId":1,"damage":{"diceString":"1d4"}}},{"equipped":true,"definition":{"name":"Mail","filterType":"Armor","armorClass":14,"armorTypeId":2}},{"equipped":true,"definition":{"name":"Shield","filterType":"Armor","armorClass":2,"armorTypeId":4}}],"modifiers":{"class":[{"type":"proficiency","subType":"shields"}]},"characterValues":[{"typeId":28,"valueId":"7","value":true}]})");
        checks::Snapshot stats{};stats.scores[0]=8;stats.scores[1]=18;stats.scores[5]=20;
        auto import=[&](){book.clear();auto list=book["actions"].to<JsonArray>();equipment::import(source.as<JsonObjectConst>(),book,list,stats,6);};
        import();assert(book["gear"]["ac"]==18 && book["gear"]["armored"]==true && book["gear"]["mageAC"]==0);
        auto bond=[&](uint32_t saved=7){return equipment::resolveBond(book.as<JsonObjectConst>(),saved);};
        auto weapon=[&](){return book["actions"][1].as<JsonObjectConst>();};
        assert(!bond().item && !bond().canChoose(weapon()) && weapon()["toHit"]==-1);
        auto mods=source["modifiers"]["class"].as<JsonArray>();
        auto enable=mods.add<JsonObject>();
        enable["type"]="enable-feature";enable["subType"]="enable-pact-weapon";enable["componentId"]=90;
        source["options"]["class"].clear();import();
        assert(bond().owns(weapon()) && bond().roll(weapon())["toHit"]==2 && bond().roll(weapon())["stat"]=="STR");
        auto replacement=mods.add<JsonObject>();
        replacement["type"]="replace-weapon-ability";replacement["entityId"]=6;replacement["componentId"]=91;
        import();assert(bond().roll(weapon())["stat"]=="STR");
        replacement["componentId"]=90;import();
        assert(bond().roll(weapon())["toHit"]==8 && bond().roll(weapon())["weaponBonus"]==5 && weapon()["toHit"]==-1);
        // Exercise the same bounded JSON filter as the Wi-Fi importer.
        JsonDocument envelope,filter,filtered,roundtrip;
        envelope["data"]=source;
        checks::filterRollData(filter);moves::filter(filter);
        std::stringstream encoded;serializeJson(envelope,encoded);
        assert(!deserializeJson(filtered,encoded,DeserializationOption::Filter(filter)));
        auto list=roundtrip["actions"].to<JsonArray>();
        equipment::import(filtered["data"],roundtrip,list,stats,6);
        auto verified=equipment::resolveBond(roundtrip.as<JsonObjectConst>(),0);
        assert(verified.item==7 && verified.roll(list[1])["stat"]=="CHA");
        enable["isGranted"]=false;import();assert(!bond().item && bond().roll(weapon())["stat"]=="STR");
        enable["isGranted"]=true;enable["subType"]="enable-hex-weapon";
        auto mark=source["characterValues"].as<JsonArray>().add<JsonObject>();
        mark["typeId"]=29;mark["valueId"]="7";mark["value"]=true;
        import();assert(!bond().item && weapon()["toHit"]==-1 && weapon()["hex"].isNull());
        auto proficiency=mods.add<JsonObject>();proficiency["type"]="proficiency";proficiency["subType"]="simple-weapons";
        import();assert(!bond().item && weapon()["hex"]==true && weapon()["toHit"]==8);
        auto property=source["inventory"][0]["definition"]["properties"].to<JsonArray>().add<JsonObject>();property["name"]="Two-Handed";
        source["inventory"][2]["equipped"]=false;import();
        assert(weapon()["stat"]=="STR" && weapon()["hex"].isNull());
        source["inventory"][2]["equipped"]=true;
        source["inventory"][0]["definition"].remove("properties");
        source["inventory"][1]["definition"]["armorTypeId"]=3;import();assert(book["gear"]["ac"]==16);
        source["inventory"][1]["equipped"]=false;import();assert(book["gear"]["ac"]==16 && book["gear"]["mageAC"]==19);
        source["modifiers"]["class"].clear();import();assert(book["gear"]["ac"]==14 && book["gear"]["untrainedShield"]==true);
        source["options"]["class"].clear();import();assert(!bond().item && !bond().canChoose(weapon()) && weapon()["toHit"]==-1);
    }
    {
        JsonDocument source;
        deserializeJson(source,R"({"success":true,"data":{
          "id":9,"name":"Warder","stats":[{"id":2,"value":14}],"modifiers":{},"inventory":[],
          "classes":[{"level":2,"definition":{"name":"Sorcerer","id":2190884}},
                     {"level":4,"definition":{"name":"Warlock","id":2190885}}],
          "options":{"class":[{"definition":{"id":501,"name":"Armor of Shadows"}}]},
          "spells":{"class":[{"componentId":501,"usesSpellSlot":false,
                              "definition":{"id":2619006,"level":1,"name":"Mage Armor"}}]}}})");
        auto verify=[&](int manual){
            std::stringstream input;serializeJson(source,input);
            Sheet sheet{};JsonDocument book;
            assert(!parseSheet(input,9,sheet,&book));
            int grant=casting::atWillMageArmor(book.as<JsonObjectConst>());
            assert(grant==0);
            auto b=casting::build(book);
            assert(b.baseAC==12 && b.mageAC==15 && b.miscAC==manual);
            auto s=session::initial(b);
            auto cast=casting::spellCast(book["spells"][grant],casting::Meta::None,b.modern,-1,true);
            assert(cast.ward==session::Ward::MageArmor && cast.pool==-1 && !cast.points);
            assert(session::armorClass(b,s)==12 && !session::apply(b,s,cast));
            assert(session::armorClass(b,s)==15 && session::points(b,s)==2);
            assert(!session::apply(b,s,cast) && session::armorClass(b,s)==15 && s.mageRounds==4800);
            assert(!session::apply(b,s,{session::Op::Elapse,-1,0,false,session::Ward::None,4800}));
            assert(session::armorClass(b,s)==12);
            auto spell=book["spells"][grant];
            spell["costReview"]=true;assert(casting::atWillMageArmor(book.as<JsonObjectConst>())==-1);
            spell.remove("costReview");spell["usesSlot"]=true;
            assert(casting::atWillMageArmor(book.as<JsonObjectConst>())==-1);
            spell["usesSlot"]=false;book["gear"]["armored"]=true;
            assert(casting::atWillMageArmor(book.as<JsonObjectConst>())==-1);
        };
        verify(0);
        auto adjustment=source["data"]["characterValues"].to<JsonArray>().add<JsonObject>();
        adjustment["typeId"]=3;adjustment["value"]=3;
        verify(3);
    }
    {
        JsonDocument book;
        deserializeJson(book,R"({"schema":4,"gear":{"pactEnabled":true,"pactMarks":2},"actions":[
          {"weapon":true,"itemId":7,"toHit":1,"pactRoll":{"toHit":5}},
          {"weapon":true,"itemId":8,"toHit":2,"pactRoll":{"toHit":6}}]})");
        auto rows=book["actions"].as<JsonArray>();
        auto resolve=[&](uint32_t id){return equipment::resolveBond(book.as<JsonObjectConst>(),id);};
        assert(resolve(0).marked==2 && !resolve(0).item);
        auto chosen=resolve(7);
        assert(chosen.owns(rows[0]) && !chosen.owns(rows[1]));
        assert(chosen.roll(rows[0])["toHit"]==5 && chosen.roll(rows[1])["toHit"]==2);
        assert(resolve(8).owns(rows[1]) && !resolve(8).owns(rows[0]));
        rows[0].remove("pactRoll");assert(!resolve(7).item);
        book["gear"]["pactMarks"]=1;assert(resolve(7).item==8 && !resolve(7).canChoose(rows[1]));
        book["gear"]["pactEnabled"]=false;assert(!resolve(8).item && !resolve(8).canChoose(rows[1]) && resolve(8).roll(rows[1])["toHit"]==2);
        book.remove("gear");book["schema"]=3;rows[1]["pact"]="Bonded";
        assert(!resolve(8).item && resolve(8).roll(rows[1]).isNull() && casting::playable(book["schema"]|0u));
    }
    JsonDocument def,output;
    deserializeJson(def,R"({"definition":{"id":2619161,"name":"Eldritch Blast","level":0,"requiresAttackRoll":true,"modifiers":[{"subType":"force","die":{"diceString":"1d10"}}]}})");
    for(int level:{1,5,11,17}){
        auto list=output.to<JsonArray>();
        moves::addSpell(list,def.as<JsonObjectConst>(),"Caster",6,level,false);
        assert(list[0]["beams"]==(1+(level>=5)+(level>=11)+(level>=17)));
        assert(list[0]["effects"][0][0]=="1d10" && list[0]["hit"]==true);
    }
    for(int j=1;j+1<argc;j+=2){
        std::ifstream source(argv[j]);assert(source);
        Sheet sheet{};JsonDocument book;
        const char *error=parseSheet(source,std::stoul(argv[j+1]),sheet,&book);
        if(error){std::cerr<<error<<"\n";return 1;}
        assert(!book.overflowed() && measureMsgPack(book)<=96*1024);
        std::cout<<sheet.name<<": "<<book["spells"].size()<<" spell entries, "<<book["actions"].size()<<" actions, "<<measureMsgPack(book)<<" bytes\n";
        for(auto r:book["spells"].as<JsonArray>()){
            assert(r["name"].is<const char *>());
            if(r["beams"].is<int>())std::cout<<"  "<<r["name"].as<const char *>()<<" beams="<<r["beams"].as<int>()<<"\n";
        }
        if(argc==3)serializeJson(book,std::cout);
    }
}
