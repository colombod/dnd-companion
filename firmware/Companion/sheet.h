#pragma once
#include "check_import.h"
#include "move_import.h"
#ifdef ARDUINO_ARCH_ESP32
#include <esp_heap_caps.h>
class RollJsonMemory : public ArduinoJson::Allocator {
public:
    void *allocate(size_t n) override {return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
    void deallocate(void *p) override {heap_caps_free(p);}
    void *reallocate(void *p,size_t n) override {return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
};
#endif
#include <cstdint>
#include <cstdio>
#include <cstring>

struct alignas(uint32_t) SourceTag {
    char etag[128];
    char lastModified[48];
    char revision[40];
};
struct Sheet {
    uint32_t id;
    char name[64];
    char classes[120];
    uint32_t synced;
    uint8_t level;
    SourceTag tag{};
    char avatar[400]{};
    checks::Snapshot rolls{};
    uint32_t bookHash{};
    uint8_t bookBank{},bookVersion{},wornArmor{},reserved{};
};
inline bool sameSummary(const Sheet &a, const Sheet &b) {
    return a.id == b.id && a.level == b.level && !strcmp(a.name,b.name) && !strcmp(a.classes,b.classes);
}
inline bool sheetChanged(const Sheet &old, const Sheet &incoming) {
    return !sameSummary(old,incoming) || (old.bookVersion==incoming.bookVersion && old.bookHash && old.bookHash!=incoming.bookHash) || (old.rolls.version && memcmp(&old.rolls,&incoming.rolls,sizeof(old.rolls))!=0) || (old.tag.revision[0] && incoming.tag.revision[0] &&
           strcmp(old.tag.revision,incoming.tag.revision)!=0) || (old.avatar[0] && strcmp(old.avatar,incoming.avatar)!=0);
}

template<class Reader>
const char *parseSheet(Reader &source, uint32_t expected, Sheet &out,JsonDocument *book=nullptr) {
    JsonDocument filter;
#ifdef ARDUINO_ARCH_ESP32
    RollJsonMemory memory;
    JsonDocument doc(&memory);
#else
    JsonDocument doc;
#endif
    checks::filterRollData(filter);
    if(book)moves::filter(filter);
    filter["success"] = true;
    auto data = filter["data"];
    data["id"] = data["name"] = true;
    data["dateModified"] = true;
    data["decorations"]["avatarUrl"] = true;
    data["classes"][0]["level"] = true;
    data["classes"][0]["definition"]["name"] = true;
    if (filter.overflowed()) return "Could not allocate sheet filter";
    auto error = deserializeJson(doc, source, DeserializationOption::Filter(filter),
                                 DeserializationOption::NestingLimit(60));
    if (error) return "Incomplete or unreadable sheet";
    JsonObject sheet = doc["data"];
    if (!doc["success"].as<bool>() || sheet["id"].as<uint32_t>() != expected || !expected)
        return "Link and sheet do not match";
    Sheet next{};
    next.id = expected;
    const char *revision = sheet["dateModified"] | "";
    if (strlen(revision) < sizeof(next.tag.revision)) strcpy(next.tag.revision, revision);
    const char *avatar=sheet["decorations"]["avatarUrl"] | "";
    if (strlen(avatar)<sizeof(next.avatar)) strcpy(next.avatar,avatar);
    const char *name = sheet["name"] | "";
    if (!name[0] || strlen(name) >= sizeof(next.name)) return "Unsupported character name";
    strcpy(next.name, name);
    size_t used = 0;
    for (JsonObject cls : sheet["classes"].as<JsonArray>()) {
        int level = cls["level"] | 0;
        const char *title = cls["definition"]["name"] | "";
        if (!title[0] || level < 1 || level > 20 || next.level + level > 20) return "Invalid class data";
        int n = snprintf(next.classes + used, sizeof(next.classes) - used, "%s%s %d",
                         used ? " / " : "", title, level);
        if (n < 0 || size_t(n) >= sizeof(next.classes) - used) return "Class list too long";
        used += n;
        next.level += level;
    }
    if (!next.level) return "Class levels missing";
    checks::importRolls(sheet,next.rolls);
    if(book && !moves::extract(sheet,*book,next.level))return "Spell or action data incomplete";
    if(book){next.bookVersion=casting::version;next.wornArmor=(*book)["gear"]["armored"]==true;}
    out = next;
    return nullptr;
}
