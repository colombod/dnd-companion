#include "../firmware/Companion/session_store.h"
#include "../firmware/Companion/sheet.h"
#include <Preferences.h>
#include <cassert>
#include <fstream>
#include <iostream>
using namespace session;
using vitals::Status;

Change action(Op op,int amount=0){
    Change c{op};c.amount=amount;c.recoveryHours=2;return c;
}
int main(int argc,char **argv){
    Build b;b.hpHint=24;b.removedHP=14;b.sourceTemp=5;
    State s=initial(b);
    auto applyOK=[&](Change c){assert(!apply(b,s,c));};
    applyOK(action(Op::SetMaxHP,24));assert(s.hp.current==10 && s.temp==5);
    applyOK(action(Op::TakeHit,9));assert(s.hp.current==6 && !s.temp);
    auto crit=action(Op::TakeHit,6);crit.critical=true;
    applyOK(crit);assert(s.hp.status==Status::Dying && !s.hp.failures);
    auto dying=s;
    applyOK(action(Op::DeathSave,1));assert(s.hp.failures==2);
    applyOK(action(Op::DeathSave,11));assert(s.hp.successes==1);
    applyOK(action(Op::Stabilize));assert(s.hp.status==Status::Stable && !s.hp.current && !s.hp.failures && !s.hp.successes);
    assert(apply(b,s,action(Op::DeathSave,19)));
    auto stable=s;
    applyOK(action(Op::GrantTemp,8));applyOK(action(Op::TakeHit,1));
    assert(s.hp.status==Status::Dying && s.hp.failures==1 && s.temp==7 && !s.hp.recoveryRounds);
    applyOK(crit);assert(s.hp.status==Status::Dead && s.hp.failures==3);
    assert(apply(b,s,action(Op::Heal,3)) && apply(b,s,action(Op::Stabilize)) && apply(b,s,action(Op::LongRest)));
    applyOK(action(Op::Revive));assert(s.hp.current==1 && !s.hp.failures);
    applyOK(action(Op::Heal,90));assert(s.hp.current==24);
    b.revisedRest=true;s=stable;assert(apply(b,s,action(Op::ShortRest)) && !s.hp.current);
    b.revisedRest=false;applyOK(action(Op::ShortRest));assert(s.hp.recoveryRounds==600);
    s=stable;applyOK(action(Op::Elapse,1199));assert(s.hp.recoveryRounds==1 && !s.hp.current);
    applyOK(action(Op::Elapse,1));assert(s.hp.current==1 && s.hp.status==Status::Awake);
    s=dying;auto save=action(Op::DeathSave,8);save.points=2;
    for(int i=0;i<3;i++)applyOK(save);
    assert(s.hp.status==Status::Stable && s.hp.recoveryRounds==1200 && !s.hp.successes);
    for(int face=1;face<=20;++face)for(int mod=-12;mod<=12;++mod){
        s=dying;save.amount=face;save.points=mod;applyOK(save);
        assert(s.hp.current==(face==20));
        assert(s.hp.successes==(face!=1 && face!=20 && face+mod>=10));
        assert(s.hp.failures==(face==1?2:face!=20 && face+mod<10?1:0));
    }
    s=initial(b);applyOK(action(Op::SetMaxHP,24));applyOK(action(Op::TakeHit,38));
    assert(s.hp.status==Status::Dying);s=initial(b);applyOK(action(Op::SetMaxHP,24));applyOK(action(Op::TakeHit,39));
    assert(s.hp.status==Status::Dead);
    s=dying;applyOK(action(Op::TakeHit,24));assert(s.hp.status==Status::Dead);
    s=initial(b);applyOK(action(Op::SetMaxHP,24));applyOK(action(Op::LongRest));assert(s.hp.current==24 && !s.temp);
    {
        Build down=b;down.removedHP=50;down.sourceStable=true;down.sourceFail=2;
        auto imported=initial(down);assert(!apply(down,imported,action(Op::SetMaxHP,24)));
        assert(!imported.hp.current && imported.hp.status==Status::Stable && !imported.hp.failures);
    }
    using vitals::Roll;
    auto aid=action(Op::SetAid,3);aid.pool=vitals::Guidance;applyOK(aid);
    assert(vitals::applies(s.aids,vitals::Guidance,Roll::Skill,2)); // Arcana.
    assert(!vitals::applies(s.aids,vitals::Guidance,Roll::Skill,5));
    assert(!vitals::applies(s.aids,vitals::Guidance,Roll::Ability));
    aid.amount=0;applyOK(aid);assert(vitals::applies(s.aids,vitals::Guidance,Roll::Ability));
    assert(!vitals::applies(s.aids,vitals::Guidance,Roll::Death));
    aid.pool=vitals::Bardic;aid.amount=5;applyOK(aid);
    aid.pool=vitals::Bless;aid.amount=0;applyOK(aid);
    aid.pool=vitals::Bane;applyOK(aid);
    for(auto effect:{vitals::Bless,vitals::Bane}){
        assert(vitals::applies(s.aids,effect,Roll::Death) && vitals::applies(s.aids,effect,Roll::Attack));
        assert(!vitals::applies(s.aids,effect,Roll::Skill,3));
    }
    applyOK(action(Op::Elapse,10));assert(!s.aids.rounds[vitals::Bless] && !s.aids.rounds[vitals::Guidance]);
    assert(s.aids.rounds[vitals::Bardic]==590);aid.pool=vitals::Bardic;aid.amount=-1;applyOK(aid);
    assert(!vitals::applies(s.aids,vitals::Bardic,Roll::Save));

    aid.pool=vitals::Bardic;aid.amount=7;applyOK(aid);
    Change use=action(Op::UseAid,12);use.pool=vitals::Bardic;use.roll=Roll::Death;
    auto beforeUse=s;use.amount=13;assert(apply(b,s,use) && !memcmp(&s,&beforeUse,sizeof(s)));
    use.amount=12;applyOK(use);assert(!s.aids.rounds[vitals::Bardic] && apply(b,s,use));
    int bonus=0;
    aid.pool=vitals::Bane;aid.amount=0;applyOK(aid);
    assert(vitals::useDie(s.aids,vitals::Bane,Roll::Death,-1,4,bonus) && bonus==-4 && s.aids.rounds[vitals::Bane]);
    aid.pool=vitals::Guidance;aid.amount=3;applyOK(aid);
    assert(!vitals::useDie(s.aids,vitals::Guidance,Roll::Save,2,4,bonus));
    assert(vitals::useDie(s.aids,vitals::Guidance,Roll::Skill,2,4,bonus) && bonus==4 && s.aids.rounds[vitals::Guidance]);
    aid.amount=0;applyOK(aid);
    assert(vitals::useDie(s.aids,vitals::Guidance,Roll::Ability,-1,3,bonus) && !s.aids.rounds[vitals::Guidance]);

    // Flash-layout regression: migrate a 0.1.1 session and its undo without reimporting resources.
    struct OldRecord{uint32_t version=4;StoredStateV4 current{},previous{};bool canUndo=true;uint8_t lastOp=0;};
    OldRecord old;old.current.temp=8;old.current.tempKnown=true;old.previous.temp=3;old.previous.tempKnown=true;
    Preferences prefs;prefs.begin("dnd-session");prefs.putBytes("s70",&old,sizeof(old));
    assert(read(70,b).temp==8 && undo(70) && read(70,b).temp==3);
    assert(!commit(70,b,action(Op::SetMaxHP,24)));assert(!commit(70,b,action(Op::TakeHit,16)));
    assert(read(70,b).hp.status==Status::Dying);
    b.removedHP=0;reconcile(70,b);assert(!read(70,b).hp.current && !read(71,b).hp.maximum);
    Preferences::failKey="s70";assert(commit(70,b,action(Op::Stabilize)));Preferences::failKey="";
    assert(undo(70) && read(70,b).hp.current==10 && read(70,b).temp==3);

    for(int i=1;i+1<argc;i+=2){
        std::ifstream input(argv[i]);JsonDocument book;Sheet sheet{};
        assert(!parseSheet(input,std::stoul(argv[i+1]),sheet,&book));
        auto live=casting::build(book);
        assert(book["health"].is<JsonObject>() && live.hpHint>=0);
        if(sheet.rolls.scores[2])assert(live.hpHint>0);
        if(!live.hpHint){
            assert(!sheet.rolls.scores[2]);auto manual=initial(live);
            assert(!manual.hp.maximum && !apply(live,manual,action(Op::SetMaxHP,64)));
            assert(manual.hp.maximum==64);
            std::cout<<"Imported level "<<int(sheet.level)<<" with unresolved Constitution: manual maximum verified\n";
        }else std::cout<<"Imported level "<<int(sheet.level)<<" HP suggestion "<<live.hpHint<<"\n";
    }
    std::cout<<"HP, death saves, stability, recovery, contextual effects and v4 migration passed.\n";
}
