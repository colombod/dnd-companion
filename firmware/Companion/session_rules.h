#pragma once
#include "vitals.h"
#include "pack_rules.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
namespace session {
constexpr int pools=18;
enum class Op { Cast, SlotToPoints, PointsToSlot, Metamagic, ShortRest, LongRest, TakeHit, Elapse, GrantTemp, UseMisc, FlashOfGenius, SetMaxHP, Heal, DeathSave, Stabilize, Revive, SetAid, UseAid, Stock, Coin, ItemUse, StartTurn, EndTracking, MarkTurn, SpendTurn };
enum class Ward { None, MageArmor, Agathys, Shield };
enum class TempChoice { Unspecified, Keep, Take };
struct Build {
    pack::Source bag{};
    uint8_t limit[pools]{},spent[pools]{},sorcerer{},spMax{},spUsed{};
    bool modern{},known=true,armored{};
    bool forgeKnown{},geniusUnresolved{},inkUnresolved{};
    uint8_t geniusMax{},geniusUsed{},geniusRecovery{},inkMax{},inkUsed{};
    int baseAC{},mageAC{},sourceTemp{},miscAC{};
    int hpHint{},removedHP{},sourceSuccess{},sourceFail{};bool sourceStable{},revisedRest{};
};
struct StoredStateV3{
    uint8_t spent[pools]{},created[5]{},spUsed{};
    bool restored{};
    uint16_t temp{},mageRounds{};
    // Same-level castings have equal potency and duration. Only the most recent can outlast the others.
    uint16_t agathysRounds[9]{};
    bool shieldSpell{},tempKnown{},useMisc{};
};
struct StoredStateV4 : StoredStateV3 {
    uint8_t geniusSpent{},inkSpent{},featureFlags{};
};
struct StoredStateV5 : StoredStateV4 {vitals::Health hp{};vitals::Aids aids{};};
struct State : StoredStateV5 {pack::Bag bag{};pack::Turn turn{};};
struct Change{Op op;int pool=-1;int points=0;bool restore=false; Ward ward=Ward::None; int amount=0; bool melee=false; TempChoice tempChoice=TempChoice::Unspecified; bool mappingMagic=false; bool critical=false; int recoveryHours=0; vitals::Roll roll=vitals::Roll::Ability; int skill=-1;uint32_t itemId{};pack::Timing timing=pack::Timing::None;pack::Target target=pack::Target::Self;bool tablePotion=false;};
inline void reconcileUses(const Build &b,State &s){
    if(!b.forgeKnown)return;
    auto sync=[&](unsigned bit,uint8_t &spent,uint8_t maximum,uint8_t imported){
        spent=std::min(maximum,s.featureFlags&bit?spent:imported);
        if(maximum)s.featureFlags|=bit;else s.featureFlags&=~bit;
    };
    if(!b.geniusUnresolved)sync(1,s.geniusSpent,b.geniusMax,b.geniusUsed);
    if(!b.inkUnresolved)sync(2,s.inkSpent,b.inkMax,b.inkUsed);
}
inline int geniusLeft(const Build &b,const State &s){return std::max(0,int(b.geniusMax)-s.geniusSpent);}
inline int inkLeft(const Build &b,const State &s){return std::max(0,int(b.inkMax)-s.inkSpent);}
inline State initial(const Build &b){
    State s{};memcpy(s.spent,b.spent,pools);s.spUsed=b.spUsed;s.temp=std::clamp(b.sourceTemp,0,65535);s.tempKnown=true;reconcileUses(b,s);return s;
}
constexpr int level(int pool){return pool%9+1;}
inline int remaining(const Build &b,const State &s,int pool){
    if(pool<0 || pool>=pools)return 0;
    return std::max(0,int(b.limit[pool])-s.spent[pool])+(pool<5?s.created[pool]:0);
}
struct SlotCount {int left{},used{};};
inline SlotCount slotCount(const Build &b,const State &s,bool pact,int minLevel=1){
    SlotCount count;
    for(int lv=std::max(1,minLevel);lv<=9;lv++){
        int p=(pact?9:0)+lv-1;
        count.left+=remaining(b,s,p);
        count.used+=std::min(b.limit[p],s.spent[p]);
    }
    return count;
}
inline int points(const Build &b,const State &s){return std::max(0,int(b.spMax)-s.spUsed);}
inline int creationCost(int lv){constexpr int cost[]={0,2,3,5,6,7};return lv>=1 && lv<=5?cost[lv]:0;}
inline bool canCreate(const Build &b,int lv){
    constexpr int minLevel[]={0,2,3,5,7,9};return lv>=1 && lv<=5 && b.sorcerer>=2 && (!b.modern || b.sorcerer>=minLevel[lv]);
}
inline int metamagicCost(const char *name,bool modern,int spellLevel){
    if(!strcmp(name,"Twinned Spell"))return modern?1:std::max(1,spellLevel);
    if(!strcmp(name,"Quickened Spell"))return 2;
    if(!strcmp(name,"Heightened Spell"))return modern?2:3;
    if(!strcmp(name,"Seeking Spell"))return modern?1:2;
    for(auto n:{"Careful Spell","Distant Spell","Empowered Spell","Extended Spell","Subtle Spell","Transmuted Spell"})
        if(!strcmp(name,n))return 1;
    return 0;
}
inline bool useSlot(const Build &b,State &s,int p){
    if(!b.known || !remaining(b,s,p))return false;
    if(p<5 && s.created[p])--s.created[p];else ++s.spent[p];
    return true;
}
inline int armorClass(const Build &b,const State &s){
    if(!b.baseAC)return 0;
    return std::max(b.baseAC,s.mageRounds && !b.armored?b.mageAC:0)+(s.shieldSpell?5:0)+(s.useMisc?b.miscAC:0);
}
inline int agathysLevel(const State &s){
    for(int lv=9;lv>=1;--lv)if(s.agathysRounds[lv-1])return lv;
    return 0;
}
inline int agathysOffer(const State &s,int lv){
    return lv>=1 && lv<=9 && lv>=agathysLevel(s)?5*lv:0;
}
inline bool needsTempChoice(const State &s,int offer){return offer>0 && s.temp>0;}
inline int retaliation(const State &s,bool melee){return melee && s.temp?5*agathysLevel(s):0;}
inline void setTemp(State &s,int amount){
    s.temp=amount;
    if(!s.temp)memset(s.agathysRounds,0,sizeof(s.agathysRounds));
}
inline void elapse(State &s,int rounds){
    s.mageRounds=std::max(0,int(s.mageRounds)-rounds);
    for(auto &remaining:s.agathysRounds)remaining=std::max(0,int(remaining)-rounds);
    s.shieldSpell=false;
    vitals::elapse(s.aids,rounds);
    vitals::elapse(s.hp,rounds);
}
// Returns an error without mutating state. Storage commits the successful next state atomically.
inline const char *apply(const Build &b,State &state,Change c){
    State s=state;s.spUsed=std::min(s.spUsed,b.spMax);int p=c.pool,sp=points(b,s);
    bool unable=s.hp.maximum && !s.hp.current;
    if(unable && (c.op==Op::Cast || c.op==Op::FlashOfGenius || c.op==Op::SpendTurn ||
                  c.op==Op::PointsToSlot || c.op==Op::SlotToPoints))return "Unable to act";
    switch(c.op){
    case Op::StartTurn:s.turn={true,0};s.shieldSpell=false;break;
    case Op::EndTracking:s.turn={};break;
    case Op::SpendTurn:
        if(!s.turn.tracking || c.timing==pack::Timing::None)return "Start turn tracking";
        if(auto e=pack::spend(s.turn,c.timing))return e;
        break;
    case Op::MarkTurn:
        if(!s.turn.tracking || c.timing==pack::Timing::None)return "Start turn tracking";
        s.turn.used^=pack::timingMask(c.timing);break;
    case Op::Coin:if(auto error=pack::pay(s.bag,b.bag,p,c.amount,c.restore))return error;break;
    case Op::Stock:case Op::ItemUse:{
        pack::Item item;
        if(c.tablePotion && !c.itemId)item=pack::tablePotion;
        else if(!b.bag.item(c.itemId,item))return "Item unavailable";
        if(c.op==Op::Stock){if(auto e=pack::adjust(s.bag,item,c.amount,c.restore))return e;break;}
        if(pack::stock(s.bag,item)<1)return "Item unavailable";
        bool helped=c.target==pack::Target::Helped;
        if(!helped && s.hp.maximum && !s.hp.current)return "Unable to act";
        if(item.dice){
            if(c.timing!=pack::Timing::Action && c.timing!=pack::Timing::Bonus)return "Check potion timing";
            if(c.amount<item.dice+item.bonus || c.amount>4*item.dice+item.bonus)return "Check healing";
            if(c.target!=pack::Target::Ally){
                if(!s.hp.maximum)return "Set maximum HP first";
                if(auto e=vitals::heal(s.hp,c.amount))return e;
            }
        }else if(helped || c.amount)return "Check item use";
        if(!helped)if(auto e=pack::spend(s.turn,c.timing))return e;
        if(auto e=pack::adjust(s.bag,item,-1,false))return e;
        break;
    }
    case Op::SetMaxHP:{
        if(c.amount<1 || c.amount>vitals::limit)return "Check maximum HP";
        auto &h=s.hp;
        if(!h.maximum){
            h.current=std::clamp(c.amount-b.removedHP,0,c.amount);
            if(!h.current){
                vitals::enter(h,vitals::Status::Dying);
                h.successes=std::clamp(b.sourceSuccess,0,3);h.failures=std::clamp(b.sourceFail,0,3);
                if(h.failures==3)h.status=vitals::Status::Dead;
                else if(b.sourceStable || h.successes==3)vitals::enter(h,vitals::Status::Stable);
            }
        }else h.current=std::min(int(h.current),c.amount);
        h.maximum=c.amount;break;
    }
    case Op::DeathSave:if(auto e=vitals::save(s.hp,c.amount,c.points,c.recoveryHours))return e;break;
    case Op::Heal:if(auto e=vitals::heal(s.hp,c.amount))return e;break;
    case Op::Stabilize:if(auto e=vitals::stabilize(s.hp,c.recoveryHours))return e;break;
    case Op::Revive:
        if(s.hp.status!=vitals::Status::Dead || !s.hp.maximum)return "Check revival";
        s.hp.current=1;vitals::enter(s.hp,vitals::Status::Awake);break;
    case Op::UseAid:{
        int bonus=0;
        if(c.pool<0 || c.pool>=vitals::AidCount || !vitals::useDie(s.aids,vitals::Aid(c.pool),c.roll,c.skill,c.amount,bonus))return "Check effect";
        break;
    }
    case Op::SetAid:if(auto e=vitals::setAid(s.aids,c.pool,c.amount))return e;break;
    case Op::FlashOfGenius:
        if(!geniusLeft(b,s))return "No uses remaining";
        if(auto e=pack::spend(s.turn,pack::Timing::Reaction))return e;
        ++s.geniusSpent;
        break;
    case Op::Cast:
        if(auto e=pack::spend(s.turn,c.timing))return e;
        if(c.mappingMagic){
            if(p!=-1 || c.amount!=1 || !inkLeft(b,s))return "No uses remaining";
            ++s.inkSpent;
        }
        if(p!= -1 && !useSlot(b,s,p))return "Slot unavailable";
        if(c.points<0 || c.points>sp)return "Not enough points";
        s.spUsed+=c.points;
        if(c.ward==Ward::MageArmor){
            if(b.armored)return "Remove worn armor first";
            s.mageRounds=4800;
        }else if(c.ward==Ward::Agathys){
            if(c.amount<1 || c.amount>9 || (p>=0 && c.amount!=level(p)))return "Check spell level";
            int offer=agathysOffer(s,c.amount);
            if(needsTempChoice(s,offer) && c.tempChoice==TempChoice::Unspecified)return "Choose temporary HP";
            if(offer && (!s.temp || c.tempChoice==TempChoice::Take))setTemp(s,offer);
            s.agathysRounds[c.amount-1]=600;
        }else if(c.ward==Ward::Shield)s.shieldSpell=true;
        break;
    case Op::SlotToPoints:
        if(!b.modern)if(auto e=pack::spend(s.turn,pack::Timing::Bonus))return e;
        if(b.sorcerer<2 || !b.spMax)return "Font of Magic needed";
        if(sp==b.spMax)return "Points already full";
        if(!useSlot(b,s,p))return "Slot unavailable";
        s.spUsed=std::max(0,int(s.spUsed)-level(p));break;
    case Op::PointsToSlot:{
        if(auto e=pack::spend(s.turn,pack::Timing::Bonus))return e;
        if(p<0 || p>=5 || !canCreate(b,p+1))return "Cannot create this level";
        int cost=creationCost(p+1);
        if(sp<cost)return "Not enough points";
        if(s.created[p]>=250)return "Created slot limit";
        s.spUsed+=cost;++s.created[p];break;
    }
    case Op::Metamagic:
        if(c.points<1 || c.points>sp)return "Not enough points";
        s.spUsed+=c.points;break;
    case Op::ShortRest:
        if(s.hp.maximum && !s.hp.current && (b.revisedRest || s.hp.status!=vitals::Status::Stable))return "Need HP to start rest";
        s.turn={};
        s.geniusSpent=std::max(0,int(s.geniusSpent)-b.geniusRecovery);
        elapse(s,600);memset(s.spent+9,0,9);
        if(b.modern && c.restore && b.sorcerer>=5 && !s.restored && s.spUsed){
            s.spUsed=std::max(0,int(s.spUsed)-b.sorcerer/2);s.restored=true;
        }else if(!b.modern && b.sorcerer>=20)s.spUsed=std::max(0,int(s.spUsed)-4);
        break;
    case Op::LongRest:{
        if(s.hp.maximum && !s.hp.current)return "Need HP to start rest";
        auto hp=s.hp;hp.current=hp.maximum;bool misc=s.useMisc;auto flags=s.featureFlags;
        auto bag=s.bag;s={};s.bag=bag;s.hp=hp;s.featureFlags=flags;s.tempKnown=true;s.useMisc=misc;break;
    }
    case Op::TakeHit:
        if(c.amount<0 || c.amount>vitals::limit)return "Check damage";
        vitals::hit(s.hp,c.amount,std::min(c.amount,int(s.temp)),c.critical);
        setTemp(s,std::max(0,int(s.temp)-c.amount));
        break;
    case Op::GrantTemp:
        if(c.amount<0 || c.amount>1000)return "Check temporary HP";
        setTemp(s,c.amount);break;
    case Op::UseMisc:s.useMisc=!s.useMisc;break;
    case Op::Elapse:
        if(c.amount<1 || c.amount>4800)return "Check game time";
        elapse(s,c.amount);break;
    }
    state=s;return nullptr;
}
}
