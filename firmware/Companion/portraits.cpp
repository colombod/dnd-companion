#include "portraits.h"
#include "face.h"
#include <Preferences.h>
#include <FFat.h>
#include <esp_partition.h>
#include "session_store.h"
#include "session_files.h"

namespace {
Preferences meta;
bool opened, mounted;
bool ready() {
    if (!opened) opened=meta.begin("dnd-portraits");
    return opened;
}
bool mount() {
    if (mounted) return true;
    auto *part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_FAT,"ffat");
    if (!part) return false;
    // Initialize only a fully erased partition. Preserve existing files on a mount error.
    bool blank=true;
    uint8_t block[1024];
    for (size_t at=0; blank && at<part->size; at+=sizeof(block)) {
        if (esp_partition_read(part,at,block,sizeof(block))!=ESP_OK) return false;
        for (uint8_t byte:block) if (byte!=255) {blank=false;break;}
    }
    mounted=FFat.begin(blank);
    return mounted;
}
bool isIcon(ArtLayer layer) { return layer==ArtLayer::Icon; }
String key(uint32_t id,ArtLayer layer) { return String(isIcon(layer)?"a":"i")+id; }
String path(uint32_t id,ArtLayer layer,bool slot) {
    return String(isIcon(layer)?"/a":"/p")+id+(slot?"b.jpg":"a.jpg");
}
// Background flags retain compatibility with older per-character phone uploads.
// Icons commit their source fingerprint and bank in one NVS write.
uint64_t record(uint32_t id,ArtLayer layer) {
    if (!ready()) return 0;
    String k=key(id,layer);
    return isIcon(layer)?meta.getULong64(k.c_str(),0):meta.getUChar(k.c_str(),0);
}
struct Cache {
    uint32_t id=0;
    bool loaded=false, dice=false,book=false;
    ImageBuffer buffer;
};
Cache background, icon;
Cache &cache(ArtLayer layer) { return isIcon(layer)?icon:background; }
const lv_img_dsc_t *load(uint32_t id, ArtLayer layer) {
    Cache &c=cache(layer);
    if (c.loaded && !c.dice && !c.book && c.id==id) return &c.buffer.image;
    c.loaded=false;
    c.dice=false;
    c.book=false;
    auto flags=record(id,layer);
    if (flags && (isIcon(layer) || (flags&4)) && mount()) {
        auto file=FFat.open(path(id,layer,flags&1),"r");
        size_t size=file?file.size():0;
        if (size && size<=(isIcon(layer)?iconLimit:backgroundLimit)) {
            auto *bytes=static_cast<uint8_t *>(ps_malloc(size));
            c.loaded=bytes && file.read(bytes,size)==size &&
                c.buffer.decode(bytes,size,isIcon(layer)?iconSize:faceSize,!isIcon(layer));
            free(bytes);
        }
    }
    if (!c.loaded && !isIcon(layer)) {
        static const uint8_t genericArt[]={
#include "fallback_art.inc"
        };
        c.loaded=c.buffer.decode(genericArt,sizeof(genericArt),faceSize,true);
    }
    c.id=id;
    return c.loaded?&c.buffer.image:nullptr;
}
bool erase(uint32_t id,ArtLayer layer) {
    if (!ready()) return false;
    auto k=key(id,layer);
    if (!meta.isKey(k.c_str())) return true;
    if (!meta.remove(k.c_str())) return false;
    cache(layer).loaded=false;
    if (mount()) {
        for (bool slot:{false,true}) {
            String file=path(id,layer,slot);
            if (FFat.exists(file)) FFat.remove(file);
        }
    }
    return true;
}
}
const lv_img_dsc_t *backgroundFor(uint32_t id) {return load(id,ArtLayer::Background);}
const lv_img_dsc_t *iconFor(uint32_t id) {return load(id,ArtLayer::Icon);}
bool hasBackground(uint32_t id) {return record(id,ArtLayer::Background)&4;}
uint32_t iconSource(const char *url) {
    if (!url || !*url) return 0;
    uint32_t h=2166136261u;
    while (*url) h=(h^static_cast<uint8_t>(*url++))*16777619u;
    return h?h:1;
}
bool iconCurrent(uint32_t id,uint32_t source) {
    return source && (record(id,ArtLayer::Icon)>>8)==source;
}
bool saveArtwork(uint32_t id,ArtLayer layer,const uint8_t *jpeg,size_t length,uint32_t source) {
    const bool avatar=isIcon(layer);
    if (!id || !jpeg || !length || length>(avatar?iconLimit:backgroundLimit) ||
        (avatar && !source) || !validImage(jpeg,length,avatar?iconSize:faceSize)) return false;
    uint64_t old=record(id,layer);
    if (!opened || !mount()) return false;
    bool slot=!(old&1);
    String name=path(id,layer,slot);
    auto file=FFat.open(name,"w");
    if (!file) return false;
    bool written=file.write(jpeg,length)==length;
    file.flush();file.close();
    auto check=FFat.open(name,"r");
    written=written && check && check.size()==length;
    check.close();
    String k=key(id,layer);
    bool committed=written && (avatar ?
        meta.putULong64(k.c_str(),(uint64_t(source)<<8)|2|slot)==sizeof(uint64_t) :
        meta.putUChar(k.c_str(),6|slot)==1);
    if (!committed) {FFat.remove(name);return false;}
    cache(layer).loaded=false;
    if (old) FFat.remove(path(id,layer,old&1));
    return true;
}
bool removeBackground(uint32_t id) {return erase(id,ArtLayer::Background);}
static String bookPath(uint32_t id,bool bank){return String("/m")+id+(bank?"b.bin":"a.bin");}
void removePortrait(uint32_t id) {
    session::forget(id);
    erase(id,ArtLayer::Icon);erase(id,ArtLayer::Background);
    if(mount())for(bool bank:{false,true})if(FFat.exists(bookPath(id,bank)))FFat.remove(bookPath(id,bank));
}
bool writeMoveBook(uint32_t id,bool bank,const uint8_t *data,size_t size){
    if(!mount() || !data || !size || size>96*1024)return false;
    auto file=FFat.open(bookPath(id,bank),"w");
    if(!file)return false;
    bool ok=file.write(data,size)==size;file.flush();file.close();
    auto check=FFat.open(bookPath(id,bank),"r");
    return ok && check && check.size()==size;
}
bool readMoveBook(uint32_t id,bool bank,uint32_t hash,JsonDocument &book){
    if(!mount() || !hash)return false;
    auto file=FFat.open(bookPath(id,bank),"r");
    if(!file || !file.size() || file.size()>96*1024)return false;
    uint32_t h=2166136261u;uint8_t chunk[256];
    while(file.available()){
        auto n=file.read(chunk,sizeof(chunk));if(!n)return false;
        for(size_t i=0;i<n;i++)h=(h^chunk[i])*16777619u;
    }
    return (h?h:1)==hash && file.seek(0) && !deserializeMsgPack(book,file);
}

