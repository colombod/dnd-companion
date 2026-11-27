#pragma once
#include <algorithm>
#include <cstdint>

namespace vitals {
enum class Status : uint8_t { Awake, Dying, Stable, Dead };
struct Health {
    uint16_t maximum{},current{},recoveryRounds{};
    uint8_t successes{},failures{};
    Status status=Status::Awake;
};
constexpr int limit=9999;
inline void enter(Health &h,Status status){
    h.status=status;h.successes=h.failures=0;h.recoveryRounds=0;
}
inline void failures(Health &h,int count){
    h.failures=std::min(3,int(h.failures)+count);
    if(h.failures==3)h.status=Status::Dead;
}
inline void hit(Health &h,int damage,int absorbed,bool critical){
    if(!h.maximum || !damage || h.status==Status::Dead)return;
    if(!h.current){
        if(damage>=h.maximum){enter(h,Status::Dead);return;}
        h.status=Status::Dying;h.recoveryRounds=0;
        failures(h,critical?2:1);
        return;
    }
    int left=int(h.current)-std::max(0,damage-absorbed);
    h.current=std::max(0,left);
    if(left<=0)enter(h,-left>=h.maximum?Status::Dead:Status::Dying);
}
inline const char *heal(Health &h,int amount){
    if(!h.maximum || amount<1 || amount>limit)return "Check healing";
    if(h.status==Status::Dead)return "Revival required";
    h.current=std::min(int(h.maximum),h.current+amount);
    enter(h,Status::Awake);return nullptr;
}
inline const char *stabilize(Health &h,int hours){
    if(!h.maximum || h.status!=Status::Dying)return "Not dying";
    if(hours<1 || hours>4)return "Check recovery time";
    enter(h,Status::Stable);h.recoveryRounds=600*hours;return nullptr;
}
inline const char *save(Health &h,int face,int modifier,int recoveryHours){
    if(!h.maximum || h.status!=Status::Dying)return "Not dying";
    if(face<1 || face>20 || modifier< -100 || modifier>100)return "Check roll";
    if(face==20)return heal(h,1);
    if(face==1 || face+modifier<10)failures(h,face==1?2:1);
    else if(++h.successes==3)return stabilize(h,recoveryHours);
    return nullptr;
}

inline void elapse(Health &h,int rounds){
    if(h.status!=Status::Stable || !h.recoveryRounds)return;
    if(rounds>=h.recoveryRounds)heal(h,1);
    else h.recoveryRounds-=rounds;
}

// Each entry represents a received effect. Durations are counted in six-second rounds.
enum Aid { Guidance, Bardic, Bless, Bane, AidCount };
enum class Roll { Skill, Ability, Attack, Save, Death };
struct Aids {uint16_t rounds[AidCount]{}; uint8_t guidance{},bardic{};};
inline bool applies(const Aids &a,Aid aid,Roll kind,int skill=-1){
    if(!a.rounds[aid])return false;
    if(aid==Guidance)return (kind==Roll::Ability || kind==Roll::Skill) &&
        (!a.guidance || (kind==Roll::Skill && skill+1==a.guidance));
    if(aid==Bardic)return true;
    return kind==Roll::Attack || kind==Roll::Save || kind==Roll::Death;
}
inline int dieSize(const Aids &a,Aid aid){return aid==Bardic?6+2*(a.bardic%4):4;}
inline bool singleUse(const Aids &a,Aid aid){return aid==Bardic || (aid==Guidance && !a.guidance);}
inline bool useDie(Aids &a,Aid aid,Roll roll,int skill,int face,int &bonus){
    if(!applies(a,aid,roll,skill) || face<1 || face>dieSize(a,aid))return false;
    bonus=aid==Bane?-face:face;
    if(singleUse(a,aid))a.rounds[aid]=0;
    return true;
}
// Guidance 0 = legacy, 1..18 = revised skill. Bardic 0..3 = legacy d6..d12, 4..7 = revised.
inline const char *setAid(Aids &a,int aid,int option){
    if(aid<0 || aid>=AidCount || option< -1)return "Check effect";
    if(option<0){a.rounds[aid]=0;return nullptr;}
    if(aid==Guidance){if(option>18)return "Check effect";a.guidance=option;}
    else if(aid==Bardic){if(option>7)return "Check effect";a.bardic=option;}
    else if(option)return "Check effect";
    a.rounds[aid]=aid==Bardic?(option>=4?600:100):10;
    return nullptr;
}
inline void elapse(Aids &a,int rounds){
    for(auto &r:a.rounds)r=std::max(0,int(r)-rounds);
}
}
