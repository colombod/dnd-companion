#include "ui_language.h"
#include "health_ui.h"
#include "roll_aid.h"
#include "roll_animation.h"
#include "play.h"
#include "dice_gestures.h"
#include "portraits.h"
#include "board.h"
#include "book_ui.h"
#include "cast_import.h"
#include "session_store.h"
#include "round_controls.h"
#include "rim_text.h"
#include <Preferences.h>
#include <esp_random.h>

namespace play {
namespace {
constexpr uint32_t ivory=0xf5eddf,gold=0xf4d49a,teal=0x99ddd2,night=0x17232d;
struct Point {int x,y;};
constexpr Point choices[]={{108,96},{252,96},{297,178},{253,245},{107,245},{63,178}};
constexpr Point dicePos[]={{55,216},{65,144},{111,94},{180,77},{249,94},{295,144},{305,216}};
constexpr Point modes[]={{82,243},{180,256},{278,243}};
constexpr int sides[]={4,6,8,10,12,20,100};
constexpr int modeMap[]={2,0,1};
const uint8_t dicePixels[][1416]={
#include "dice_tiles.inc"
};
View view=View::Select;
uint32_t character=0,lastDetent=0;
bool isSave=false,subtract=false,rollingPool=false,waitForQuiet=false,directTray=false;
int group=0,selected=0,poolPage=0;
dnd::Mode manual=dnd::Mode::Normal;
dnd::CheckRoll result{};
constexpr dial::Capsule flashButton{180,253,205,46};
rollaid::Picker aidPicker;
int reactionBonus;
String reactionError;
session::Build reactionBuild(const Sheet &sheet){
    if(!casting::playable(sheet.bookVersion))return {};
    return casting::build(bookui::currentBook(sheet));
}
moves::Roll sourceRoll;
String sourceName,sourceLabel;
uint8_t mix[7]{},savedMix[7]{},faces[100]{};
unsigned faceCount=0;
int64_t poolTotal=0;
Preferences recipes;
bool recipesReady;
String mixKey(uint32_t id){return "m"+String(id);}
bool recipesOpen(){if(!recipesReady) recipesReady=recipes.begin("dnd-dice");return recipesReady;}
String sign(int value){return value>=0?"+"+String(value):String(value);}
bool near(int x,int y,Point p,int r){int dx=x-p.x,dy=y-p.y;return dx*dx+dy*dy<=r*r;}
void label(const String &text,int x,int y,int width,const lv_font_t *font,uint32_t color) {
    auto *l=lv_label_create(lv_scr_act());
    String shown=text;shown.replace("’","'");shown.replace("–","-");
    lv_label_set_text(l,shown.c_str());lv_obj_set_width(l,width);
    if(shown.indexOf('\n')<0 && lv_txt_get_width(shown.c_str(),shown.length(),font,0,LV_TEXT_FLAG_NONE)>width){
        font=&lv_font_montserrat_14;lv_label_set_long_mode(l,LV_LABEL_LONG_SCROLL_CIRCULAR);
    }
    lv_obj_set_style_text_font(l,font,0);
    lv_obj_set_style_text_color(l,lv_color_hex(color),0);
    lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_align(l,LV_ALIGN_CENTER,x-180,y-180);
}
lv_obj_t *disc(Point p,int diameter,uint32_t color,lv_opa_t alpha) {
    auto *o=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(o);
    lv_obj_set_size(o,diameter,diameter);lv_obj_set_pos(o,p.x-diameter/2,p.y-diameter/2);
    lv_obj_set_style_radius(o,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,alpha,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
lv_obj_t *dieImage(int i,Point p){
    static lv_img_dsc_t sprites[7]{};
    auto &d=sprites[i];
    if(!d.data){d.header.cf=LV_IMG_CF_INDEXED_4BIT;d.header.w=d.header.h=52;d.data_size=sizeof(dicePixels[i]);d.data=dicePixels[i];}
    auto *img=lv_img_create(lv_scr_act());lv_img_set_src(img,&d);lv_obj_set_pos(img,p.x-26,p.y-26);
    return img;
}
void token(int i,Point p){
    dieImage(i,p);
    label(i==6?"%":String(sides[i]),p.x,p.y,45,&lv_font_montserrat_18,ivory);
}
int entry(int n){return isSave?checks::saves+n:group*6+n;}
int dieAt(int x,int y){for(int i=0;i<7;i++) if(near(x,y,dicePos[i],32)) return i;return -1;}
unsigned diceCount(){unsigned n=0;for(auto v:mix)n+=v;return n;}
void editDie(int i,bool remove){
    if(remove && mix[i]) --mix[i];
    else if(!remove && diceCount()<100) ++mix[i];
}
String poolFormula();
String moveFormula(){
    String s=String(sourceRoll.count)+"d"+sourceRoll.sides;
    return sourceRoll.bonus?s+" "+sign(sourceRoll.bonus):s;
}
void drawRolling(){
    String heading=sourceRoll.valid()?sourceName:rollingPool?String(ui::tr("Rolling dice")):String(ui::tr(checks::name(selected)));
    label(heading,180,91,258,heading.length()>17?&lv_font_montserrat_20:&lv_font_montserrat_28,ivory);
    rollanim::draw();
    label(sourceRoll.valid()?moveFormula():rollingPool?poolFormula():String("d20"),180,244,234,&lv_font_montserrat_18,gold);
}
void arm(View target,uint32_t now){
    reactionBonus=0;reactionError="";aidPicker={};
    view=target;
    waitForQuiet=lastDetent && now-lastDetent<220;
}
int randomFace(int n){
    uint32_t sides=uint32_t(n),threshold=(0u-sides)%sides,value;
    do {value=esp_random();} while(value<threshold);
    return 1+int(value%sides);
}
const char *modeName(dnd::Mode m){return m==dnd::Mode::Advantage?ui::tr("Advantage"):m==dnd::Mode::Disadvantage?ui::tr("Disadvantage"):ui::tr("Normal");}
dnd::Mode effective(const checks::Entry &e){
    return dnd::rollMode((e.flags&checks::Advantage)||manual==dnd::Mode::Advantage,
                         (e.flags&checks::Disadvantage)||manual==dnd::Mode::Disadvantage);
}
String explanation(const checks::Snapshot &s,int i,int level){
    const auto &e=s.rows[i];
    if(e.flags&checks::Override) return ui::tr("Sheet override ")+sign(e.overrideTotal);
    String t=String(ui::tr(checks::shortNames[e.ability]))+" "+sign(dnd::abilityModifier(s.scores[e.ability]));
    if(e.training){
        const char *types[]={"",ui::tr("half"),ui::tr("prof"),ui::tr("expert")};
        t+="  •  "+String(types[e.training])+" "+sign(dnd::trainingBonus(dnd::Training(e.training),dnd::proficiencyBonus(level)));
    }
    if(e.bonus)t+="  •  "+sign(e.bonus);
    return t;
}
String poolFormula(){
    String s;
    for(int i=0;i<7;i++) if(mix[i]){if(s.length())s+=" + ";s+=String(mix[i])+"d"+sides[i];}
    return s;
}
void drawSelector(const Sheet &sheet,const String &status){
    if(!sheet.rolls.version){
        label(ui::tr("Roll data needed"),180,139,280,&lv_font_montserrat_28,ivory);
        label(status,180,185,280,&lv_font_montserrat_14,teal);
        label(ui::tr("Tap to refresh"),180,224,260,&lv_font_montserrat_18,ivory);
    }else{
        for(int j=0;j<6;j++){
            int i=entry(j),mod;auto p=choices[j];
            checks::Prepared roll(sheet.rolls,sheet.level,i);
            String title=ui::tr(i<checks::plain?checks::name(i):checks::shortNames[(i-checks::plain)%6]);
            if(title=="Sleight of Hand")title="Sleight\nof Hand";
            else if(title.length()>12 && title.indexOf(' ')>0){
                int middle=title.length()/2,best=-1;
                for(int k=0;k<int(title.length());k++)if(title[k]==' ' && (best<0 || abs(k-middle)<abs(best-middle)))best=k;
                title.setCharAt(best,'\n');
            }
            disc(p,74,night,LV_OPA_90);
            label(title,p.x,p.y-8,103,&lv_font_montserrat_14,ivory);
            label(roll.modifier(mod)?sign(mod):String("?"),p.x,p.y+20,65,&lv_font_montserrat_18,gold);
        }
        label(isSave?ui::tr("Saves"):group==3?ui::tr("Abilities"):ui::tr("Skills"),180,142,140,&lv_font_montserrat_28,ivory);
        label(isSave?ui::tr("Choose a save"):String(LV_SYMBOL_UP "  ")+String(group+1)+"/4  " LV_SYMBOL_DOWN,180,163,124,&lv_font_montserrat_14,gold);
        token(5,{180,204});label(ui::tr("Dice tray"),180,237,84,&lv_font_montserrat_14,teal);
    }
    returnArc(ui::tr("Back to sheet"));
}
void drawCheck(const Sheet &sheet){
    auto &s=sheet.rolls;const auto &e=s.rows[selected];
    checks::Prepared roll(s,sheet.level,selected);int bonus;
    bool known=roll.modifier(bonus);
    String title=ui::tr(checks::name(selected));
    label(title,180,103,280,title.length()>13?&lv_font_montserrat_28:&lv_font_montserrat_36,ivory);
    String hint=healthui::reminder(session::read(sheet.id,reactionBuild(sheet)).aids,
        isSave?vitals::Roll::Save:selected<checks::plain?vitals::Roll::Skill:vitals::Roll::Ability,selected);
    const char *conditional=e.flags&checks::Concentration?"Advantage on concentration":e.flags&checks::Conditional?"Check situational effects":nullptr;
    if(conditional){if(hint.length())hint+="  •  ";hint+=ui::tr(conditional);}
    if(view==View::Result && aidPicker.receipt.length())hint=aidPicker.receipt+(hint.length()?"  •  "+hint:"");
    if(hint.length())label(hint,180,71,244,&lv_font_montserrat_14,teal);
    if(!known){
        label(ui::tr("Check your sheet"),180,170,280,&lv_font_montserrat_20,gold);
        label(ui::tr("Modifier needs review"),180,204,270,&lv_font_montserrat_14,ivory);
    }else{
        bool settled=view==View::Result;
        label(settled?String(int(result.result.total)):sign(bonus),180,154,180,&lv_font_montserrat_36,gold);
        String line;
        if(settled){
            line="d20 "+String(result.first);
            if(result.mode!=dnd::Mode::Normal)line+=" / "+String(result.second);
            line+="   •   "+String(result.result.kept)+" "+sign(result.bonus);
        }else line=String(modeName(effective(e)))+ui::tr("   •   Tap or spin");
        label(line,180,187,280,&lv_font_montserrat_18,ivory);
        label(explanation(s,selected,sheet.level),180,210,295,&lv_font_montserrat_14,teal);
        auto b=settled?reactionBuild(sheet):session::Build{};
        if(b.geniusMax){
            int left=session::geniusLeft(b,session::read(character,b));
            String caption=reactionError.length()?reactionError:String(left)+" / "+b.geniusMax+" "+ui::tr("uses");
            String title=reactionBonus?String(ui::tr("Flash of Genius"))+" "+sign(reactionBonus):
                         String(ui::tr("Failed? Use reaction"))+" "+sign(b.geniusMax);
            if(!left && !reactionBonus)title=ui::tr("No uses remaining");
            flashButton.draw(title,ivory,left && !reactionBonus?0x405451:night,caption.c_str());
        }else for(int j=0;j<3;j++){
            auto m=dnd::Mode(modeMap[j]);auto p=modes[j];
            disc(p,54,m==manual?0x405451:night,LV_OPA_90);
            label(j==0?ui::tr("Disadv."):j==1?ui::tr("Normal"):ui::tr("Adv."),p.x,p.y,64,&lv_font_montserrat_14,m==manual?gold:ivory);
        }
    }
    returnArc(isSave?ui::tr("Back to saves"):ui::tr("Back to checks"));
}
void drawTray(){
    if(view==View::PoolResult){
        label(String(int(poolTotal)),180,102,180,&lv_font_montserrat_36,gold);
        unsigned start=poolPage*6;
        for(unsigned k=start;k<faceCount && k<start+6;k++){
            unsigned end=0;int d=0;for(;d<7;d++){end+=mix[d];if(k<end)break;}
            auto n=k-start;
            label("d"+String(sides[d])+"   "+String(faces[k]),n%2?235:125,151+int(n/2)*36,98,&lv_font_montserrat_20,ivory);
        }
        label(faceCount>6?String(poolPage+1)+" / "+String((faceCount+5)/6)+ui::tr("   •   Swipe for dice"):ui::tr("Tap total to edit"),180,262,244,&lv_font_montserrat_14,teal);
    }else{
        for(int i=0;i<7;i++){
            auto p=dicePos[i];token(i,p);
            if(mix[i]){disc({p.x+17,p.y+17},27,0x393326,LV_OPA_COVER);label(String(mix[i]),p.x+17,p.y+17,27,&lv_font_montserrat_14,gold);}
        }
        String formula=poolFormula();
        label(!formula.length()?ui::tr("Choose dice"):formula,180,167,166,formula.length()>13?&lv_font_montserrat_14:&lv_font_montserrat_20,ivory);
        label(ui::tr("Tap or spin to roll"),180,207,150,&lv_font_montserrat_14,gold);
        for(int j=0;j<2;j++){
            auto *button=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(button);
            int cx=j?228:132;
            lv_obj_set_size(button,86,38);lv_obj_set_pos(button,cx-43,227);
            lv_obj_set_style_radius(button,19,0);
            lv_obj_set_style_bg_color(button,lv_color_hex(j==int(subtract)?0x405451:night),0);
            lv_obj_set_style_bg_opa(button,LV_OPA_COVER,0);
            label(j?ui::tr("- Remove"):ui::tr("+ Add"),cx,246,84,&lv_font_montserrat_14,j==int(subtract)?gold:ivory);
        }
        label(ui::tr("Hold a die to remove"),180,276,236,&lv_font_montserrat_14,ivory);
    }
    returnArc(view==View::PoolResult?ui::tr("Back to tray"):directTray?ui::tr("Back to sheet"):ui::tr("Back to choices"));
}
void drawSource(const Sheet &sheet){
    if(sourceRoll.attack){
        String hint=healthui::reminder(session::read(sheet.id,reactionBuild(sheet)).aids,vitals::Roll::Attack);
        if(view==View::Result && aidPicker.receipt.length())hint=aidPicker.receipt+(hint.length()?"  •  "+hint:"");
        if(hint.length())label(hint,180,72,242,&lv_font_montserrat_14,teal);
    }
    label(sourceName,180,100,258,sourceName.length()>17?&lv_font_montserrat_20:&lv_font_montserrat_28,ivory);
    bool settled=view==View::Result;
    label(settled?String(int(sourceRoll.attack?result.result.total:poolTotal)):moveFormula(),180,159,221,&lv_font_montserrat_36,gold);
    String detail=ui::tr("Tap or spin to roll");
    if(settled){
        if(sourceRoll.attack){
            detail="d20 "+String(result.first);
            if(result.second)detail+=" / "+String(result.second);
            detail+=ui::tr("  •  kept ")+String(result.result.kept)+" "+sign(result.bonus);
        }else{
            detail="";
            for(unsigned i=0;i<std::min(faceCount,6u);i++){if(i)detail+=" + ";detail+=faces[i];}
            if(faceCount>6)detail+=" + ...";if(sourceRoll.bonus)detail+=" "+sign(sourceRoll.bonus);
        }
    }
    label(detail,180,201,272,&lv_font_montserrat_18,ivory);
    if(sourceRoll.attack){
        for(int j=0;j<3;j++){
            auto p=modes[j];bool on=dnd::Mode(modeMap[j])==manual;
            disc(p,54,on?0x405451:night,LV_OPA_90);
            label(j==0?ui::tr("Disadv."):j==1?ui::tr("Normal"):ui::tr("Adv."),p.x,p.y,64,&lv_font_montserrat_14,on?gold:ivory);
        }
    }else if(settled){
        String hint=ui::tr("Tap total to ready again");
        if(sourceRoll.halfOnSave)hint=ui::tr("Successful save  ")+String(int(poolTotal/2));
        if(sourceRoll.extraOnMax)hint=ui::tr("Extra d8s  ")+String(int(faceCount)-sourceRoll.count);
        if(sourceRoll.leapOnPair){
            bool seen[9]{},pair=false;for(unsigned i=0;i<faceCount;i++)if(faces[i]<=8){pair|=seen[faces[i]];seen[faces[i]]=true;}
            hint=pair?ui::tr("Pair rolled  •  check leap"):ui::tr("No matching dice");
        }
        label(hint,180,246,242,&lv_font_montserrat_14,teal);
    }
    returnArc(ui::tr("Back to move"));
}
}
void open(uint32_t id,bool saves){
    aidPicker={};
    if(character!=id){
        memset(mix,0,sizeof(mix));auto k=mixKey(id);
        if(recipesOpen() && recipes.getBytesLength(k.c_str())==sizeof(mix)) recipes.getBytes(k.c_str(),mix,sizeof(mix));
        if(diceCount()>100)memset(mix,0,sizeof(mix));
        memcpy(savedMix,mix,sizeof(mix));
    }
    character=id;isSave=saves;selected=saves?checks::saves:0;group=0;view=View::Select;subtract=false;directTray=false;sourceRoll={};
}
void openTray(uint32_t id){open(id,false);directTray=true;arm(View::Tray,millis());}
void openMove(uint32_t id,const String &name,const String &label,const moves::Roll &roll){
    open(id,false);sourceName=name;sourceLabel=label;sourceRoll=roll;manual=dnd::Mode::Normal;arm(View::Ready,millis());
}
bool fromMove(){return sourceRoll.valid();}
void forgetMix(uint32_t id){
    if(character==id){character=0;memset(mix,0,sizeof(mix));}
    auto k=mixKey(id);if(recipesOpen() && recipes.isKey(k.c_str()))recipes.remove(k.c_str());
}
uint32_t owner(){return character;}
bool hitsLandmark(int x,int y,bool saves){return near(x,y,{saves?305:55,173},36);}
void sheetLandmark(bool right,int pair){
    Point p{right?305:55,173};disc(p,62,night,LV_OPA_90);
    label(pair==4?(right?LV_SYMBOL_LOOP:LV_SYMBOL_DIRECTORY):pair==3?(right?LV_SYMBOL_BELL:LV_SYMBOL_PLUS):pair==2?(right?LV_SYMBOL_PAUSE:LV_SYMBOL_PLUS):pair==1?(right?LV_SYMBOL_CHARGE:LV_SYMBOL_LIST):(right?LV_SYMBOL_OK:LV_SYMBOL_EYE_OPEN),p.x,p.y-10,40,&lv_font_montserrat_18,gold);
    label(pair==4?(right?ui::tr("Turn"):ui::tr("Pack")):pair==3?(right?ui::tr("Effects"):ui::tr("Health")):pair==2?(right?ui::tr("Rest"):ui::tr("Magic")):pair==1?(right?ui::tr("Actions"):ui::tr("Spells")):(right?ui::tr("Saves"):ui::tr("Checks")),p.x,p.y+11,62,&lv_font_montserrat_14,ivory);
}
void draw(const Sheet &sheet,const String &status){
    bool tray=view==View::Tray || view==View::PoolResult || (view==View::Rolling && rollingPool);
    if(const auto *art=playBackdrop(tray)){
        auto *im=lv_img_create(lv_scr_act());lv_img_set_src(im,art);lv_obj_set_pos(im,0,0);
        if(!tray)lv_obj_set_style_img_opa(im,LV_OPA_70,0);
    }
    if(view!=View::Select){
        const char *kind=fromMove()?sourceLabel.c_str():tray?ui::tr("Dice tray"):isSave?ui::tr("Saving throw"):selected<checks::plain?ui::tr("Skill check"):ui::tr("Ability check");
        arcText(kind,ArcLabel::PlayTitle,gold);
    }
    if(aidPicker.open){aidPicker.draw(session::read(sheet.id,reactionBuild(sheet)).aids);returnArc(ui::tr("Back to roll"));return;}
    if(view==View::Rolling)drawRolling();else if(fromMove())drawSource(sheet);else if(tray)drawTray();else if(view==View::Select)drawSelector(sheet,status);else drawCheck(sheet);
}
Event touch(const Sheet &sheet,int x,int y,int dx,int dy){
    if(view==View::Rolling)return Event::None;
    bool swipe=dx>=42 && abs(dx)>abs(dy);
    if(view==View::Result && (!fromMove() || sourceRoll.attack)){
        const auto kind=fromMove()?vitals::Roll::Attack:isSave?vitals::Roll::Save:selected<checks::plain?vitals::Roll::Skill:vitals::Roll::Ability;
        const bool tap=abs(dx)<=18 && abs(dy)<=18;
        if(aidPicker.open){
            if(swipe || (tap && dial::inReturn(x,y)))aidPicker.open=false;
            else if(tap && dial::Capsule{180,250,174,44}.contains(x,y)){
                int extra=aidPicker.take(character,reactionBuild(sheet),kind,selected);
                result.result.total+=extra;result.bonus+=extra;if(extra)pulse();
            }else if(tap && y>=107 && y<=156)aidPicker.choose(session::read(character,reactionBuild(sheet)).aids,kind,selected,x<180?-1:1);
            return Event::Paint;
        }
        if(tap && dial::Capsule{180,71,230,42}.contains(x,y) && aidPicker.start(session::read(character,reactionBuild(sheet)).aids,kind,selected))return Event::Paint;
    }
    if(!swipe && abs(dy)>=42 && abs(dy)>=abs(dx)){
        int step=dy<0?1:-1;
        if(view==View::Select && !isSave)group=(group+step+4)%4;
        else if((view==View::Ready || view==View::Result) && (!fromMove() || sourceRoll.attack)){manual=dy<0?dnd::Mode::Advantage:dnd::Mode::Disadvantage;arm(View::Ready,millis());}
        else if(view==View::PoolResult)poolPage=std::clamp(poolPage+step,0,int((faceCount-1)/6));
        return Event::Paint;
    }
    if(!swipe && (abs(dx)>18 || abs(dy)>18))return Event::None;
    if(swipe || dial::inReturn(x,y)){
        if(fromMove() || view==View::Select || (view==View::Tray && directTray))return Event::Back;
        if(view==View::PoolResult)arm(View::Tray,millis());
        else view=View::Select;
        return Event::Paint;
    }
    if(view==View::Select){
        if(!sheet.rolls.version)return near(x,y,{180,180},86)?Event::Refresh:Event::None;
        int nearest=-1,dist=42*42+1;
        for(int j=0;j<6;j++){
            int u=x-choices[j].x,v=y-choices[j].y,d=u*u+v*v;
            if(d<dist){nearest=j;dist=d;}
        }
        if(nearest>=0){
            selected=entry(nearest);manual=dnd::Mode::Normal;arm(View::Ready,millis());return Event::Paint;
        }
        if(near(x,y,{180,205},35)){arm(View::Tray,millis());return Event::Paint;}
    }else if(view==View::Ready || view==View::Result){
        if(view==View::Result && !fromMove()){
            auto b=reactionBuild(sheet);
            if(b.geniusMax && y>=215 && y<=282){
                if(!reactionBonus && flashButton.contains(x,y) && session::geniusLeft(b,session::read(character,b))){
                    auto err=session::commit(character,b,{session::Op::FlashOfGenius});
                    if(err)reactionError=ui::tr(err);
                    else{reactionBonus=b.geniusMax;result.bonus+=reactionBonus;result.result.total+=reactionBonus;pulse();}
                }
                return Event::Paint;
            }
        }
        if(!fromMove() || sourceRoll.attack)for(int j=0;j<3;j++)if(near(x,y,modes[j],34)){manual=dnd::Mode(modeMap[j]);arm(View::Ready,millis());return Event::Paint;}
        if(view==View::Result && near(x,y,{180,153},47)){arm(View::Ready,millis());return Event::Paint;}
        if(view==View::Ready && near(x,y,{180,157},46)){
            waitForQuiet=false;return wheel(sheet,1,millis())?Event::Paint:Event::None;
        }
    }else if(view==View::Tray){
        int i=dieAt(x,y);if(i>=0){editDie(i,subtract);return Event::Paint;}
        if(x>=112 && x<=248 && y>=141 && y<=223){
            waitForQuiet=false;return wheel(sheet,1,millis())?Event::Paint:Event::None;
        }
        for(int j=0;j<2;j++){
            int cx=j?228:132;
            int dx=std::max(0,abs(x-cx)-22),dy=y-246;
            if(dx*dx+dy*dy<=22*22){subtract=j!=0;return Event::Paint;}
        }
    }else if(view==View::PoolResult && near(x,y,{180,104},50)){arm(View::Tray,millis());return Event::Paint;}
    return Event::None;
}
int trayDieAt(int x,int y){return view==View::Tray?dieAt(x,y):-1;}
bool removeDie(int i){
    if(view!=View::Tray || i<0 || i>=7 || !mix[i])return false;
    --mix[i];pulse();return true;
}
bool wheel(const Sheet &sheet,int turns,uint32_t now){
    if(!turns)return false;
    uint32_t idle=now-lastDetent;lastDetent=now;
    if(aidPicker.open){auto kind=fromMove()?vitals::Roll::Attack:isSave?vitals::Roll::Save:selected<checks::plain?vitals::Roll::Skill:vitals::Roll::Ability;
        return aidPicker.choose(session::read(character,reactionBuild(sheet)).aids,kind,selected,turns>0?1:-1);}
    if(view==View::Rolling){rollanim::sample(turns,now);return false;}
    if(view==View::Select && !isSave && sheet.rolls.version){group=((group+turns)%4+4)%4;return true;}
    if(waitForQuiet && idle<220)return false;
    waitForQuiet=false;
    if(view!=View::Ready && view!=View::Tray)return false;
    rollingPool=view==View::Tray;
    if(fromMove()){
        if(sourceRoll.attack){
            result={};result.mode=manual;result.bonus=sourceRoll.bonus;
            result.first=randomFace(20);if(manual!=dnd::Mode::Normal)result.second=randomFace(20);
            if(!dnd::resolveD20(manual,result.first,result.second,result.bonus,result.result))return false;
        }else{
            if(!moves::resolveEffect(sourceRoll,randomFace,faces,faceCount,poolTotal))return false;
        }
    }else if(rollingPool){
        if(!diceCount())return false;
        dnd::DieGroup groups[7];size_t n=0;
        for(int i=0;i<7;i++)if(mix[i])groups[n++]={mix[i],uint16_t(sides[i])};
        faceCount=0;
        if(!dnd::rollPool(groups,n,0,[](int sides){int die=randomFace(sides);faces[faceCount++]=die;return die;},poolTotal))return false;
        if(memcmp(savedMix,mix,sizeof(mix)) && recipesOpen() &&
           recipes.putBytes(mixKey(character).c_str(),mix,sizeof(mix))==sizeof(mix))memcpy(savedMix,mix,sizeof(mix));
        poolPage=0;
    }else{
        checks::Prepared p(sheet.rolls,sheet.level,selected);auto mode=effective(sheet.rolls.rows[selected]);
        if(!dnd::rollCheck(p.profile,0,mode==dnd::Mode::Advantage,mode==dnd::Mode::Disadvantage,randomFace,result))return false;
    }
    rollanim::begin(turns,now);view=View::Rolling;return true;
}
bool tick(uint32_t now){
    if(view!=View::Rolling)return false;
    if(!rollanim::finished(now))return false;
    view=rollingPool?View::PoolResult:View::Result;pulse();return true;
}
void describe(const Sheet &s){
    int mod=0;checks::Prepared p(s.rolls,s.level,selected);
    Serial.printf("PLAY id=%u snapshot=%u view=%d group=%d selected=%s known=%d",s.id,s.rolls.version,int(view),group,checks::name(selected),p.modifier(mod));
    Serial.printf(" mod=%d dice=%u faces=%u total=%d first=%d second=%d kept=%d\n",mod,diceCount(),faceCount,int(view==View::PoolResult?poolTotal:result.result.total),result.first,result.second,result.result.kept);
    Serial.printf("SPIN strength=%d duration=%u direction=%d\n",rollanim::strength(),rollanim::duration(),rollanim::direction());
    if(fromMove())Serial.printf("MOVE name=%s count=%d sides=%d bonus=%d attack=%d total=%d\n",sourceName.c_str(),sourceRoll.count,sourceRoll.sides,sourceRoll.bonus,sourceRoll.attack,int(sourceRoll.attack?result.result.total:poolTotal));
    if(view==View::PoolResult){
        Serial.print("FACES");for(unsigned i=0;i<faceCount;i++)Serial.printf(" %u",faces[i]);Serial.println();
    }
}
}