const lv_img_dsc_t *playBackdrop(bool dice) {
    if (!dice) return backgroundFor(0);
    static const uint8_t artwork[]={
#include "dice_face.inc"
    };
    if (!background.loaded || !background.dice) {
        background.loaded=background.buffer.decode(artwork,sizeof(artwork),faceSize,true);
        background.dice=true;
        background.book=false;
    }
    return background.loaded?&background.buffer.image:nullptr;
}
const lv_img_dsc_t *bookBackdrop(){
    static const uint8_t art[]={
#include "spell_face.inc"
    };
    if(!background.loaded || !background.book){
        background.loaded=background.buffer.decode(art,sizeof(art),faceSize,true);
        background.book=true;background.dice=false;
    }
    return background.loaded?&background.buffer.image:nullptr;
}
const lv_img_dsc_t *bootBackdrop(){
    static const uint8_t art[]={
#include "boot_face.inc"
    };
    background.loaded=false;background.book=background.dice=false;
    return background.buffer.decode(art,sizeof(art),faceSize,false)?&background.buffer.image:nullptr;
}

namespace sessionfiles {
static String path(uint32_t id,bool bank){return String("/s")+id+(bank?"b.bin":"a.bin");}
bool read(uint32_t id,bool bank,void *record,size_t bytes){
    if(!mount())return false;
    auto file=FFat.open(path(id,bank),"r");
    return file && file.size()==bytes && file.read(static_cast<uint8_t*>(record),bytes)==bytes;
}
bool write(uint32_t id,bool bank,const void *record,size_t bytes){
    if(!mount())return false;
    auto file=FFat.open(path(id,bank),"w");
    if(!file)return false;
    bool written=file.write(static_cast<const uint8_t*>(record),bytes)==bytes;
    file.flush();file.close();return written;
}
void erase(uint32_t id){
    if(!mount())return;
    for(bool bank:{false,true}){
        auto name=path(id,bank);if(FFat.exists(name))FFat.remove(name);
    }
}
}
