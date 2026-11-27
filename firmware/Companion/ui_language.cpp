#include "ui_language.h"
#include <Preferences.h>
#include <algorithm>
#include <cstring>

namespace ui {
namespace {
Preferences preferences;
bool writable;
Language selected=Language::English;
constexpr const char *codes[]={"en","it","es"};
struct Message {const char *en,*it,*es;};
constexpr Message catalog[]={
#include "messages.inc"
};
}
Language language(){return selected;}
const char *code(){return codes[static_cast<unsigned>(selected)];}
void loadLanguage(){
    writable=preferences.begin("dnd-ui");
    unsigned value=writable?preferences.getUChar("language",0):0;
    selected=value<3?static_cast<Language>(value):Language::English;
}
bool setLanguage(const char *value){
    for(unsigned i=0;i<3;i++)if(!strcmp(codes[i],value)){
        if(selected==static_cast<Language>(i))return true;
        if(!writable || preferences.putUChar("language",i)!=1)return false;
        selected=static_cast<Language>(i);
        return true;
    }
    return false;
}
const char *tr(const char *english){
    if(selected==Language::English || !english || !*english)return english;
    auto end=std::end(catalog);
    auto entry=std::lower_bound(std::begin(catalog),end,english,[](const Message &m,const char *s){return strcmp(m.en,s)<0;});
    if(entry==end || strcmp(entry->en,english))return english;
    return selected==Language::Italian?entry->it:entry->es;
}
}
