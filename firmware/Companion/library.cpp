#include "library.h"
#include "session_store.h"
#include "portraits.h"
#include "play.h"
struct PrivateSeed { Sheet sheet; };
#include "seed.h"
#include <Preferences.h>
#include <Arduino.h>

static Preferences prefs;
static bool writable;
struct Catalog { uint32_t ids[Library::capacity], active, selected; };

static Catalog catalog(const Library &lib) {
    Catalog c{};
    for (int i=0; i<lib.count; i++) c.ids[i] = lib.records[i].id;
    c.active = lib.count ? lib.records[lib.active].id : 0;
    c.selected = lib.hasSelection;
    return c;
}
static bool commit(const Catalog &c) {
    return writable && prefs.putBytes("catalog3", &c, sizeof(c)) == sizeof(c);
}
int Library::find(uint32_t id) const {
    for (int i=0; i<count; ++i) if (records[i].id == id) return i;
    return -1;
}
static String key(uint32_t id) { return "c" + String(id); }

void Library::begin() {
    writable = prefs.begin("dnd-library1");
    appearance=prefs.getUChar("pickerArt",0);
    Catalog c{};
    bool migrated = prefs.getBytesLength("catalog3")==sizeof(c) && prefs.getBytes("catalog3",&c,sizeof(c))==sizeof(c);
    if (!migrated && prefs.isKey("catalog3")) return; // Preserve an unreadable existing catalog for recovery.
    bool earlierCatalog=prefs.isKey("catalog2");
    if (!migrated && earlierCatalog) {
        struct Previous {uint32_t ids[capacity-1],active,selected;} old{};
        if (prefs.getBytes("catalog2",&old,sizeof(old))==sizeof(old)) {
            memcpy(c.ids,old.ids,sizeof(old.ids));c.active=old.active;c.selected=old.selected;
        }
    } else if (!migrated) {
        size_t len=prefs.getBytesLength("ids");
        if (len && len<=sizeof(c.ids) && len%4==0) prefs.getBytes("ids",c.ids,len);
        c.selected=prefs.isKey("active");c.active=prefs.getUInt("active",0);
    }
    for (uint32_t id : c.ids) {
        if (!id || find(id)>=0) continue;
        Sheet s{};
        String k = key(id);
        size_t n = prefs.getBytesLength(k.c_str());
        if ((n == sizeof(s) || n == offsetof(Sheet,tag) || n == offsetof(Sheet,avatar) || n == offsetof(Sheet,rolls) || n == offsetof(Sheet,bookHash)) && prefs.getBytes(k.c_str(), &s, n)==n &&
            s.id==id && !s.name[sizeof(s.name)-1] && !s.classes[sizeof(s.classes)-1]) records[count++] = s;
    }
    active = find(c.active);
    hasSelection = c.selected && active >= 0;
    if (active < 0) active = 0;
    if (!migrated) {
#ifdef HAS_SEEDS
        if (!earlierCatalog) for (const auto &seed:seeds) {
            if (find(seed.sheet.id)<0 && count<capacity && !save(seed.sheet)) records[count++]=seed.sheet;
        }
#endif
        commit(catalog(*this));
    }
}
bool Library::readable() const {
    Catalog saved{};
    if (!writable || prefs.getBytes("catalog3", &saved, sizeof(saved)) != sizeof(saved)) return false;
    unsigned countRead = 0;
    for (uint32_t id : saved.ids) {
        if (!id) continue;
        if (find(id) < 0 || !session::readable(id)) return false;
        ++countRead;
    }
    return countRead == count && (!saved.selected || hasSelection);
}
bool Library::save(const Sheet &s, uint32_t replacing) {
    if (!writable || !s.id) return false;
    bool replace = replacing && replacing != s.id;
    int i = find(replace ? replacing : s.id);
    if (replace && (i < 0 || find(s.id)>=0)) return false;
    bool add = i < 0;
    if (add && count == capacity) return false;
    if (prefs.putBytes(key(s.id).c_str(), &s, sizeof(s)) != sizeof(s)) return false;
    if (add || replace) {
        if (add) i = count;
        Catalog c = catalog(*this);
        c.ids[i] = s.id;
        if (replace && i == active) c.active = s.id;
        if (!commit(c)) return false;
        if (add) ++count;
        else { prefs.remove(key(replacing).c_str()); removePortrait(replacing); play::forgetMix(replacing); }
    }
    records[i] = s;
    return true;
}
bool Library::remove(uint32_t id) {
    int i = find(id);
    if (!id || i < 0) return false;
    Catalog c = catalog(*this);
    for (int j=i; j<capacity-1; ++j) c.ids[j] = c.ids[j+1];
    c.ids[capacity-1] = 0;
    if (i == active) { c.active=c.ids[0];c.selected=c.active!=0; }
    if (!commit(c)) return false;
    for (int j=i; j<count-1; ++j) records[j] = records[j+1];
    --count;
    records[count]={};
    active = find(c.active);
    if (active<0) active=0;
    hasSelection=c.selected;
    prefs.remove(key(id).c_str());
    removePortrait(id);play::forgetMix(id);
    return true;
}
bool Library::select(int i) {
    if (i < 0 || i >= count) return false;
    auto c = catalog(*this);
    c.active = records[i].id;
    c.selected = 1;
    bool ok = commit(c);
    active = i;
    hasSelection = true;
    return ok;
}
const char *Library::pickerArt() const {
    return appearance==1?"icon":appearance==2?"background":"both";
}
bool Library::setPickerArt(const String &value) {
    if (value!="icon" && value!="background" && value!="both") return false;
    uint8_t next=value=="icon"?1:value=="background"?2:0;
    if (!writable || prefs.putUChar("pickerArt",next)!=1) return false;
    appearance=next;
    return true;
}
