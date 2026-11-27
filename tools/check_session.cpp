#include "../firmware/Companion/session_store.h"
#include "../firmware/Companion/move_roll.h"
#include <Preferences.h>
#include <cassert>
#include <iostream>
using namespace session;
int main(){
    Build b;b.sorcerer=b.spMax=2;b.modern=true;b.limit[0]=3;b.limit[10]=2;b.spent[10]=1;
    auto s=initial(b);assert(points(b,s)==2 && remaining(b,s,10)==1);
    auto original=s;
    auto pact=slotCount(b,s,true,2);assert(pact.left==1 && pact.used==1);
    assert(slotCount(b,s,false,2).left==0 && slotCount(b,s,true,3).used==0);
    assert(apply(b,s,{Op::SlotToPoints,10}) && !memcmp(&original,&s,sizeof(s)));
    auto partial=s;partial.spUsed=1;
    assert(!apply(b,partial,{Op::SlotToPoints,10}) && points(b,partial)==2 && remaining(b,partial,10)==0);
    assert(remaining(b,partial,0)==3);
    assert(!apply(b,s,{Op::PointsToSlot,0}) && remaining(b,s,0)==4 && !points(b,s));
    assert(slotCount(b,s,false).left==4 && slotCount(b,s,false).used==0);
    auto unchanged=s;assert(apply(b,s,{Op::Cast,10,1}) && !memcmp(&unchanged,&s,sizeof(s)));
    assert(!apply(b,s,{Op::SlotToPoints,10}) && points(b,s)==2 && !remaining(b,s,10));
    assert(!apply(b,s,{Op::ShortRest}) && remaining(b,s,10)==2 && s.created[0]==1);
    assert(!apply(b,s,{Op::Cast,0,1}) && s.created[0]==0 && s.spent[0]==0 && points(b,s)==1);
    auto before=s;assert(apply(b,s,{Op::Cast,10,2}) && !memcmp(&before,&s,sizeof(s)));
    auto trial=s;assert(!apply(b,trial,{Op::Cast,10,1}) && !points(b,trial));
    assert(points(b,s)==1 && slotCount(b,trial,true).used==1 && slotCount(b,s,true).used==0);
    assert(!apply(b,s,{Op::LongRest}) && remaining(b,s,0)==3 && remaining(b,s,10)==2 && points(b,s)==2);
    b.sorcerer=b.spMax=6;s.spUsed=6;
    assert(!apply(b,s,{Op::ShortRest}) && points(b,s)==0);
    assert(!apply(b,s,{Op::ShortRest,-1,0,true}) && points(b,s)==3);
    assert(!apply(b,s,{Op::ShortRest,-1,0,true}) && points(b,s)==3);
    assert(!apply(b,s,{Op::LongRest}) && !s.restored);
    assert(!canCreate(b,4));b.modern=false;assert(canCreate(b,4));
    b.sorcerer=b.spMax=20;s.spUsed=15;assert(!apply(b,s,{Op::ShortRest}) && s.spUsed==11);
    assert(metamagicCost("Twinned Spell",true,4)==1 && metamagicCost("Twinned Spell",false,4)==4);
    assert(metamagicCost("Distant Spell",true,3)==1);
    Build first;first.sorcerer=first.spMax=2;first.limit[0]=3;
    assert(!commit(100,first,{Op::Cast,0,1}) && read(100,first).spent[0]==1 && read(100,first).spUsed==1);
    assert(!read(101,first).spent[0]);Preferences::failKey="s100";
    assert(commit(100,first,{Op::LongRest}) && read(100,first).spent[0]==1);
    Preferences::failKey="";assert(undo(100) && !read(100,first).spent[0] && !undo(100));
    assert(setBondChoice(100,7) && bondChoice(100)==7 && !bondChoice(101));
    assert(!commit(100,first,{Op::Cast,0,1}));
    Preferences::failKey="b100";
    assert(!setBondChoice(100,8) && bondChoice(100)==7);
    Preferences::failKey="";
    assert(setBondChoice(100,8) && undo(100) && bondChoice(100)==8);
    assert(setBondChoice(100,0) && !bondChoice(100));
    assert(setBondChoice(100,7));forget(100);assert(!bondChoice(100));
    Build warder;warder.baseAC=12;warder.mageAC=15;warder.sourceTemp=10;warder.limit[1]=1;
    auto current=initial(warder);assert(current.temp==10 && !retaliation(current,true));
    Change mage{Op::Cast,-1,0,false,Ward::MageArmor};
    assert(!apply(warder,current,mage) && armorClass(warder,current)==15 && !current.spent[1]);
    warder.armored=true;auto untouched=current;
    assert(apply(warder,current,mage) && !memcmp(&current,&untouched,sizeof(current)));
    auto paidMage=mage;paidMage.pool=1;
    assert(apply(warder,current,paidMage) && remaining(warder,current,1)==1);
    warder.armored=false;
    assert(apply(warder,current,{Op::Cast,1,0,false,Ward::Agathys,2}) && remaining(warder,current,1)==1);
    assert(!apply(warder,current,{Op::Cast,1,0,false,Ward::Agathys,2,false,TempChoice::Keep}));
    assert(current.temp==10 && !remaining(warder,current,1) && retaliation(current,true)==10 && !retaliation(current,false));
    assert(!apply(warder,current,{Op::GrantTemp,-1,0,false,Ward::None,14}) && retaliation(current,true)==10);
    assert(!apply(warder,current,{Op::TakeHit,-1,0,false,Ward::None,14,true}) && !current.temp && !retaliation(current,true));
    assert(!apply(warder,current,{Op::Cast,-1,0,false,Ward::Agathys,1}));
    assert(!apply(warder,current,{Op::Cast,-1,0,false,Ward::Shield}) && armorClass(warder,current)==20);
    assert(!apply(warder,current,{Op::ShortRest}) && !current.shieldSpell && current.temp==5 && !agathysLevel(current) && current.mageRounds==4200);
    assert(!apply(warder,current,{Op::Elapse,-1,0,false,Ward::None,4200}) && armorClass(warder,current)==12);
    assert(!apply(warder,current,{Op::LongRest}) && !current.temp);
    std::vector<uint8_t> old(56);old[0]=1;old[4]=1;old[4+18]=1;old[54]=1;
    Preferences::data["dnd-session/s102"]=old;
    warder.limit[0]=1;warder.sorcerer=warder.spMax=2;
    auto saved=read(102,warder);
    assert(saved.spent[0]==1 && saved.created[0]==1 && saved.temp==10);
    assert(undo(102) && !read(102,warder).spent[0] && read(102,warder).temp==10);
    assert(!commit(102,warder,mage));warder.armored=true;reconcile(102,warder);
    warder.armored=false;assert(!read(102,warder).mageRounds);
    // A rebuild can invalidate part of a session. Undo may only restore a legal state.
    auto high=first;high.sorcerer=high.spMax=5;high.limit[1]=1;
    assert(!commit(104,high,{Op::PointsToSlot,2}) && !commit(104,high,{Op::Cast,1}));
    auto reduced=high;reduced.sorcerer=reduced.spMax=1;reduced.limit[1]=0;
    reconcile(104,reduced);reconcile(104,reduced);
    auto repaired=read(104,high);
    assert(!repaired.created[2] && !repaired.spent[1] && repaired.spUsed==1);
    assert(undo(104));repaired=read(104,high);
    assert(!repaired.created[2] && !repaired.spent[1] && repaired.spUsed==1);
    {
        Build b{};std::fill_n(b.limit,9,3);b.sourceTemp=17;
        auto cast=[](int lv,TempChoice choice=TempChoice::Unspecified){
            return Change{Op::Cast,lv-1,0,false,Ward::Agathys,lv,false,choice};
        };
        auto s=initial(b),before=s;
        assert(apply(b,s,cast(2)) && !memcmp(&s,&before,sizeof(s)));
        assert(!apply(b,s,cast(2,TempChoice::Keep)) && s.temp==17 && remaining(b,s,1)==2);
        s=before;assert(!apply(b,s,cast(2,TempChoice::Take)) && s.temp==10);
        assert(!apply(b,s,{Op::TakeHit,-1,0,false,Ward::None,9}) && s.temp==1);
        elapse(s,30);assert(!apply(b,s,cast(1)) && s.temp==1 && retaliation(s,true)==10);
        auto depleted=s;
        assert(!apply(b,depleted,{Op::TakeHit,-1,0,false,Ward::None,1,true}) && !agathysLevel(depleted));
        assert(!apply(b,depleted,{Op::GrantTemp,-1,0,false,Ward::None,12}) && !retaliation(depleted,true));
        for(int first=1;first<=9;++first)for(int second=1;second<=9;++second)for(int gap:{0,1,599,600}){
            s=initial(b);assert(!apply(b,s,cast(first,TempChoice::Take)));
            elapse(s,gap);assert(!apply(b,s,cast(second,TempChoice::Take)));
            assert(agathysLevel(s)==(gap==600?second:std::max(first,second)));
            auto hp=s.temp;assert(retaliation(s,true)==5*agathysLevel(s) && !retaliation(s,false));
            elapse(s,600-gap);assert(agathysLevel(s)==(gap?second:0) && s.temp==hp);
            elapse(s,gap);assert(!agathysLevel(s) && s.temp==hp);
        }
        s=initial(b);assert(!apply(b,s,cast(3,TempChoice::Take)));
        assert(!apply(b,s,{Op::TakeHit,-1,0,false,Ward::None,14}) && s.temp==1);
        elapse(s,100);assert(!apply(b,s,cast(2)));
        elapse(s,100);assert(!apply(b,s,cast(1)));
        elapse(s,400);assert(retaliation(s,true)==10 && s.temp==1);
        elapse(s,100);assert(retaliation(s,true)==5 && s.temp==1);
        elapse(s,100);assert(!retaliation(s,true) && s.temp==1);
        // Serialized v2 offsets are fixed by the firmware's on-device layout.
        std::vector<uint8_t> old(80,0);old[0]=2;old[4+26]=10;old[4+30]=120;
        old[4+32]=10;old[4+34]=1;old[40+26]=4;old[40+30]=80;old[40+32]=5;
        old[40+34]=1;old[76]=1;old[77]=static_cast<uint8_t>(Op::Cast);
        Preferences::data["dnd-session/s103"]=old;
        s=read(103,b);assert(s.temp==10 && s.agathysRounds[1]==120 && agathysLevel(s)==2);
        assert(undo(103));s=read(103,b);assert(s.temp==4 && s.agathysRounds[0]==80 && agathysLevel(s)==1);
        assert(!commit(103,b,cast(2,TempChoice::Keep)));
        assert(read(103,b).temp==4 && read(103,b).agathysRounds[0]==80 && read(103,b).agathysRounds[1]==600);
        assert(undo(103) && agathysLevel(read(103,b))==1);
    }
    moves::Roll burst{2,8,0,false};burst.extraOnMax=4;uint8_t faces[100];unsigned count;int64_t total;
    assert(moves::resolveEffect(burst,[](int){return 8;},faces,count,total) && count==6 && total==48);
    moves::Roll weak{1,4,-3,false};weak.weaponDamage=true;
    assert(moves::resolveEffect(weak,[](int){return 1;},faces,count,total) && count==1 && total==0);
    std::cout<<"Defensive casts, temporary HP, melee retaliation, game time, equipment changes, legacy session migration, cast and metamagic atomicity, slot conversions, Pact recovery, created-slot expiry, both editions of Restoration, storage isolation, write failure, Undo and bounded exploding dice passed.\n";
}
