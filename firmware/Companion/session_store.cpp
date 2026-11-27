#include "session_store.h"
#include "session_files.h"
#include <Arduino.h>
#include <Preferences.h>
namespace session {
namespace {
Preferences store;bool opened=false;
struct Record{uint32_t version=6;State current{},previous{};bool canUndo=false;uint8_t lastOp=0;uint32_t sourceKey{};int sourceCount=-1;};
// FAT holds the two full snapshots. A torn write to the inactive bank cannot change the NVS pointer.
struct Pointer {uint32_t marker{},hash{};};
constexpr uint32_t bankMarker=0x53455360;
uint32_t cachedId{};
Pointer cachedPointer{};
Record cached{};
uint32_t checksum(const Record &r){
    auto *bytes=reinterpret_cast<const uint8_t*>(&r);
    uint32_t h=2166136261u;
    for(size_t i=0;i<sizeof(r);++i)h=(h^bytes[i])*16777619u;
    return h;
}
struct V5Record{uint32_t version;StoredStateV5 current,previous;bool canUndo;uint8_t lastOp;};
struct V4Record{uint32_t version;StoredStateV4 current,previous;bool canUndo;uint8_t lastOp;};
struct V3Record{uint32_t version;StoredStateV3 current,previous;bool canUndo;uint8_t lastOp;};
struct LegacyState{uint8_t spent[pools],created[5],spUsed;bool restored;};
struct LegacyRecord{uint32_t version;LegacyState current,previous;bool canUndo;uint8_t lastOp;};
static_assert(sizeof(LegacyRecord)==56);
struct V2State{
    uint8_t spent[pools],created[5],spUsed;
    bool restored;
    uint16_t temp,mageRounds,frostRounds;
    uint8_t cold;
    bool shieldSpell,tempKnown,useMisc;
};
struct V2Record{uint32_t version;V2State current,previous;bool canUndo;uint8_t lastOp;};
static_assert(sizeof(V2State)==36 && sizeof(V2Record)==80);
State migrate(const LegacyState &old){
    State s{};memcpy(s.spent,old.spent,pools);memcpy(s.created,old.created,5);
    s.spUsed=old.spUsed;s.restored=old.restored;return s;
}
State migrate(const V2State &old){
    State s{};memcpy(s.spent,old.spent,pools);memcpy(s.created,old.created,5);
    s.spUsed=old.spUsed;s.restored=old.restored;s.temp=old.temp;s.mageRounds=old.mageRounds;
    s.shieldSpell=old.shieldSpell;s.tempKnown=old.tempKnown;s.useMisc=old.useMisc;
    if(old.temp && old.cold>=5 && old.cold<=45 && old.cold%5==0)
        s.agathysRounds[old.cold/5-1]=old.frostRounds;
    return s;
}
void hydrate(State &s,const Build &b){
    reconcileUses(b,s);
    if(b.armored)s.mageRounds=0;
    if(b.known)for(int pool=0;pool<pools;pool++)s.spent[pool]=std::min(s.spent[pool],b.limit[pool]);
    s.spUsed=std::min(s.spUsed,b.spMax);
    for(int i=0;i<5;i++)if(!canCreate(b,i+1))s.created[i]=0;
    if(!b.modern || b.sorcerer<5)s.restored=false;
    if(!s.tempKnown){s.temp=std::clamp(b.sourceTemp,0,65535);s.tempKnown=true;}
    if(!s.temp)setTemp(s,0);
}
String key(uint32_t id){return "s"+String(id);}
bool ready(){if(!opened)opened=store.begin("dnd-session");return opened;}
bool get(uint32_t id,Record &r){
    if(!ready())return false;
    auto k=key(id);
    auto length=store.getBytesLength(k.c_str());
    if(length==sizeof(Pointer)){
        Pointer pointer;
        if(store.getBytes(k.c_str(),&pointer,sizeof(pointer))!=sizeof(pointer) ||
           (pointer.marker&~1u)!=bankMarker)return false;
        if(cachedId==id && pointer.marker==cachedPointer.marker && pointer.hash==cachedPointer.hash){
            r=cached;return true;
        }
        if(!sessionfiles::read(id,pointer.marker&1,&r,sizeof(r)) || r.version!=6 || checksum(r)!=pointer.hash)return false;
        cached=r;cachedId=id;cachedPointer=pointer;return true;
    }
    if(length==sizeof(LegacyRecord)){
        LegacyRecord old{};
        if(store.getBytes(k.c_str(),&old,sizeof(old))!=sizeof(old) || old.version!=1)return false;
        r.current=migrate(old.current);r.previous=migrate(old.previous);r.canUndo=old.canUndo;r.lastOp=old.lastOp;return true;
    }
    if(length==sizeof(V2Record)){
        V2Record old{};
        if(store.getBytes(k.c_str(),&old,sizeof(old))!=sizeof(old) || old.version!=2)return false;
        r.current=migrate(old.current);r.previous=migrate(old.previous);r.canUndo=old.canUndo;r.lastOp=old.lastOp;return true;
    }
    if(length==sizeof(V3Record)){
        V3Record old{};
        if(store.getBytes(k.c_str(),&old,sizeof(old))!=sizeof(old) || old.version!=3)return false;
        static_cast<StoredStateV3 &>(r.current)=old.current;
        static_cast<StoredStateV3 &>(r.previous)=old.previous;
        r.canUndo=old.canUndo;r.lastOp=old.lastOp;return true;
    }
    if(length==sizeof(V4Record)){
        V4Record old{};
        if(store.getBytes(k.c_str(),&old,sizeof(old))!=sizeof(old) || old.version!=4)return false;
        static_cast<StoredStateV4 &>(r.current)=old.current;
        static_cast<StoredStateV4 &>(r.previous)=old.previous;
        r.canUndo=old.canUndo;r.lastOp=old.lastOp;return true;
    }
    if(length==sizeof(V5Record)){
        V5Record old{};
        if(store.getBytes(k.c_str(),&old,sizeof(old))!=sizeof(old) || old.version!=5)return false;
        static_cast<StoredStateV5 &>(r.current)=old.current;
        static_cast<StoredStateV5 &>(r.previous)=old.previous;
        r.canUndo=old.canUndo;r.lastOp=old.lastOp;return true;
    }
    return false;
}
bool save(uint32_t id,const Record &r){
    if(!ready())return false;
    auto k=key(id);
    bool bank=false;
    if(store.getBytesLength(k.c_str())==sizeof(Pointer)){
        Pointer old;
        if(store.getBytes(k.c_str(),&old,sizeof(old))!=sizeof(old) || (old.marker&~1u)!=bankMarker)return false;
        bank=!(old.marker&1);
    }
    Pointer pointer{bankMarker|unsigned(bank),checksum(r)};
    Record check;
    if(!sessionfiles::write(id,bank,&r,sizeof(r)) || !sessionfiles::read(id,bank,&check,sizeof(check)) ||
       checksum(check)!=pointer.hash || store.putBytes(k.c_str(),&pointer,sizeof(pointer))!=sizeof(pointer))return false;
    cached=r;cachedId=id;cachedPointer=pointer;return true;
}
}
bool readable(uint32_t id){
    Record r;
    return ready() && (!store.isKey(key(id).c_str()) || get(id,r));
}
State read(uint32_t id,const Build &b){Record r;if(!get(id,r))return initial(b);hydrate(r.current,b);return r.current;}
vitals::Health health(uint32_t id){Record r;return get(id,r)?r.current.hp:vitals::Health{};}
const char *commit(uint32_t id,const Build &b,Change c){
    if(!id || !ready())return "Storage unavailable";
    Record r;
    if(!get(id,r)){
        if(store.isKey(key(id).c_str()))return "Saved state needs review";
        r.current=initial(b);
    }
    hydrate(r.current,b);auto before=r.current;
    if(auto error=apply(b,r.current,c))return error;
    r.previous=before;r.canUndo=true;r.lastOp=static_cast<uint8_t>(c.op);
    r.sourceCount=-1;
    if(c.op==Op::Coin){r.sourceKey=c.pool;r.sourceCount=b.bag.money[c.pool];}
    if(c.op==Op::Stock || c.op==Op::ItemUse){
        pack::Item it;
        if(b.bag.item(c.itemId,it)){r.sourceKey=c.itemId;r.sourceCount=it.stock;}
    }
    return save(id,r)?nullptr:"Could not save change";
}
bool undo(uint32_t id){
    Record r;if(!get(id,r) || !r.canUndo)return false;
    r.current=r.previous;r.canUndo=false;return save(id,r);
}
bool last(uint32_t id,Op &op){Record r;if(!get(id,r) || !r.canUndo)return false;op=Op(r.lastOp);return true;}
void reconcile(uint32_t id,const Build &build){
    Record r;
    if(!get(id,r))return;
    auto before=r;
    auto refresh=[&](State &s){
        hydrate(s,build);
        if(build.bag.find)for(auto &v:s.bag.edits){
            pack::Item it;
            if(v.id && (!build.bag.item(v.id,it) || v.count==it.stock))v={};
        }
        for(int i=0;i<pack::coinCount;++i)if(build.bag.money[i]>=0 && build.bag.money[i]==s.bag.balances[i])
            s.bag.editedCoins&=~(1u<<i);
    };
    refresh(r.current);
    if(r.canUndo)refresh(r.previous);
    if(r.canUndo && r.sourceCount>=0){
        if(Op(r.lastOp)==Op::Coin && r.sourceKey<pack::coinCount){
            int source=build.bag.money[r.sourceKey];if(source>=0 && source!=r.sourceCount)r.canUndo=false;
        }else if(build.bag.find){
            pack::Item it;
            if(!build.bag.item(r.sourceKey,it) || it.stock!=r.sourceCount)r.canUndo=false;
        }
    }
    if(before.canUndo!=r.canUndo || memcmp(&before.current,&r.current,sizeof(State)) || memcmp(&before.previous,&r.previous,sizeof(State)))save(id,r);
}
String bondKey(uint32_t id){return "b"+String(id);}
uint32_t bondChoice(uint32_t character){
    if(!character || !ready())return 0;
    auto k=bondKey(character);
    uint32_t item=0;
    return store.isKey(k.c_str()) && store.getBytes(k.c_str(),&item,sizeof(item))==sizeof(item)?item:0;
}
bool setBondChoice(uint32_t character,uint32_t item){
    if(!character || !ready())return false;
    auto k=bondKey(character);
    if(!item)return !store.isKey(k.c_str()) || store.remove(k.c_str());
    return store.putBytes(k.c_str(),&item,sizeof(item))==sizeof(item);
}
void forget(uint32_t id){
    if(!ready())return;
    auto k=key(id);
    if(store.isKey(k.c_str()) && !store.remove(k.c_str()))return;
    if(cachedId==id)cachedId=0;
    sessionfiles::erase(id);
    setBondChoice(id,0);
}
}
