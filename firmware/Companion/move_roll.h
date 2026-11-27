#pragma once
#include "rules.h"
#include <cstdlib>
#include <cctype>
#include <cstring>
namespace moves {
struct Roll {
    int count=0,sides=0,bonus=0;
    bool attack=false;
    int extraOnMax=0;
    bool halfOnSave=false,leapOnPair=false,weaponDamage=false;
    bool valid() const{return count>0;}
};
// Only a complete single dice expression can become a touch roll.
inline Roll readDice(const char *text){
    if(!text)return {};
    char *end=nullptr;
    long count=std::strtol(text,&end,10);
    if(end==text || (*end!='d' && *end!='D') || count<1 || count>100)return {};
    const char *p=end+1;long sides=std::strtol(p,&end,10);
    if(end==p || sides<2 || sides>100)return {};
    while(*end==' ')++end;
    long bonus=0;
    if(*end=='+' || *end=='-'){
        int sign=*end++=='-'?-1:1;
        while(*end==' ')++end;
        p=end;if(!std::isdigit(static_cast<unsigned char>(*p)))return {};
        bonus=sign*std::strtol(p,&end,10);if(bonus< -1000 || bonus>1000)return {};
        while(*end==' ')++end;
    }
    if(*end)return {};
    return {int(count),int(sides),int(bonus),false};
}
template<class Next>
inline bool resolveEffect(const Roll &r,Next next,uint8_t (&faces)[100],unsigned &count,int64_t &total){
    count=0;
    dnd::DieGroup g{uint16_t(r.count),uint16_t(r.sides)};
    auto draw=[&](int sides){int v=next(sides);if(v>0 && v<=sides)faces[count++]=v;return v;};
    if(!dnd::rollPool(&g,1,r.bonus,draw,total))return false;
    int extra=r.extraOnMax;
    for(unsigned i=0;i<count && extra>0 && count<100;i++)if(faces[i]==r.sides){
        int v=draw(r.sides);if(v<1 || v>r.sides)return false;total+=v;--extra;
    }
    if(r.weaponDamage && total<0)total=0;
    return true;
}
}
