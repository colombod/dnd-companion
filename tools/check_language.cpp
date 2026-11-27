#include "../firmware/Companion/ui_language.h"
#include "Preferences.h"
#include <cassert>
#include <cstring>

int main(){
    ui::loadLanguage();
    assert(!strcmp(ui::code(),"en") && !strcmp(ui::tr("History"),"History"));
    assert(ui::setLanguage("it") && !strcmp(ui::tr("History"),"Storia"));
    ui::loadLanguage();
    assert(!strcmp(ui::code(),"it") && !strcmp(ui::tr("Perception"),"Percezione"));
    assert(!ui::setLanguage("zz") && !strcmp(ui::code(),"it"));
    Preferences::failKey="language";
    assert(!ui::setLanguage("es") && !strcmp(ui::code(),"it"));
    Preferences::failKey.clear();
    assert(ui::setLanguage("es") && !strcmp(ui::tr("Perception"),"Percepción"));
    assert(!strcmp(ui::tr("My homebrew spell"),"My homebrew spell"));
    Preferences::data["dnd-ui/language"]={255};
    ui::loadLanguage();
    assert(!strcmp(ui::code(),"en"));
}
