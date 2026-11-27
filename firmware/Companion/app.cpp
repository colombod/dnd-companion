#include "ui_language.h"
#include "app.h"
#include "ota.h"
#include "board.h"
#include "play.h"
#include "dice_gestures.h"
#include "book_ui.h"
#include "boot_scene.h"
#include "resources_ui.h"
#include "health_ui.h"
#include "pack_ui.h"
#include "library.h"
#include "refresh.h"
#include "rules.h"
#include "networks.h"
#include "portraits.h"
#include "face.h"
#include "rim_text.h"
using dial::Rim;
#include "setup_wifi.h"
#include "round_controls.h"
#include "profile_state.h"
#include <WiFi.h>
#include <ctime>

enum class Page { Boot, Picker, Sheet, Setup, Review, Device, Play, Book, Resources, Language, Firmware, Health, Pack };
static int sheetPair;
static void shiftPair(int by){sheetPair=((sheetPair+by)%5+5)%5;}
static Library library;
static Page page = Page::Boot;
static int candidate;
static uint32_t fetching, noticeId, bootStart;
static String notice;
static uint32_t replacing, reviewTicket, reviewOld, checkedId, checkedAt;
static bool discardFetch,repairBook;
static Sheet proposed{};
static String oldPackDetail,newPackDetail;
// Read only verified bank contents. A background review must not replace the book on the play screen.
static void comparePack(const Sheet &old,const Sheet &next){
    oldPackDetail="";newPackDetail="";
    if(!sameSummary(old,next) || old.bookVersion!=casting::version || next.bookVersion!=casting::version ||
       old.bookHash==next.bookHash)return;
    RollJsonMemory memory;JsonDocument a(&memory),b(&memory);
    if(!readMoveBook(old.id,old.bookBank,old.bookHash,a) || !readMoveBook(next.id,next.bookBank,next.bookHash,b))return;
    auto left=pack::source(a),right=pack::source(b);
    if(left.find && right.find && a["pack"]!=b["pack"]){
        for(auto book:{&b,&a})for(auto row:(*book)["pack"].as<JsonArrayConst>()){
            uint32_t id=row["id"]|0u;pack::Item was,now;
            bool had=left.item(id,was),has=right.item(id,now);
            if(had==has && was.stock==now.stock)continue;
            String name=row["name"]|"?";
            oldPackDetail=(had?String(was.stock):String("0"))+" × "+name;
            newPackDetail=(has?String(now.stock):String("0"))+" × "+name;
            return;
        }
    }
    for(int i=0;i<pack::coinCount;++i)if(left.money[i]>=0 && right.money[i]>=0 && left.money[i]!=right.money[i]){
        oldPackDetail=String(left.money[i])+" "+pack::coins[i];
        newPackDetail=String(right.money[i])+" "+pack::coins[i];return;
    }
}
static int changedRoll(const Sheet &old,const Sheet &next) {
    if(!sameSummary(old,next) || !old.rolls.version || !next.rolls.version)return -1;
    int rulesOnly=-1;
    for(int i=0;i<checks::count;i++){
        int a=0,b=0;
        bool haveA=checks::Prepared(old.rolls,old.level,i).modifier(a);
        bool haveB=checks::Prepared(next.rolls,next.level,i).modifier(b);
        if(haveA!=haveB || a!=b)return i;
        if(rulesOnly<0 && memcmp(&old.rolls.rows[i],&next.rolls.rows[i],sizeof(checks::Entry)))rulesOnly=i;
    }
    return rulesOnly;
}
static String reviewDetail(const Sheet &s,int row){
    if(row<0)return ui::tr("Level ")+String(s.level)+"   •   "+s.classes;
    int bonus;
    String text=ui::tr(checks::name(row));
    if(row>=checks::saves)text+=ui::tr(" save");
    else if(row>=checks::plain)text+=ui::tr(" check");
    if(!checks::Prepared(s.rolls,s.level,row).modifier(bonus))return text+"  ?";
    text+="  ";if(bonus>=0)text+="+";text+=bonus;
    const auto &e=s.rolls.rows[row];
    if(e.flags&checks::Concentration)return text+ui::tr("  •  concentration");
    if(e.flags&checks::Conditional)return text+ui::tr("  •  situational");
    if(e.flags&checks::Advantage)return text+ui::tr("  •  advantage");
    if(e.flags&checks::Disadvantage)return text+ui::tr("  •  disadvantage");
    if(e.flags&checks::Override)return text+ui::tr("  •  override");
    const char *training[]={"",ui::tr("  •  half"),ui::tr("  •  proficient"),ui::tr("  •  expert")};
    return text+training[e.training];
}
struct ImportJob { uint32_t id; bool finished; String message; };
static ImportJob jobs[Library::capacity];
static int jobCount;
static bool redraw = true, wasOnline;
static String networkName;
static lv_obj_t *railLabel;
static Page statusReturn=Page::Picker;
const Sheet *getSheet(uint32_t id) { int i=library.find(id);return i>=0?&library.records[i]:nullptr; }
void artworkChanged() {redraw=true;}
bool setDisplayLanguage(const String &code){
    if(!ui::setLanguage(code.c_str()))return false;
    redraw=true;
    return true;
}
bool setPickerArt(const String &mode) {
    if (!library.setPickerArt(mode)) return false;
    redraw=true;
    return true;
}

static const uint32_t ink = 0x111521, gold = 0xd9b478, paper = 0xf0e7d6, teal = 0x88cabb;

