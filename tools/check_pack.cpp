#include "../firmware/Companion/sheet.h"
#include "../firmware/Companion/session_store.h"
#include <Preferences.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace session;
using pack::Timing;

static Change use(uint32_t id,int hp,Timing timing,pack::Target target=pack::Target::Self){
    Change c{Op::ItemUse};c.itemId=id;c.amount=hp;c.timing=timing;c.target=target;return c;
}
static JsonDocument import(std::istream &in,uint32_t id){
    JsonDocument book;Sheet sheet{};assert(!parseSheet(in,id,sheet,&book));return book;
}
int main(int argc,char **argv){
    std::istringstream json(R"sheet({
  "success": true,
  "data": {
    "id": 7,
    "name": "Example",
    "classes": [
      {
        "level": 5,
        "definition": {
          "name": "Wizard"
        }
      }
    ],
    "actions": {},
    "currencies": {
      "gp": 50,
      "sp": 0
    },
    "inventory": [
      {
        "id": 12,
        "quantity": 3,
        "definition": {
          "id": 400,
          "name": "Potion of Healing",
          "filterType": "Potion",
          "isHomebrew": false,
          "grantedModifiers": [
            {
              "type": "bonus",
              "subType": "hit-points",
              "dice": {
                "diceCount": 2,
                "diceValue": 4,
                "fixedValue": 2
              }
            }
          ]
        }
      },
      {
        "id": 13,
        "quantity": 1,
        "definition": {
          "id": 401,
          "filterType": "Scroll",
          "name": "Spell Scroll (Level 1)"
        },
        "entityTypeId": 1439493548
      },
      {
        "id": 14,
        "quantity": 1,
        "definition": {
          "filterType": "Potion",
          "isHomebrew": true,
          "name": "Potion of Healing",
          "grantedModifiers": [
            {
              "type": "bonus",
              "subType": "hit-points",
              "dice": {
                "diceCount": 2,
                "diceValue": 4,
                "fixedValue": 2
              }
            }
          ]
        }
      },
      {
        "id": 15,
        "quantity": -1,
        "definition": {
          "filterType": "Potion",
          "name": "Bad stack"
        }
      }
    ],
    "spells": {
      "item": [
        {
          "componentId": 401,
          "definition": {
            "id": 9,
            "name": "Scroll spell",
            "level": 1
          },
          "usesSpellSlot": false
        }
      ]
    },
    "characterValues": [
      {
        "typeId": 8,
        "valueId": "13",
        "valueTypeId": "1439493548",
        "value": "Scroll of Light"
      }
    ]
  }
})sheet");
    // A first boot can accept an unused sheet, but must reject an unreadable saved session.
    assert(readable(42));
    Preferences::data["dnd-session/s42"]={0xaa,0xbb};
    assert(!readable(42));
    assert(Preferences::data["dnd-session/s42"].size()==2);
    auto book=import(json,7);auto b=casting::build(book);assert(book["pack"].size()==3);
    assert(book["spells"][0]["costReview"]==true && book["pack"][1]["name"]=="Scroll of Light");
    pack::Item potion,scroll,other;assert(b.bag.item(12,potion) && potion.dice==2 && potion.bonus==2);
    assert(b.bag.item(13,scroll) && !scroll.dice && b.bag.item(14,other) && !other.dice);
    b.removedHP=10;b.limit[0]=3;b.spMax=2;b.sorcerer=2;b.geniusMax=3;
    Change maximum{Op::SetMaxHP};maximum.amount=20;
    assert(!commit(7,b,maximum));assert(!commit(7,b,{Op::StartTurn}));
    assert(!commit(7,b,use(12,7,Timing::Bonus)));
    auto state=[&](){return read(7,b);};
    auto s=state();assert(s.hp.current==17 && pack::stock(s.bag,potion)==2 && s.turn.used==pack::timingMask(Timing::Bonus));
    assert(commit(7,b,use(12,5,Timing::Bonus)) && state().hp.current==17 && pack::stock(state().bag,potion)==2);
    assert(undo(7));s=state();assert(s.hp.current==10 && !s.turn.used && pack::stock(s.bag,potion)==3);
    Preferences::failKey="s7";assert(commit(7,b,use(12,7,Timing::Bonus)));Preferences::failKey="";
    assert(state().hp.current==10 && pack::stock(state().bag,potion)==3);
    sessionfiles::failWrite=true;assert(commit(7,b,use(12,7,Timing::Bonus)));sessionfiles::failWrite=false;
    sessionfiles::corruptWrite=true;assert(commit(7,b,use(12,7,Timing::Bonus)));sessionfiles::corruptWrite=false;
    assert(state().hp.current==10 && pack::stock(state().bag,potion)==3);
    Change hit{Op::TakeHit};hit.amount=10;assert(!commit(7,b,hit));
    assert(commit(7,b,{Op::Cast}) && commit(7,b,{Op::FlashOfGenius}));
    assert(commit(7,b,use(12,4,Timing::Action)) && !state().hp.current);
    assert(!commit(7,b,use(12,4,Timing::Action,pack::Target::Helped)));
    s=state();assert(s.hp.current==4 && !s.turn.used && !s.hp.successes && s.hp.status==vitals::Status::Awake);
    assert(!commit(7,b,use(12,10,Timing::Action,pack::Target::Ally)));assert(state().hp.current==4);
    Change cast{Op::Cast};cast.pool=0;cast.timing=Timing::Action;assert(commit(7,b,cast) && !state().spent[0]);
    cast.timing=Timing::Reaction;assert(!commit(7,b,cast));assert(commit(7,b,{Op::FlashOfGenius}) && !state().geniusSpent);
    assert(!commit(7,b,{Op::StartTurn}) && !state().turn.used);
    assert(!commit(7,b,use(13,0,Timing::Reaction)));assert(state().spent[0]==1 && !pack::stock(state().bag,scroll));
    assert(commit(7,b,use(13,0,Timing::None)));
    Change coin{Op::Coin};coin.pool=0;coin.amount=-12;assert(!commit(7,b,coin));
    coin.amount=-39;assert(commit(7,b,coin));coin.pool=2;coin.amount=1;assert(commit(7,b,coin));
    assert(pack::balance(state().bag,b.bag,0)==38 && pack::balance(read(8,b).bag,b.bag,0)==50);
    assert(!commit(7,b,{Op::LongRest}));s=state();assert(!s.turn.tracking && s.hp.current==20 && !s.spent[0] && pack::stock(s.bag,potion)==1 && !pack::stock(s.bag,scroll));
    coin.pool=0;coin.amount=2;assert(!commit(7,b,coin));
    book["purse"]["gp"]=53;b.bag=pack::source(book);reconcile(7,b);assert(pack::balance(state().bag,b.bag,0)==40 && !undo(7));
    coin.restore=true;assert(!commit(7,b,coin));assert(pack::balance(state().bag,b.bag,0)==53 && undo(7));
    Change more{Op::Stock};more.itemId=12;more.amount=1;assert(!commit(7,b,more));
    book["pack"][0]["qty"]=2;reconcile(7,b);assert(!undo(7));
    book["pack"][0]["qty"]=4;assert(b.bag.item(12,potion));assert(pack::stock(state().bag,potion)==4);
    more.amount=-1;assert(!commit(7,b,more));JsonDocument oldBook;reconcile(7,casting::build(oldBook));
    assert(pack::stock(state().bag,potion)==3);book["pack"].remove(0);reconcile(7,b);assert(!undo(7));
    for(auto &entry:state().bag.edits)assert(entry.id!=12);
    Change loot{Op::Stock};loot.tablePotion=true;loot.amount=2;assert(!commit(8,b,loot));
    auto drink=use(0,6,Timing::Action,pack::Target::Ally);drink.tablePotion=true;
    assert(!commit(8,b,drink) && read(8,b).bag.tablePotions==1);
    assert(!commit(8,b,{Op::LongRest}) && read(8,b).bag.tablePotions==1);
    assert(commit(8,b,use(0,6,Timing::Action)) && !read(7,b).bag.tablePotions);
    pack::Bag full;for(unsigned i=1;i<=10;++i)assert(!pack::adjust(full,{i,1},-1,false));
    assert(pack::adjust(full,{11,1},-1,false) && !pack::adjust(full,{5,1},0,true));
    assert(!pack::adjust(full,{11,1},-1,false));
    struct V5{uint32_t version=5;StoredStateV5 current{},previous{};bool canUndo=true;uint8_t lastOp=uint8_t(Op::Heal);};
    V5 v;v.current.hp.maximum=v.previous.hp.maximum=24;v.current.hp.current=18;v.previous.hp.current=11;v.current.aids.bardic=5;
    Preferences pref;pref.begin("dnd-session");pref.putBytes("s9",&v,sizeof(v));assert(read(9,b).aids.bardic==5 && undo(9) && read(9,b).hp.current==11);
    assert(pref.getBytesLength("s7")==8 && pref.getBytesLength("s9")==8);
    auto key=std::make_pair(uint32_t(9),false);
    sessionfiles::files[key].back()^=1;state(); // Load another character to evict the verified bank from RAM.
    assert(commit(9,b,{Op::LongRest}) && !read(9,b).hp.maximum);
    forget(9);assert(!pref.isKey("s9") && !sessionfiles::files.count(key));
    if(argc==5){
        for(int i=1;i<5;i+=2){
            std::ifstream in(argv[i]);auto live=import(in,std::stoul(argv[i+1]));auto src=pack::source(live);
            if(i==1){assert(src.money[0]==1010 && live["pack"].size()==2 && live["pack"][0]["qty"]==3 && live["pack"][0]["dice"]==2 && live["pack"][1]["basicScroll"]==true);}
            else{assert(src.money[0]==32 && src.money[1]==103 && src.money[2]==50 && src.money[3]==123 && src.money[4]==15 && !live["pack"].size());}
            std::cout<<"Filtered profile: "<<live["pack"].size()<<" carried stacks, "<<src.money[0]<<" GP\n";
        }
    }
    std::cout<<"Pack, wallet, turn economy, refresh, rollback and v5 migration passed. Settings pointer bytes "<<pref.getBytesLength("s7")<<"\n";
}
