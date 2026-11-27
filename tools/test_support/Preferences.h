#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
class Preferences {
    std::string ns;
public:
    inline static std::map<std::string,std::vector<uint8_t>> data;
    inline static std::string failKey;
    bool begin(const char *key) { ns=std::string(key)+"/"; return true; }
    bool isKey(const char *k) { return data.count(ns+k); }
    bool remove(const char *k) { return data.erase(ns+k); }
    size_t getBytesLength(const char *k) { return isKey(k)?data[ns+k].size():0; }
    size_t getBytes(const char *k,void *out,size_t cap) {
        auto n=getBytesLength(k); if (!n || n>cap) return 0;
        memcpy(out,data[ns+k].data(),n); return n;
    }
    size_t putBytes(const char *k,const void *p,size_t n) {
        if (failKey==k) return 0;
        auto *b=static_cast<const uint8_t *>(p);
        data[ns+k]={b,b+n}; return n;
    }
    uint8_t getUChar(const char *k,uint8_t fallback) {uint8_t v;return getBytes(k,&v,1)?v:fallback;}
    size_t putUChar(const char *k,uint8_t v) {return putBytes(k,&v,1);}
    uint32_t getUInt(const char *k,uint32_t fallback) {
        uint32_t v{};return getBytes(k,&v,sizeof(v))?v:fallback;
    }
    String getString(const char *k) {
        if (!isKey(k)) return {};
        const auto &b=data[ns+k]; return std::string(b.begin(),b.end());
    }
};

// Host counterpart of the two flash banks used by the session store.
namespace sessionfiles {
inline std::map<std::pair<uint32_t,bool>,std::vector<uint8_t>> files;
inline bool failWrite=false,corruptWrite=false;
inline bool read(uint32_t id,bool bank,void *out,size_t bytes){
    auto it=files.find({id,bank});
    if(it==files.end() || it->second.size()!=bytes)return false;
    memcpy(out,it->second.data(),bytes);return true;
}
inline bool write(uint32_t id,bool bank,const void *record,size_t bytes){
    if(failWrite)return false;
    auto *p=static_cast<const uint8_t*>(record);
    auto &file=files[{id,bank}];file.assign(p,p+bytes);
    if(corruptWrite)file.back()^=1;
    return true;
}
inline void erase(uint32_t id){files.erase({id,false});files.erase({id,true});}
}
