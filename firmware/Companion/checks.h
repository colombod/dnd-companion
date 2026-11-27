#pragma once
#include "rules.h"
#include <cstring>

namespace checks {
struct Skill { const char *name, *slug; uint8_t ability, sourceId; };
inline constexpr Skill skills[]={
    {"Acrobatics","acrobatics",1,3},{"Animal Handling","animal-handling",4,11},
    {"Arcana","arcana",3,6},{"Athletics","athletics",0,2},{"Deception","deception",5,16},{"History","history",3,7},
    {"Insight","insight",4,12},{"Intimidation","intimidation",5,17},{"Investigation","investigation",3,8},
    {"Medicine","medicine",4,13},{"Nature","nature",3,9},{"Perception","perception",4,14},
    {"Performance","performance",5,18},{"Persuasion","persuasion",5,19},{"Religion","religion",3,10},
    {"Sleight of Hand","sleight-of-hand",1,4},{"Stealth","stealth",1,5},{"Survival","survival",4,15}
};
inline constexpr const char *abilities[]={"Strength","Dexterity","Constitution","Intelligence","Wisdom","Charisma"};
inline constexpr const char *shortNames[]={"STR","DEX","CON","INT","WIS","CHA"};
inline constexpr const char *slugs[]={"strength","dexterity","constitution","intelligence","wisdom","charisma"};
constexpr int plain=18,saves=24,count=30;
enum Flag:uint8_t { Unresolved=1,Override=2,Advantage=4,Disadvantage=8,Conditional=16,Concentration=32 };
struct Entry {
    int16_t bonus,overrideTotal;
    uint8_t ability,training,flags,reserved;
};
struct alignas(uint32_t) Snapshot {
    uint8_t version,scores[6],reserved;
    Entry rows[count];
};
inline const char *name(int i) {return i<plain?skills[i].name:abilities[(i-plain)%6];}
inline dnd::Kind kind(int i) {return i<plain?dnd::Kind::SkillCheck:i<saves?dnd::Kind::AbilityCheck:dnd::Kind::SavingThrow;}
// Only normalized numeric fields live in flash. Names and rules stay in code.
struct Prepared {
    dnd::Check check{};
    dnd::Profile profile{};
    Prepared(const Snapshot &s,int level,int index) {
        if (!s.version || index<0 || index>=count) return;
        const auto &e=s.rows[index];
        check={name(index),kind(index),dnd::Ability(e.ability),dnd::Training(e.training),e.bonus,
               !(e.flags&Unresolved),bool(e.flags&Override),e.overrideTotal};
        profile.level=level;memcpy(profile.scores,s.scores,6);
        profile.checks=&check;profile.count=1;
    }
    Prepared(const Prepared &)=delete;
    bool modifier(int &value) const {return dnd::modifier(profile,0,value);}
};
}