static lv_obj_t *textAt(const String &s, int y, int width, const lv_font_t *font, uint32_t color,
                        lv_label_long_mode_t mode=LV_LABEL_LONG_DOT) {
    auto *obj = lv_label_create(lv_scr_act());
    lv_obj_set_width(obj, width);
    lv_label_set_long_mode(obj, mode);
    lv_obj_set_height(obj, lv_font_get_line_height(font) + 2);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_text(obj, s.c_str());
    lv_obj_set_pos(obj, (360 - width) / 2, y);
    return obj;
}
static void circle(int diameter, int centerY, uint32_t color, int border) {
    auto *obj = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, diameter, diameter);
    lv_obj_set_pos(obj, (360-diameter)/2, centerY-diameter/2);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(obj, border, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(color), 0);
}
static bool phoneJoined, showPageCode;
static void setupQR() {
    bool url = showPageCode;
    String payload = url ? "http://192.168.4.1/" : "WIFI:T:WPA;S:" + accessName + ";P:" + accessPassword + ";;";
    arcText(url?ui::tr("Open setup"):ui::tr("Pair your phone"),ArcLabel::PlayTitle,gold);
    auto *quiet = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(quiet);
    lv_obj_set_pos(quiet, 79, 61);
    lv_obj_set_size(quiet, 202, 202);
    lv_obj_set_style_bg_color(quiet, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(quiet, LV_OPA_COVER, 0);
    auto *qr = lv_qrcode_create(quiet, 166, lv_color_black(), lv_color_white());
    if (qr && lv_qrcode_update(qr, payload.c_str(), payload.length()) == LV_RES_OK) lv_obj_center(qr);
    else textAt(url ? "192.168.4.1" : accessName, 145, 198, &lv_font_montserrat_20, ink);
    textAt(url ? ui::tr("Scan to add character links") : ui::tr("Scan to join the dial"), 270, 259, &lv_font_montserrat_14, paper);
    returnArc(ui::tr("Done  •  Library"));
}
static String syncLabel(const Sheet &s) {
    if (fetching && fetching == s.id) return ui::tr("Refreshing from Beyond...");
    if (notice.length() && noticeId == s.id) return ui::tr(notice.c_str());
    if (!s.id) return ui::tr("Local character");
    if (!s.synced) return ui::tr("Saved on dial");
    time_t t = s.synced;
    struct tm utc{};
    gmtime_r(&t, &utc);
    char date[40];
    strftime(date,sizeof(date),"%Y-%m-%d %H:%M UTC",&utc);
    return String(ui::tr("Cached"))+" "+date;
}
static void render() {
    redraw = false;
    auto *screen = lv_scr_act();
    railLabel=nullptr;
    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(ink), 0);
    circle(352, 180, 0x755b3c, 3);
    if (page == Page::Boot) {
        boot::draw();
        return;
    }
    if (page == Page::Setup) {
        setupQR();
        return;
    }
    if (page == Page::Firmware) {
        const auto &v = ota::view();
        using ota::Stage;
        arcText(ui::tr("Firmware"), ArcLabel::PlayTitle, gold);
        textAt(String(ui::tr("Installed")) + "   v" + ota::version(), 72, 268, &lv_font_montserrat_18, paper);
        textAt(String(ui::tr("Build ID")) + "   " + ota::buildId(), 99, 300, &lv_font_montserrat_18, teal);
        const char *heading = "Check for updates";
        switch (v.stage) {
            case Stage::RestorePreview: heading = "Restore firmware?"; break;
            case Stage::Checking: heading = "Checking..."; break;
            case Stage::Available: heading = "Update available"; break;
            case Stage::Current: heading = "Up to date"; break;
            case Stage::Installing: heading = "Installing"; break;
            case Stage::Restarting: heading = "Restarting..."; break;
            case Stage::Failed: heading = v.message; break;
            default: break;
        }
        textAt(ui::tr(heading), 130, 300, &lv_font_montserrat_20, paper);
        if (v.stage == Stage::Available) {
            textAt(String("v") + v.release.version + "   •   " + String(v.release.size / 1048576.0, 2) + " MiB",
                   159, 285, &lv_font_montserrat_18, gold);
            textAt(v.release.note, 191, 274, &lv_font_montserrat_14, paper, LV_LABEL_LONG_SCROLL_CIRCULAR);
        } else if (v.stage == Stage::RestorePreview) {
            textAt(String("v") + ota::previousVersion(), 161, 285, &lv_font_montserrat_20, gold);
            textAt(ui::tr("Keep characters and Wi-Fi"), 192, 274, &lv_font_montserrat_14, paper);
        } else if (!ota::busy() && ota::previousVersion()) {
            dial::Capsule{180,190,218,44}.draw(ui::tr("Previous firmware"), ink, teal);
        } else if (v.stage == Stage::Installing) {
            textAt(String(v.percent) + "%", 173, 205, &lv_font_montserrat_36, gold);
            textAt(ui::tr("Keep the dial powered"), 226, 250, &lv_font_montserrat_14, paper);
        }
        if (!ota::busy()) {
            const char *action = v.stage == Stage::Available ? "Install update" : "Check now";
            if (v.stage == Stage::RestorePreview) action = "Restore now";
            dial::Capsule{180,249,192,46}.draw(ui::tr(action), ink, gold);
        }
        if (v.stage != Stage::Installing && v.stage != Stage::Restarting)
            returnArc(ui::tr("Device status"));
        return;
    }
    if(page==Page::Language){
        arcText(ui::tr("Language"),ArcLabel::PlayTitle,gold);
        const char *names[]={"English","Italiano","Español"};
        for(int i=0;i<3;i++){
            bool active=int(ui::language())==i;
            dial::Capsule{180,110+57*i,190,46}.draw(names[i],active?ink:paper,active?gold:0x283b40);
        }
        returnArc(ui::tr("Device status"));
        return;
    }
    if (page==Page::Device) {
        arcText(ui::tr("Device status"),ArcLabel::PlayTitle,gold);
        textAt(WiFi.isConnected()?ui::tr("Wi-Fi connected"):ui::tr("Wi-Fi offline"),94,290,&lv_font_montserrat_20,paper);
        if (WiFi.isConnected()) textAt(networkName,122,276,&lv_font_montserrat_14,teal,LV_LABEL_LONG_SCROLL_CIRCULAR);
        String power=powerSummary();
        textAt(power=="USB" ? ui::tr("USB connected") : power=="External" ? ui::tr("External power") : ui::tr("Battery ")+power,153,300,&lv_font_montserrat_20,paper);
        railLabel=textAt(ui::tr("Supply rail ")+String(railMillivolts()/1000.0,2)+" V",185,280,&lv_font_montserrat_14,teal);
        textAt(ui::tr("Charging status unavailable"),210,280,&lv_font_montserrat_14,paper);
        dial::Capsule{112,253,120,44}.draw(ui::tr("Language"),ink,gold);
        dial::Capsule{248,253,120,44}.draw(ui::tr("Firmware"),ink,gold);
        returnArc(statusReturn==Page::Picker?ui::tr("Back to library"):ui::tr("Back to sheet"));
        return;
    }
    if (page == Page::Review && reviewTicket) {
        int i=library.find(reviewOld);
        int row=i>=0?changedRoll(library.records[i],proposed):-1;
        arcText(ui::tr("Sheet update"),ArcLabel::PlayTitle,gold);
        textAt(ui::tr(row<0 && oldPackDetail.length()?"Cached sheet":"ON YOUR DIAL"), 81, 265, &lv_font_montserrat_14, teal);
        if (i>=0) {
            textAt(library.records[i].name, 101, 285, &lv_font_montserrat_20, paper);
            textAt(row<0 && oldPackDetail.length()?oldPackDetail:reviewDetail(library.records[i],row), 129, 298, &lv_font_montserrat_14, paper);
        }
        textAt(ui::tr("FROM D&D BEYOND"), 158, 284, &lv_font_montserrat_14, teal);
        textAt(proposed.name, 178, 298, &lv_font_montserrat_20, paper);
        textAt(row<0 && newPackDetail.length()?newPackDetail:reviewDetail(proposed,row), 207, 295, &lv_font_montserrat_14, paper);
        dial::Capsule{180,259,174,44}.draw(ui::tr("Apply update"),ink,gold);
        returnArc(ui::tr("Keep current"));
        return;
    }
    if (page==Page::Play && library.count && library.records[library.active].id==play::owner()) {
        const auto &sheet=library.records[library.active];
        play::draw(sheet,syncLabel(sheet));return;
    }
    if (page==Page::Play) page=Page::Picker;
    if(page==Page::Pack){
        if(library.count && library.records[library.active].id==packui::owner()){packui::draw(library.records[library.active]);return;}
        page=Page::Picker;
    }
    if(page==Page::Health){
        if(library.count && library.records[library.active].id==healthui::owner()){healthui::draw(library.records[library.active]);return;}
        page=Page::Picker;
    }
    if(page==Page::Resources){
        if(library.count && library.records[library.active].id==resources::owner()){const auto &s=library.records[library.active];resources::draw(s,syncLabel(s));return;}
        page=Page::Picker;
    }
    if(page==Page::Book){
        if(library.count && library.records[library.active].id==bookui::owner()){
            const auto &s=library.records[library.active];bookui::draw(s,syncLabel(s));return;
        }
        page=Page::Picker;
    }
    bool picking = page == Page::Picker;
    if (!library.count) {
        arcText(ui::tr("Device status"),ArcLabel::Status,paper);
        textAt(ui::tr("Your characters"),125,296,&lv_font_montserrat_28,paper);
        textAt(ui::tr("Add your first character by link"),184,296,&lv_font_montserrat_14,teal);
        arcText(String(LV_SYMBOL_SETTINGS "  ")+ui::tr("Phone setup"),ArcLabel::Action,teal);
        return;
    }
    const auto &s = library.records[picking ? candidate : library.active];
    const String mode=picking?library.pickerArt():"both";
    if (mode!="icon") if (const auto *portrait = backgroundFor(s.id)) {
        auto *img = lv_img_create(screen);
        lv_img_set_src(img, portrait);
        lv_obj_set_pos(img, 0, 0);
    }
    if (mode!="background") {
        circle(136,180,gold,2);
        auto *frame=lv_obj_create(screen);
        lv_obj_remove_style_all(frame);
        lv_obj_set_size(frame,iconSize,iconSize);
        lv_obj_center(frame);
        lv_obj_set_style_radius(frame,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_clip_corner(frame,true,0);
        lv_obj_set_style_bg_color(frame,lv_color_hex(0x22303d),0);
        lv_obj_set_style_bg_opa(frame,LV_OPA_COVER,0);
        lv_obj_clear_flag(frame,LV_OBJ_FLAG_SCROLLABLE);
        const auto *icon=s.avatar[0]?iconFor(s.id):nullptr;
        if (icon) {
            auto *img=lv_img_create(frame);
            lv_img_set_src(img,icon);
            lv_obj_center(img);
        } else {
            auto *initial=lv_label_create(frame);
            size_t n=1;
            while (n<sizeof(s.name) && (uint8_t(s.name[n])&0xc0)==0x80) ++n;
            lv_label_set_text(initial,String(s.name).substring(0,n).c_str());
            lv_obj_set_style_text_font(initial,&lv_font_montserrat_36,0);
            lv_obj_set_style_text_color(initial,lv_color_hex(gold),0);
            lv_obj_center(initial);
        }
    }
    circle(354, 180, 0xa38759, 1);
    String suffix;
    if (picking) suffix+="  •  "+String(candidate+1)+"/"+String(library.count);
    suffix+="  •  ";
    String power=powerSummary();
    if (power=="USB") suffix+=LV_SYMBOL_USB " ";
    else if (power.startsWith("~")) suffix+=LV_SYMBOL_BATTERY_2 " ";
    suffix+=power;
    arcText(connectionArc(wasOnline?networkName:String(ui::tr("Offline")),suffix),ArcLabel::Status,paper);
    if (!picking) {
        String stats=ui::tr("Level ")+String(s.level)+ui::tr("  •  Proficiency +")+String(dnd::proficiencyBonus(s.level));
        arcText(stats,ArcLabel::Level,gold);
    }
    arcText(s.name,ArcLabel::Name,paper);
    arcText(s.classes,ArcLabel::Classes,gold);
    if (picking) {
        arcText(String(LV_SYMBOL_SETTINGS "  ")+ui::tr("Phone setup"),ArcLabel::Action,teal);
    } else {
        play::sheetLandmark(false,sheetPair);play::sheetLandmark(true,sheetPair);
        auto *hint=textAt((String(LV_SYMBOL_LEFT "  ")+ui::tr("Dice")+"  ")+String(sheetPair+1)+"/5  " LV_SYMBOL_RIGHT,207,104,&lv_font_montserrat_14,paper);
        lv_obj_set_style_bg_color(hint,lv_color_hex(ink),0);lv_obj_set_style_bg_opa(hint,LV_OPA_80,0);lv_obj_set_style_radius(hint,12,0);
        arcText(fetching==s.id?ui::tr("Refreshing..."):String(LV_SYMBOL_REFRESH "  ")+ui::tr("Refresh sheet"),ArcLabel::Action,paper);
        const auto hp=session::health(s.id);
        if(hp.maximum && (!hp.current || sheetPair==3)){
            String health=String(hp.current)+" / "+hp.maximum+" HP";
            if(!hp.current){
                const char *condition=hp.status==vitals::Status::Dying?"Dying":hp.status==vitals::Status::Stable?"Stable":"Dead";
                health+="  •  ";health+=ui::tr(condition);
            }
            arcText(health,ArcLabel::Cache,hp.current?teal:gold);
        }else arcText(syncLabel(s),ArcLabel::Cache,teal);
    }
}
static void pick() {
    if (setupActive) setupClose();
    page = Page::Picker;
    candidate = library.active;
    redraw = true;
}
static void browse(int n) {
    int count = library.count;
    if (!count) return;
    candidate = ((candidate + n) % count + count) % count;
    redraw = true;
}
static void openSheet(int index) {
    if (index<0 || index>=library.count) return;
    if (setupActive) setupClose();
    bool saved = library.select(index);
    sheetPair=0;
    page = Page::Sheet;
    if (reviewTicket && library.records[index].id==reviewOld) page=Page::Review;
    candidate = index;
    if (!saved) { noticeId = library.records[index].id; notice = "Selection not saved"; }
    pulse();
    redraw = true;
    Serial.printf("OPEN %u %s\n", library.records[index].id, library.records[index].name);
}
static const char *beginFetch(uint32_t id, uint32_t oldId,bool forceBody=false) {
    if (ota::busy()) return "Firmware update in progress";
    if (fetching || reviewTicket) return "Finish the current refresh or review first.";
    int i=library.find(id);
    if (!oldId && i<0 && library.count==Library::capacity) return "Character library full";
    if (!WiFi.isConnected()) return "Wi-Fi offline";
    auto now=time(nullptr);
    bool haveRolls=!forceBody && i>=0 && library.records[i].rolls.version && library.records[i].bookVersion==casting::version && library.records[i].bookHash;
    if (!oldId && haveRolls && ((id==checkedId && millis()-checkedAt<30000) ||
        (i>=0 && now>=1700000000 && now>=library.records[i].synced && now-library.records[i].synced<30)))
        return "Checked just now";
    if (!startRefresh(id, haveRolls ? library.records[i].tag : SourceTag{})) return "Could not start refresh";
    fetching = id;
    repairBook=forceBody;
    replacing = oldId;
    discardFetch = false;
    redraw = true;
    return nullptr;
}
const char *importLink(uint32_t id) { return beginFetch(id,0); }
const char *editLink(uint32_t oldId,uint32_t id) {
    if (!oldId || !id || library.find(oldId)<0) return "Choose a saved character and enter a valid link.";
    if (id!=oldId && library.find(id)>=0) return "That character already has an entry.";
    return beginFetch(id,oldId);
}
const char *removeLink(uint32_t id) {
    uint32_t browsing=library.records[candidate].id;
    if (!id || !library.remove(id)) return "Could not remove that linked character.";
    candidate=library.find(browsing);
    if (candidate<0) candidate=library.active;
    if (fetching && (fetching==id || replacing==id)) discardFetch=true;
    if (reviewTicket && (reviewOld==id || proposed.id==id)) reviewTicket=0;
    if (checkedId==id) checkedId=0;
    for (int i=0;i<jobCount;i++) if (jobs[i].id==id) {
        jobs[i].finished=true; jobs[i].message="Removed";
    }
    if (page==Page::Review && !reviewTicket) page=Page::Picker;
    if (!library.count && page!=Page::Setup) page=Page::Picker;
    redraw=true;
    return nullptr;
}
static bool saveAcceptedSheet(const Sheet &sheet,uint32_t replacing){
    int old=library.find(sheet.id);
    bool changed=old<0 || library.records[old].bookHash!=sheet.bookHash;
    if(!library.save(sheet,replacing))return false;
    if(changed && sheet.bookHash){
        RollJsonMemory memory;
        JsonDocument book(&memory);
        if(readMoveBook(sheet.id,sheet.bookBank,sheet.bookHash,book))reconcilePlayState(sheet.id,book);
    }
    return true;
}
const char *decideReview(uint32_t ticket,bool apply) {
    if (!ticket || ticket!=reviewTicket) return "Review expired. Reload the current update.";
    if (apply && !saveAcceptedSheet(proposed,reviewOld)) return "Could not save; the current sheet is still available.";
    noticeId=apply ? proposed.id : reviewOld;
    notice=apply ? "Update applied" : "Kept current sheet";
    for (int i=0;i<jobCount;i++) if (jobs[i].id==proposed.id) {
        jobs[i].finished=true; jobs[i].message=notice;
    }
    reviewTicket=0;
    if (page==Page::Review || (apply && ((page==Page::Pack && packui::owner()==reviewOld) || (page==Page::Play && play::owner()==reviewOld) || (page==Page::Book && bookui::owner()==reviewOld)))) page=Page::Sheet;
    if (apply) pulse();
    redraw=true;
    return nullptr;
}
const char *queueLinks(const String &input) {
    if (!input.length() || input.length() > 3000) return "Paste up to 12 links, one per line.";
    uint32_t batch[Library::capacity];
    int count = 0;
    for (int at = 0; at < int(input.length());) {
        int end = input.indexOf('\n', at);
        if (end < 0) end = input.length();
        String line = input.substring(at, end);
        line.trim();
        at = end + 1;
        if (!line.length()) continue;
        uint32_t id = characterLinkId(line);
        if (!id) return "Check the links. Use a D&D Beyond character URL on each line.";
        bool duplicate = false;
        for (int i = 0; i < count; i++) if (batch[i] == id) duplicate = true;
        for (int i = 0; i < jobCount; i++) if (!jobs[i].finished && jobs[i].id == id) duplicate = true;
        if (duplicate) continue;
        if (count == Library::capacity) return "Too many links in one request.";
        batch[count++] = id;
    }
    int pending = 0;
    for (int i = 0; i < jobCount; i++) if (!jobs[i].finished) pending++;
    if (pending + count > Library::capacity) return "Wait for the current imports to finish.";
    if (!count) return nullptr;
    int keep = 0;
    for (int i = 0; i < jobCount; i++) if (!jobs[i].finished) jobs[keep++] = jobs[i];
    jobCount = keep;
    for (int i = 0; i < count; i++) jobs[jobCount++] = {batch[i], false, "Queued"};
    return nullptr;
}
static void runQueue() {
    if (ota::busy() || fetching || reviewTicket || !WiFi.isConnected()) return;
    for (int i = 0; i < jobCount; ++i) {
        auto &job = jobs[i];
        if (job.finished) continue;
        const char *error = importLink(job.id);
        job.finished = error != nullptr;
        job.message = error ? error : "Fetching from D&D Beyond";
        break;
    }
}
String libraryStatus() {
    JsonDocument doc;
    networkStatus(doc);
    String net=doc["networkMessage"]|"";
    if(net.startsWith("Connecting to "))doc["networkMessage"]=String(ui::tr("Connecting to"))+" "+net.substring(14);
    else doc["networkMessage"]=String(ui::tr(net.c_str()));
    doc["language"]=ui::code();
    doc["pickerArt"]=library.pickerArt();
    if (reviewTicket) {
        auto review=doc["review"].to<JsonObject>();
        review["ticket"]=reviewTicket;
        const auto &old=library.records[library.find(reviewOld)];
        int row=changedRoll(old,proposed);
        review["before"]=String(old.name)+" / "+reviewDetail(old,row);
        review["after"]=String(proposed.name)+" / "+reviewDetail(proposed,row);
        review["revision"]=proposed.tag.revision;
    }
    auto entries = doc["library"].to<JsonArray>();
    for (int i = 0; i < library.count; i++) {
        auto row = entries.add<JsonArray>();
        row.add(library.records[i].name);
        row.add(library.records[i].classes);
        row.add(library.records[i].id);
        row.add(bool(library.records[i].avatar[0]));
        uint32_t source=iconSource(library.records[i].avatar);
        row.add(hasBackground(library.records[i].id));
        row.add(iconCurrent(library.records[i].id,source));
        row.add(source);
    }
    auto pending = doc["jobs"].to<JsonArray>();
    for (int i = 0; i < jobCount; i++) {
        auto row = pending.add<JsonArray>();
        row.add(String(jobs[i].id));
        row.add(ui::tr(jobs[i].message.c_str()));
    }
    String response;
    serializeJson(doc, response);
    return response;
}
static void syncCurrent() {
    uint32_t id = library.records[library.active].id;
    if (!id) { pick(); return; }
    if (reviewTicket && reviewOld==id) { page=Page::Review; redraw=true; return; }
    const char *error = importLink(id);
    if (error) { noticeId = id; notice = error; redraw = true; }
}
static void gesture(int x, int y, int dx, int dy) {
    if (page == Page::Boot) {page=library.hasSelection?Page::Sheet:Page::Picker;redraw=true;return;}
    if (page == Page::Firmware) {
        const auto stage = ota::view().stage;
        if (stage == ota::Stage::Installing || stage == ota::Stage::Restarting) return;
        const bool tap = abs(dx) <= 18 && abs(dy) <= 18;
        if ((dx >= 42 && dx > abs(dy)) || (tap && dial::inReturn(x,y))) {
            ota::cancelPreview();
            page = Page::Device;
        } else if (tap && stage != ota::Stage::Available && stage != ota::Stage::RestorePreview &&
                   dial::Capsule{180,190,218,44}.contains(x,y)) {
            ota::previewRestore();
        } else if (tap && dial::Capsule{180,249,192,46}.contains(x,y)) {
            const bool sheetBusy = fetching || setupActive || reviewTicket;
            if (stage == ota::Stage::RestorePreview) ota::restore(sheetBusy);
            else ota::start(stage == ota::Stage::Available, sheetBusy);
        }
        redraw = true;
        return;
    }
    if(page==Page::Health || page==Page::Pack){
        auto event=(page==Page::Pack?packui::touch:healthui::touch)(library.records[library.active],x,y,dx,dy);
        if(event==bookui::Event::Refresh)syncCurrent();
        if(event==bookui::Event::Back)page=Page::Sheet;
        if(event!=bookui::Event::None)redraw=true;return;
    }
    if(page==Page::Resources){
        auto event=resources::touch(library.records[library.active],x,y,dx,dy);
        if(event==bookui::Event::Back)page=Page::Sheet;
        if(event==bookui::Event::Refresh)syncCurrent();
        if(event!=bookui::Event::None)redraw=true;return;
    }
    if(page==Page::Book){
        auto event=bookui::touch(library.records[library.active],x,y,dx,dy);
        if(event==bookui::Event::Back)page=Page::Sheet;
        if(event==bookui::Event::Roll)page=Page::Play;
        if(event==bookui::Event::Health){healthui::open(library.records[library.active],false);page=Page::Health;}
        if(event==bookui::Event::Refresh){
            if(reviewTicket)syncCurrent();
            else{const char *error=beginFetch(bookui::owner(),0,true);if(error){noticeId=bookui::owner();notice=error;}}
        }
        if(event!=bookui::Event::None)redraw=true;
        return;
    }
    if (page==Page::Play){
        auto event=play::touch(library.records[library.active],x,y,dx,dy);
        if(event==play::Event::Back)page=play::fromMove()?Page::Book:Page::Sheet;
        if(event==play::Event::Refresh)syncCurrent();
        if(event!=play::Event::None)redraw=true;
        return;
    }
    if (abs(dx) >= 42 && abs(dx) > abs(dy)) {
        if (page == Page::Picker) browse(dx < 0 ? 1 : -1);
        else if(page==Page::Sheet){shiftPair(dx<0?1:-1);redraw=true;}
        else if(dx>0 && page==Page::Language){page=Page::Device;redraw=true;}
        else if (dx>0 && page==Page::Device) {page=statusReturn;redraw=true;}
        else if (dx > 0) pick();
        return;
    }
    if(page==Page::Sheet && dy<=-42 && -dy>=abs(dx)){healthui::open(library.records[library.active],false);page=Page::Health;redraw=true;return;}
    if(page==Page::Sheet && dy>=42 && dy>=abs(dx)){pick();return;}
    if (abs(dx) > 18 || abs(dy) > 18) return;
    if(page==Page::Language){
        if(dial::inReturn(x,y)){page=Page::Device;redraw=true;return;}
        const char *codes[]={"en","it","es"};
        for(int i=0;i<3;i++)if(dial::Capsule{180,110+57*i,190,46}.contains(x,y)){
            if(setDisplayLanguage(codes[i]))pulse();
            break;
        }
        return;
    }
    if (page==Page::Device) { if(dial::Capsule{112,253,120,44}.contains(x,y)){page=Page::Language;redraw=true;}
        else if(dial::Capsule{248,253,120,44}.contains(x,y)){page=Page::Firmware;redraw=true;}
        else if (dial::inReturn(x,y)) {page=statusReturn;redraw=true;} }
    else if ((page==Page::Picker || page==Page::Sheet) && dial::inRim(x,y,Rim::Top)) {statusReturn=page;page=Page::Device;redraw=true;}
    else if (page == Page::Review) {
        if (dial::Capsule{180,259,174,44}.contains(x,y)) decideReview(reviewTicket,true);
        else if (dial::inReturn(x,y)) decideReview(reviewTicket,false);
    } else if (page == Page::Setup) {
        if (dial::inReturn(x,y)) pick();
        else if (x>=79 && x<281 && y>=61 && y<263) { showPageCode = !showPageCode; redraw = true; }
    } else if (page == Page::Picker) {
        if (dial::inRim(x,y,Rim::Bottom)) {
            if (setupOpen()) { phoneJoined = showPageCode = false; page = Page::Setup; redraw = true; }
        } else if (library.count && dial::inHub(x,y)) openSheet(candidate);
    } else if (page==Page::Sheet && dial::inRim(x,y,Rim::Bottom)) syncCurrent();
    else if(page==Page::Sheet && (play::hitsLandmark(x,y,false) || play::hitsLandmark(x,y,true))){
        auto &sheet=library.records[library.active];bool right=play::hitsLandmark(x,y,true);
        if(sheetPair==4){packui::open(sheet,right);page=Page::Pack;}
        else if(sheetPair==3){healthui::open(sheet,right);page=Page::Health;}
        else if(sheetPair==2){resources::open(sheet.id,right);page=Page::Resources;}
        else if(sheetPair==1){bookui::open(sheet,right);page=Page::Book;}
        else {play::open(sheet.id,right);page=Page::Play;}
        redraw=true;
        if((sheetPair?!casting::playable(sheet.bookVersion):!sheet.rolls.version) && !fetching && WiFi.isConnected())syncCurrent();
    } else if(page==Page::Sheet && y>=195 && y<=239 && ((x>=115 && x<158) || (x>202 && x<=245))){
        shiftPair(x<180?-1:1);redraw=true;
    } else if (page==Page::Sheet && dial::inHub(x,y)){
        play::openTray(library.records[library.active].id);page=Page::Play;redraw=true;
    }
}
static void turnWheel(int turns) {
    if(turns && page==Page::Play && play::wheel(library.records[library.active],turns,millis()))redraw=true;
    if (turns && page == Page::Picker) browse(turns);
    if(turns && page==Page::Sheet){shiftPair(turns);redraw=true;}
    if(turns && page==Page::Pack && packui::turn(library.records[library.active],turns))redraw=true;
    if(turns && page==Page::Health && healthui::turn(library.records[library.active],turns))redraw=true;
    if(turns && page==Page::Resources && resources::turn(turns))redraw=true;
    if(turns && page==Page::Book && bookui::turn(turns))redraw=true;
    if (turns) Serial.printf("WHEEL %+d\n", turns);
}
// A timed USB contact runs through the same touch sampler as the glass.
static uint32_t testContactStart,testContactDuration;
static uint16_t testX,testY;
static void pollInput() {
    turnWheel(wheelMovement());
    static int startX, startY, endX, endY;
    static bool down;
    static play::DieHold hold;
    static bool leftDie;
    static uint32_t lastSeen;
    uint16_t x, y;
    bool touching=finger(x,y);
    uint32_t now=millis();
    if(testContactDuration && now-testContactStart>=testContactDuration)testContactDuration=0;
    if(!touching && testContactDuration){x=testX;y=testY;touching=true;}
    if (touching) {
        int die=page==Page::Play?play::trayDieAt(x,y):-1;
        if (!down) {
            startX=x;startY=y;down=true;leftDie=false;
            hold.begin(die,now);
        }
        endX=x;endY=y;lastSeen=now;
        if(hold.die>=0 && die!=hold.die)leftDie=true;
        if(hold.due(die,now) && play::removeDie(die))redraw=true;
    } else if (down && now-lastSeen > 45) {
        down = false;
        Serial.printf("TOUCH %d,%d -> %d,%d\n", startX, startY, endX, endY);
        if(!hold.consumed && (!leftDie || abs(endX-startX)>=42 || abs(endY-startY)>=42))
            gesture(startX,startY,endX-startX,endY-startY);
    }
}
static void status() {
    boardStatus();
    ota::describe();
    Serial.printf("STATE page=%d candidate=%s active=%s count=%d wifi=%d refreshing=%u pickerArt=%s language=%s\n",
                  int(page), library.records[candidate].name, library.records[library.active].name,
                  library.count, WiFi.isConnected(), fetching,library.pickerArt(),ui::code());
    if(library.count)Serial.printf("CACHE schema=%u notice=%s\n",library.records[library.active].bookVersion,notice.c_str());
    JsonDocument net;
    networkStatus(net);
    Serial.printf("NETWORKS saved=%u nearby=%u scanning=%d phoneJoined=%d review=%d\n",
                  unsigned(net["saved"].size()),unsigned(net["nearby"].size()),net["scanning"].as<bool>(),
                  setupActive && WiFi.softAPgetStationNum()>0,reviewTicket!=0);
}
extern "C" void display_capture(lv_color_t *);
static void command(const String &cmd) {
    if (cmd == "STATUS") {status();if(page==Page::Play)play::describe(library.records[library.active]);if(page==Page::Book)bookui::describe();if(page==Page::Resources)resources::describe(library.records[library.active]);}
    else if(cmd.startsWith("GESTURE ")){
        int x,y,dx,dy;
        if(sscanf(cmd.c_str(),"GESTURE %d %d %d %d",&x,&y,&dx,&dy)==4 && x>=0 && x<360 && y>=0 && y<360)gesture(x,y,dx,dy);
    } else if(cmd.startsWith("TURN "))turnWheel(cmd.substring(5).toInt());
    else if(cmd.startsWith("HOLD ") && page==Page::Play){
        int x,y,ms=600;
        if(sscanf(cmd.c_str(),"HOLD %d %d %d",&x,&y,&ms)>=2 && play::trayDieAt(x,y)>=0 && ms>0 && ms<=10000){
            testX=x;testY=y;testContactStart=millis();testContactDuration=ms;
        }
    }
    else if (cmd.startsWith("PICKERART ")) Serial.println(setPickerArt(cmd.substring(10))?"Picker appearance saved":"Invalid picker appearance");
    else if (cmd == "SCAN") scanNetworks();
    else if (cmd.startsWith("ART ")) {
        unsigned id=0,length=0,source=0;
        char kind[12]{};
        if (sscanf(cmd.c_str(),"ART %u %11s %u %u",&id,kind,&length,&source)!=4) {
            Serial.println("ART invalid");return;
        }
        auto *sheet=getSheet(id);
        bool avatar=strcmp(kind,"icon")==0;
        if (!sheet || (!avatar && strcmp(kind,"background")!=0) ||
            !length || length>(avatar?iconLimit:backgroundLimit) ||
            (avatar && (!source || iconSource(sheet->avatar)!=source))) {
            Serial.println("ART invalid target or source");return;
        }
        auto *bytes=static_cast<uint8_t *>(ps_malloc(length));
        if (!bytes) {Serial.println("ART no memory");return;}
        Serial.println("ART ready");
        size_t used=0;
        uint32_t start=millis();
        while (used<length && millis()-start<8000) {
            int available=Serial.available();
            if (available>0) used+=Serial.read(bytes+used,std::min(size_t(available),size_t(length)-used));
            else delay(1);
        }
        bool ok=used==length && saveArtwork(id,avatar?ArtLayer::Icon:ArtLayer::Background,bytes,used,source);
        free(bytes);
        Serial.println(ok?"ART saved":"ART failed");
        if (ok) artworkChanged();
    }
    else if(cmd=="BOOTPREVIEW"){page=Page::Boot;bootStart=millis();redraw=true;}
    else if (cmd == "CAPTURE") {
        if(redraw)render();
        constexpr size_t bytes = 360*360*sizeof(lv_color_t);
        auto *pixels = static_cast<lv_color_t *>(ps_malloc(bytes));
        if (!pixels) { Serial.println("CAPTURE no memory"); return; }
        display_capture(pixels);
        Serial.println("RGB565 360 360");
        auto *src = reinterpret_cast<uint8_t *>(pixels);
        size_t sent = 0;
        uint32_t started = millis();
        while (sent < bytes && millis()-started < 8000) {
            sent += Serial.write(src+sent, std::min(size_t(1024), bytes-sent));
            delay(1);
        }
        free(pixels);
    }
    else if (cmd == "LIST") {
        for (int i=0; i<library.count; ++i) {
            const auto &s = library.records[i];
            Serial.printf("SHEET %u %s | %s | %u icon=%d background=%d\n", s.id, s.name, s.classes, s.synced,
                          iconCurrent(s.id,iconSource(s.avatar)),hasBackground(s.id));
        }
    } else if (cmd == "PICK") pick();
    else if (cmd.startsWith("ADD ")) {
        const char *error=queueLinks(cmd.substring(4));
        Serial.println(error?error:"Link queued");
    }
    else if (cmd == "HAPTIC") Serial.printf("HAPTIC acknowledged=%d\n", pulse());
    else if (cmd.startsWith("OPEN ")) {
        String value = cmd.substring(5);
        int index = library.find(strtoul(value.c_str(), nullptr, 10));
        if (index >= 0) openSheet(index);
    } else if (cmd == "REFRESH") {
        syncCurrent();
        if (!fetching) Serial.printf("REFRESH %s\n", notice.c_str());
    }
}
void appBegin() {
    ui::loadLanguage();
    library.begin();
    candidate = library.active;
    wifiBegin();
    bootStart = millis();
    render();
    status();
}
void appLoop() {
    static uint32_t previous = millis();
    uint32_t now = millis();
    static uint32_t powerSample;
    bool sampleDue=now-powerSample>=5000;
    if (sampleDue) {
        String old=powerSummary();
        samplePower();
        powerSample=now;
        if (powerSummary()!=old) redraw=true;
        if (railLabel) lv_label_set_text_fmt(railLabel,"%s %s V",ui::tr("Supply rail"),String(railMillivolts()/1000.0,2).c_str());
    }
    lv_tick_inc(now - previous);
    previous = now;
    if (page == Page::Boot) {
        uint32_t elapsed = now - bootStart;
        if (elapsed >= boot::duration) { page = library.hasSelection ? Page::Sheet : Page::Picker; redraw = true; }
        else boot::tick(elapsed);
    }
    pollInput();
    setupPoll();
    if (page == Page::Setup && !setupActive) pick();
    if (page == Page::Setup && (WiFi.softAPgetStationNum() > 0) != phoneJoined) {
        phoneJoined = WiFi.softAPgetStationNum() > 0;
        showPageCode = phoneJoined;
        redraw = true;
        Serial.printf("PAIR phoneJoined=%d\n", phoneJoined);
    }
    bool online=WiFi.isConnected();
    if (online!=wasOnline || (online && sampleDue)) {
        String name=online?displayNetworkName(WiFi.SSID()):String();
        if (online && name.isEmpty()) name="Connected";
        if (online!=wasOnline || name!=networkName) redraw=true;
        if (online!=wasOnline) Serial.printf("WIFI connected=%d\n",online);
        wasOnline=online;
        networkName=name;
    }
    RefreshResult result{};
    if (takeRefresh(result)) {
        fetching = 0;
        int old = library.find(replacing ? replacing : result.sheet.id);
        if(!discardFetch && !result.error[0] && !result.notModified){
            bool sameId=old>=0 && library.records[old].id==result.sheet.id;
            bool bank=sameId && library.records[old].bookBank;
            if(!sameId || result.sheet.bookHash!=library.records[old].bookHash || repairBook){
                bank=!bank;
                if(!writeMoveBook(result.sheet.id,bank,result.book,result.bookSize))strlcpy(result.error,"Could not store spellbook",sizeof(result.error));
            }
            result.sheet.bookBank=bank;
        }
        free(result.book);
        bool saved = false;
        noticeId = replacing ? replacing : result.sheet.id;
        if (discardFetch) notice = "Import cancelled";
        else if (result.error[0]) notice = result.error;
        else if(old>=0 && !result.notModified && library.records[old].rolls.version && !result.sheet.rolls.version)
            notice="Roll data missing; kept cached sheet";
        else {
            checkedId = result.sheet.id;
            checkedAt = millis();
            if (result.notModified && old >= 0) {
                result.sheet = library.records[old];
                result.sheet.synced = time(nullptr);
            }
            bool changed = old >= 0 && sheetChanged(library.records[old],result.sheet);
            if (changed) {
                comparePack(library.records[old],result.sheet);
                proposed = result.sheet;
                reviewOld = library.records[old].id;
                reviewTicket = esp_random() | 1u;
                notice = "Update ready to review";
                if ((page==Page::Sheet || page==Page::Play || page==Page::Book || page==Page::Resources || page==Page::Health || page==Page::Pack) && library.active==old) page=Page::Review;
            } else {
                saved = saveAcceptedSheet(result.sheet,replacing);
                notice = saved ? (old<0 ? "Character added" : "Up to date") : "Could not save sheet";
            }
        }
        for (int i = 0; i < jobCount; i++) {
            if (!jobs[i].finished && jobs[i].id == result.sheet.id) {
                jobs[i].finished = true;
                jobs[i].message = saved ? String(ui::tr("Saved"))+" "+result.sheet.name : notice;
            }
        }
        Serial.printf("REFRESH id=%u ok=%d %s\n", result.sheet.id,saved,notice.c_str());
        if (saved && library.records[library.active].id == noticeId) pulse();
        replacing=0;
        redraw = true;
    }
    if(page==Page::Health && healthui::tick(millis()))redraw=true;
    if(page==Page::Play && play::tick(millis()))redraw=true;
    if (ota::poll() && page == Page::Firmware) redraw = true;
    runQueue();
    static String line;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') { line.trim(); command(line); line = ""; }
        else if (line.length() < 96) line += c;
    }
    if (redraw) render();
    lv_timer_handler();
    static bool healthy;
    if (!healthy && page != Page::Boot && display_transfers()) {
        ota::confirmHealthyBoot(!ota::needsBootCheck() || library.readable());
        healthy = true;
    }
}
