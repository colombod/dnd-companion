#pragma once
#include <cstdint>
#include <climits>

namespace pack {
constexpr const char *coins[]={"gp","sp","cp","ep","pp"};
constexpr int coinCount=5;
enum class Timing : uint8_t { None, Action, Bonus, Reaction };
inline Timing activation(int type){
    switch(type){case 1:return Timing::Action;case 3:return Timing::Bonus;case 4:return Timing::Reaction;default:return Timing::None;}
}
inline const char *timingName(Timing t){
    switch(t){case Timing::Action:return "Action";case Timing::Bonus:return "Bonus action";case Timing::Reaction:return "Reaction";default:return "Table timing";}
}
constexpr unsigned timingMask(Timing t){return t==Timing::None?0u:1u<<(unsigned(t)-1);}
struct Turn {bool tracking{};uint8_t used{};};
inline const char *spend(Turn &t,Timing timing){
    if(!t.tracking || timing==Timing::None)return nullptr;
    auto mask=timingMask(timing);
    if(t.used&mask)return "Already spent this turn";
    t.used|=mask;return nullptr;
}
struct Item {uint32_t id{};int stock{},dice{},bonus{};bool local{};};
constexpr Item tablePotion{0,0,2,2,true};
struct Source {
    const void *book{};
    bool (*find)(const void*,uint32_t,Item&){};
    int money[coinCount]{-1,-1,-1,-1,-1};
    bool item(uint32_t id,Item &v)const{return id && find && find(book,id,v);}
};
struct Stack {uint32_t id{};int count{};};
struct Bag {Stack edits[10]{};int balances[coinCount]{};int tablePotions{};uint8_t editedCoins{};};
inline int stock(const Bag &bag,Item item){
    if(item.local)return bag.tablePotions;
    for(auto &v:bag.edits)if(v.id==item.id)return v.count;
    return item.stock;
}
inline const char *adjust(Bag &bag,Item item,int delta,bool reset){
    int64_t next=int64_t(stock(bag,item))+delta;
    if(!reset && (next<0 || next>INT_MAX))return "Check quantity";
    if(item.local){if(reset)return "Check quantity";bag.tablePotions=int(next);return nullptr;}
    Stack *available=nullptr;
    for(auto &v:bag.edits){
        if(v.id==item.id){
            v=reset || next==item.stock?Stack{}:Stack{item.id,int(next)};return nullptr;
        }
        if(!v.id && !available)available=&v;
    }
    if(reset || next==item.stock)return nullptr;
    if(!available)return "Ten stacks edited";
    *available={item.id,int(next)};return nullptr;
}
inline int balance(const Bag &bag,const Source &src,int coin){
    if(coin<0 || coin>=coinCount || src.money[coin]<0)return -1;
    return bag.editedCoins&(1u<<coin)?bag.balances[coin]:src.money[coin];
}
inline const char *pay(Bag &bag,const Source &src,int coin,int delta,bool reset){
    int old=balance(bag,src,coin);if(old<0)return "Balance unavailable";
    int64_t next=int64_t(old)+delta;
    if(!reset && (next<0 || next>INT_MAX))return "Check balance";
    if(reset || next==src.money[coin]){bag.editedCoins&=~(1u<<coin);bag.balances[coin]=0;}
    else {bag.editedCoins|=1u<<coin;bag.balances[coin]=int(next);}
    return nullptr;
}
enum class Target : uint8_t { Self, Ally, Helped };
}
