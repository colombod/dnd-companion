#include "../firmware/Companion/library.h"
#include "../firmware/Companion/rim.h"
#include "../firmware/Companion/networks.h"
#include "Preferences.h"
#include "WiFi.h"
#include <cassert>
#include <iostream>

void removePortrait(uint32_t){}
namespace play {void forgetMix(uint32_t){}}
int main(){
    using dial::Rim;
    assert(dial::inRim(180,23,Rim::Top) && dial::inRim(290,70,Rim::Top));
    assert(dial::inRim(180,338,Rim::Bottom) && dial::inRim(70,290,Rim::Bottom));
    assert(!dial::inRim(25,25,Rim::Top) && !dial::inRim(180,310,Rim::Bottom));
    assert(!dial::inRim(62,282,Rim::Bottom) && !dial::inRim(180,23,Rim::Bottom));
    for (int x=0;x<360;x++) for (int y=0;y<360;y++) {
        int regions=dial::inHub(x,y)+dial::inRim(x,y,Rim::Top)+dial::inRim(x,y,Rim::Bottom);
        assert(regions<=1);
    }
    Preferences legacy;
    legacy.begin("dnd-library1");
    Sheet original{42,"Birch","Druid 4",100,4};
    uint32_t id=42;
    legacy.putBytes("ids",&id,sizeof(id));
    legacy.putBytes("active",&id,sizeof(id));
    legacy.putBytes("c42",&original,offsetof(Sheet,tag));
    Library library;library.begin();
    assert(std::string(library.pickerArt())=="both");
    assert(library.setPickerArt("icon") && !library.setPickerArt("bad"));
    assert(library.hasSelection && library.records[library.active].level==4);
    Sheet other{24,"Quill","Rogue 2",110,2};
    assert(library.save(other));
    assert(!library.save(other,42)); // Duplicate replacement.
    Sheet replacement{11,"Vale","Bard 1",112,1};
    Preferences::failKey="catalog3";
    assert(!library.save(replacement,42) && library.find(42)>=0 && library.find(11)<0);
    assert(!library.remove(42) && library.find(42)>=0);
    Preferences::failKey.clear();
    assert(library.save(replacement,42));
    Library reboot;reboot.begin();
    assert(std::string(reboot.pickerArt())=="icon");
    assert(reboot.find(42)<0 && reboot.records[reboot.active].id==11);
    assert(reboot.remove(11) && reboot.remove(24) && !reboot.remove(0));
    Library empty;empty.begin();
    assert(empty.count==0 && empty.active==0); // Removed seed stays removed at boot.
    auto saved=original;
    auto incoming=original;
    strcpy(incoming.tag.revision,"2026-10-02");
    assert(!sheetChanged(saved,incoming)); // Discovering first revision alone needs no review.
    strcpy(saved.tag.revision,"2026-10-01");
    assert(sheetChanged(saved,incoming));
    incoming=saved;strcpy(incoming.classes,"Druid 5");incoming.level=5;
    assert(sheetChanged(saved,incoming) && saved.level==4);

    legacy.begin("dnd-network1");
    legacy.putBytes("ssid","Table",5);legacy.putBytes("pass","test-only",9);
    networksBegin();
    assert(!legacy.isKey("pass") && WiFi.attempted=="Table" && WiFi.supplied=="test-only");
    WiFi.online=true;WiFi.associated="Table";networksPoll();
    assert(!connectNetwork("Table","bad",false));
    fakeMillis+=25000;networksPoll();
    assert(!connectNetwork("Table","",true) && WiFi.supplied=="test-only"); // failed edit preserved
    WiFi.online=true;WiFi.associated="Table";networksPoll();
    assert(!connectNetwork("Tavern","second-test",false));
    WiFi.online=true;WiFi.associated="Tavern";networksPoll();
    scanNetworks();WiFi.scan=3;networksPoll();
    JsonDocument doc;networkStatus(doc);
    assert(doc["saved"].size()==2 && doc["nearby"].size()==2);
    std::string response;serializeJson(doc,response);
    assert(response.find("second-test")==std::string::npos && response.find("test-only")==std::string::npos);
    assert(!forgetNetwork("Tavern") && !WiFi.online && connectNetwork("Tavern","",true));
    assert(!connectNetwork("Table","",true) && WiFi.supplied=="test-only");
    std::cout<<"Catalog migration, edits, deletion, failed writes, source diff, saved Wi-Fi, scan, forget and credential redaction passed.\n";
}
