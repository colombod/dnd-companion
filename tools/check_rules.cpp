#include "../firmware/Companion/rules.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace dnd;
    for (bool adv : {false,true}) for (bool dis : {false,true}) {
        for (int a=1; a<=20; a++) for (int b=1; b<=20; b++) {
            Result roll{};
            assert(resolveD20(rollMode(adv,dis), a, b, -3, roll));
            assert(roll.total == roll.kept-3 && roll.meetsDC(roll.total) && !roll.meetsDC(roll.total+1));
            std::cout << roll.kept << '\n';
        }
    }
    for (int s=1; s<=30; s++) std::cout << abilityModifier(s) << '\n';
    for (int level=1; level<=20; level++) std::cout << proficiencyBonus(level) << '\n';
    Check entries[] = {{"Arcana",Kind::SkillCheck,Ability::Intelligence,Training::Expertise,0,true,false,0},
        {"Wisdom save",Kind::SavingThrow,Ability::Wisdom,Training::Proficient,1,true,false,0}};
    Profile p{6,{8,14,14,12,10,18},entries,2};
    int bonus = 0;
    assert(modifier(p,0,bonus) && bonus == 7);
    assert(modifier(p,1,bonus) && bonus == 4);
    int draws = 0;
    auto d20 = [&draws](int n) { assert(n == 20); return ++draws == 1 ? 7 : 16; };
    CheckRoll check{};
    assert(rollCheck(p,0,true,false,d20,check) && draws == 2 && check.result.total == 23);
    draws = 0;
    assert(rollCheck(p,1,true,true,d20,check) && draws == 1 && check.result.total == 11);
    entries[0].hasOverride = true; entries[0].overrideTotal = -2;
    assert(modifier(p,0,bonus) && bonus == -2);
    entries[0].resolved = false; assert(!modifier(p,0,bonus));
    Profile other{1,{12,12,16,8,14,10},entries,2};
    assert(modifier(other,1,bonus) && bonus == 5);
    DieGroup pool[] = {{3,6},{1,8}};
    int64_t total = 0;
    assert(rollPool(pool,2,2,[](int n){return n;},total) && total == 28);
    assert(!rollPool(pool,2,2,[](int){return 0;},total) && total == 28);
}
