#include "pack_ui.h"
#include "board.h"
#include "portraits.h"
#include "rim_text.h"
#include "rim.h"
#include "roll_aid.h"

namespace packui {
namespace {
enum class Face { Bag, Item, Purse, Adjust, Reset, Use, Turn, Next, Receipt };
Face face,returnTo;
uint32_t character,itemId;
int page,coin,delta,stepSize;
bool paying,spending,modern,localItem;
pack::Timing timing;
pack::Target target;
String notice;
session::Op receiptOp;
constexpr uint32_t dark=0x19292d,ivory=0xf6eddf,gold=0xf2cf88,mint=0xa8e3da;
constexpr dial::Capsule first{180,113,201,44},second{180,166,243,44},third{180,219,213,44};
constexpr dial::Capsule left{110,251,117,44},right{250,251,117,44},mainButton{180,250,198,44};
constexpr dial::Capsule selector{180,129,256,44};
constexpr const char *targets[]={"Drink","Give to ally","Ally gives me mine"};
constexpr const char *units[]={"GP","SP","CP","EP","PP"};
int wrap(int val,int n){return (val%n+n)%n;}
void line(const String &v,int y,int width=270){dial::text(v,y,width,&lv_font_montserrat_14,mint,true);}
void title(const String &v){dial::text(v,82,235,&lv_font_montserrat_20,ivory,true);}
void number(const String &v){dial::text(v,129,260,&lv_font_montserrat_28,gold,true);}
String signedN(int n){return n>0?String("+")+n:String(n);}
String count(int n){return String(n)+"  "+ui::tr("in pack");}
String amount(int n){return String(n)+(paying?String(" ")+units[coin]:String(""));}
JsonVariantConst rowFor(JsonDocument &book){
    for(auto row:book["pack"].as<JsonArrayConst>())if(row["id"]==itemId)return row;
    return {};
}
bool offerTablePotion(JsonDocument &book,const pack::Bag &bag){
    if(bag.tablePotions)return true;
    for(auto row:book["pack"].as<JsonArrayConst>())if(row["dice"]==2)return false;
    return true;
}
int pages(JsonDocument &book,const pack::Bag &bag){
    int total=int(book["pack"].size())+offerTablePotion(book,bag);
    return std::max(1,(total+1)/2);
}
pack::Item selected(JsonDocument &book){return localItem?pack::tablePotion:pack::item(rowFor(book));}
void advance(int steps,JsonDocument &book){
    if(face==Face::Bag)page=wrap(page+steps,pages(book,session::read(character,casting::build(book)).bag));
    if(face==Face::Purse)coin=wrap(coin+steps,pack::coinCount);
    if(face==Face::Use){
        auto item=selected(book);
        if(item.dice)target=pack::Target(wrap(int(target)+steps,3));
        else timing=pack::Timing(wrap(int(timing)+steps,4));
    }
    if(face==Face::Adjust){
        int direction=paying && spending?-1:1;
        int64_t next=int64_t(delta)+int64_t(direction)*steps*stepSize;
        delta=std::clamp<int64_t>(next,paying && !spending?1:-INT_MAX,paying && spending?-1:INT_MAX);
    }
}
bool record(const session::Build &b,session::Change c,Face after,const String &message){
    if(auto err=session::commit(character,b,c)){notice=ui::tr(err);return false;}
    returnTo=after;face=Face::Receipt;receiptOp=c.op;notice=message;pulse();return true;
}
bool known(JsonDocument &book){return book["schema"]==casting::version;}
}

void open(const Sheet &sheet,bool turns){
    character=sheet.id;face=turns?Face::Turn:Face::Bag;page=coin=0;itemId=0;localItem=false;notice="";
}
uint32_t owner(){return character;}

void draw(const Sheet &sheet){
    auto &book=bookui::currentBook(sheet);auto b=casting::build(book);auto s=session::read(character,b);
    if(auto art=playBackdrop(false)){auto *im=lv_img_create(lv_scr_act());lv_img_set_src(im,art);lv_obj_set_style_img_opa(im,LV_OPA_70,0);}
    bool turns=face==Face::Turn || face==Face::Next || (face==Face::Receipt && returnTo==Face::Turn);
    arcText(ui::tr(turns?"My turn":"Pack"),ArcLabel::PlayTitle,gold);
    if(!known(book) && !turns){
        title(ui::tr("Refresh your sheet"));line(ui::tr("Import potions, scrolls and coins"),166);
        mainButton.draw(ui::tr("Tap to refresh"),dark,gold);returnArc(ui::tr("Back to sheet"));return;
    }
    auto row=rowFor(book);auto item=selected(book);int stock=item.id || item.local?pack::stock(s.bag,item):0;
    int money=pack::balance(s.bag,b.bag,coin);
    const char *name=localItem?ui::tr("Table healing potions"):(row["name"]|"?");
    switch(face){
    case Face::Bag:{
        page=std::min(page,pages(book,s.bag)-1);
        int gp=pack::balance(s.bag,b.bag,0);
        first.draw(String(ui::tr("Purse"))+(gp>=0?String("   ")+gp+" GP":String("")),dark,gold);
        auto items=book["pack"].as<JsonArrayConst>();
        for(int j=0;j<2;++j){
            int index=page*2+j;auto r=items[index];
            if(r.isNull()){
                if(index==int(items.size()) && offerTablePotion(book,s.bag)){
                    auto n=count(s.bag.tablePotions);
                    (j?third:second).draw(ui::tr(s.bag.tablePotions?"Table healing potions":"Add healing potion"),ivory,dark,s.bag.tablePotions?n.c_str():nullptr);
                }
                break;
            }
            auto it=pack::item(r);auto n=count(pack::stock(s.bag,it));
            (j?third:second).draw(r["name"]|"?",ivory,dark,n.c_str());
        }
        if(!items.size())line(ui::tr(b.bag.find?"No imported consumables":"Inventory unavailable"),218);
        if(pages(book,s.bag)>1)line(String(LV_SYMBOL_LEFT "  ")+(page+1)+" / "+pages(book,s.bag)+"  " LV_SYMBOL_RIGHT,267,176);
        break;
    }
    case Face::Purse:
        title(ui::tr("Purse"));number(String(LV_SYMBOL_LEFT "  ")+units[coin]+"  " LV_SYMBOL_RIGHT);
        dial::text(money<0?ui::tr("Balance unavailable"):String(money),179,265,&lv_font_montserrat_36,ivory,true);
        if(money>=0){left.draw(ui::tr("Spend"),ivory,dark);right.draw(ui::tr("Earn"),dark,gold);}
        break;
    case Face::Item:
        title(name);number(count(stock));
        if(item.dice)line(String(item.dice)+"d4 + "+item.bonus+" HP",181);
        else line(ui::tr(row["scroll"]==true?"Resolve the scroll at the table":"Resolve the effect at the table"),181);
        left.draw(ui::tr("Adjust"),ivory,dark);
        if(stock)right.draw(ui::tr("Use"),dark,gold);
        break;
    case Face::Adjust:{
        title(paying?ui::tr(spending?"Spend":"Earn"):name);number(String(LV_SYMBOL_MINUS "   ")+(paying?String(abs(delta)):signedN(delta))+"   " LV_SYMBOL_PLUS);
        int old=paying?money:stock;
        auto next=int64_t(old)+delta;
        line(next<0 || next>INT_MAX?ui::tr("Check quantity"):amount(old)+"  " LV_SYMBOL_RIGHT "  "+amount(int(next)),162);
        dial::Capsule{180,201,148,44}.draw(String(ui::tr("Step"))+"   "+stepSize,ivory,dark);
        bool edited=paying?(s.bag.editedCoins&(1u<<coin)):!item.local && stock!=item.stock;
        if(edited)left.draw(ui::tr("Use sheet"),ivory,dark);
        right.draw(ui::tr("Confirm"),dark,gold);
        break;
    }
    case Face::Reset:
        title(ui::tr("Use sheet count?"));number(amount(paying?b.bag.money[coin]:item.stock));
        line(ui::tr("Follow future sheet updates"),181);mainButton.draw(ui::tr("Confirm"),dark,gold);break;
    case Face::Use:{
        title(notice.length()?notice:String(name));
        if(item.dice){
            dial::text(String(item.dice)+"d4 + "+item.bonus+" HP",120,238,&lv_font_montserrat_28,gold,true);
            second.draw(ui::tr(targets[int(target)]),ivory,dark);
            auto cost=String(modern?"2024":"2014")+"  •  "+ui::tr(pack::timingName(modern?pack::Timing::Bonus:pack::Timing::Action));
            dial::Capsule{180,209,208,44}.draw(cost,ivory,dark);
        }else{
            if(row["basicScroll"]==true){
                line(ui::tr("Check your class spell list"),112);
                line(ui::tr("Spell attack +5  •  DC 13"),134);
            }else line(ui::tr("Resolve the effect at the table"),119);
            second.draw(ui::tr(pack::timingName(timing)),ivory,dark);
            line(ui::tr("Confirm only when consumed"),211);
        }
        dial::Capsule{180,257,178,44}.draw(String(ui::tr(item.dice?"Roll & use":"Use one"))+"   "+stock+" " LV_SYMBOL_RIGHT " "+(stock-1),dark,gold);
        break;
    }
    case Face::Turn:
        if(!s.turn.tracking){
            title(ui::tr("My turn"));line(ui::tr("Begin at the start of your turn"),165);
            mainButton.draw(ui::tr("Start turn"),dark,gold);break;
        }
        for(int i=0;i<3;++i){
            auto t=pack::Timing(i+1);bool spent=s.turn.used&pack::timingMask(t);
            dial::Capsule{180,91+52*i,i==1?238:201,44}.draw(String(ui::tr(pack::timingName(t)))+"   "+ui::tr(spent?"Spent":"Ready"),spent?ivory:dark,spent?dark:mint);
        }
        left.draw(ui::tr("Finish"),ivory,dark);right.draw(ui::tr("Next turn"),dark,gold);break;
    case Face::Next:
        title(ui::tr("Next turn?"));line(ui::tr("Refresh action, bonus and reaction"),171);
        mainButton.draw(ui::tr("Start turn"),dark,gold);break;
    case Face::Receipt:
        title(ui::tr("Recorded"));dial::text(notice,145,271,&lv_font_montserrat_20,ivory,true);
        if(returnTo==Face::Item)line(count(stock),191);
        if(returnTo==Face::Purse)line(amount(money),191);
        left.draw(ui::tr("Undo"),ivory,dark);right.draw(ui::tr("Done"),dark,gold);break;
    }
    if(notice.length() && face!=Face::Receipt && face!=Face::Use)line(notice,281,220);
    returnArc(ui::tr(face==Face::Bag || face==Face::Turn?"Back to sheet":face==Face::Next || (face==Face::Receipt && returnTo==Face::Turn)?"My turn":face==Face::Use || face==Face::Adjust || face==Face::Reset || face==Face::Receipt?"Back":"Back to pack"));
}

bool turn(const Sheet &sheet,int steps){
    if(!steps || sheet.id!=character || (face!=Face::Bag && face!=Face::Purse && face!=Face::Use && face!=Face::Adjust))return false;
    notice="";advance(steps,bookui::currentBook(sheet));return true;
}
bookui::Event touch(const Sheet &sheet,int x,int y,int dx,int dy){
    using E=bookui::Event;using session::Op;
    if(sheet.id!=character)return E::Back;
    bool tap=abs(dx)<=18 && abs(dy)<=18;
    if((dx>=42 && dx>abs(dy)) || (tap && dial::inReturn(x,y))){
        notice="";
        switch(face){
        case Face::Bag:case Face::Turn:return E::Back;
        case Face::Use:face=Face::Item;break;
        case Face::Reset:face=Face::Adjust;break;
        case Face::Adjust:face=paying?Face::Purse:Face::Item;break;
        case Face::Next:face=Face::Turn;break;
        case Face::Receipt:face=returnTo;break;
        default:face=Face::Bag;break;
        }
        return E::Paint;
    }
    if(!tap)return abs(dy)>=42 && abs(dy)>=abs(dx) && turn(sheet,dy<0?1:-1)?E::Paint:E::None;
    auto &book=bookui::currentBook(sheet);
    if(!known(book) && face!=Face::Turn && face!=Face::Next && face!=Face::Receipt)return mainButton.contains(x,y)?E::Refresh:E::None;
    auto b=casting::build(book);auto s=session::read(character,b);auto item=selected(book);
    session::Change c{Op::Stock};c.itemId=itemId;c.tablePotion=localItem;
    if(face!=Face::Receipt)notice="";
    switch(face){
    case Face::Bag:
        if(first.contains(x,y)){face=Face::Purse;paying=true;coin=0;}
        else if(second.contains(x,y) || third.contains(x,y)){
            int index=page*2+(third.contains(x,y)?1:0);
            auto id=book["pack"][index]["id"]|0u;
            bool local=index==int(book["pack"].size()) && offerTablePotion(book,s.bag);
            if(id || local){
                itemId=id;localItem=local;paying=false;face=Face::Item;
                if(local && !s.bag.tablePotions){delta=stepSize=1;face=Face::Adjust;}
            }
        }else if(pages(book,s.bag)>1 && dial::Capsule{180,267,148,44}.contains(x,y))advance(x<180?-1:1,book);
        break;
    case Face::Purse:
        if(selector.contains(x,y))advance(x<180?-1:1,book);
        else if(pack::balance(s.bag,b.bag,coin)>=0 && (left.contains(x,y) || right.contains(x,y))){
            spending=left.contains(x,y);delta=spending?-1:1;stepSize=1;face=Face::Adjust;
        }
        break;
    case Face::Item:
        if(!item.id && !item.local){face=Face::Bag;break;}
        if(left.contains(x,y)){delta=1;stepSize=1;face=Face::Adjust;}
        else if(right.contains(x,y) && pack::stock(s.bag,item)>0){
            target=pack::Target::Self;modern=b.revisedRest;timing=pack::Timing::None;face=Face::Use;
        }
        break;
    case Face::Adjust:case Face::Reset:{
        bool reset=face==Face::Reset;
        if(!reset && selector.contains(x,y)){advance(x<180?-1:1,book);break;}
        if(!reset && dial::Capsule{180,201,148,44}.contains(x,y)){stepSize=stepSize==100?1:stepSize*10;break;}
        if(!reset && left.contains(x,y)){
            if(paying?(s.bag.editedCoins&(1u<<coin)):!item.local && pack::stock(s.bag,item)!=item.stock)face=Face::Reset;
            break;
        }
        if(!(reset?mainButton:right).contains(x,y) || (!reset && !delta))break;
        c.op=paying?Op::Coin:Op::Stock;c.pool=coin;c.restore=reset;c.amount=delta;
        record(b,c,paying?Face::Purse:Face::Item,reset?String(ui::tr("Sheet count restored")):signedN(delta)+(paying?String(" ")+units[coin]:String("  ")+ui::tr("in pack")));
        break;
    }
    case Face::Use:
        if(second.contains(x,y)){advance(1,book);break;}
        if(item.dice && dial::Capsule{180,209,208,44}.contains(x,y)){modern=!modern;break;}
        if(dial::Capsule{180,257,178,44}.contains(x,y)){
            c.op=Op::ItemUse;c.timing=item.dice?(modern?pack::Timing::Bonus:pack::Timing::Action):timing;
            c.target=target;c.amount=item.bonus;
            for(int i=0;i<item.dice;++i)c.amount+=rollaid::die(4);
            String result=ui::tr("One item used");
            if(item.dice){
                result=String(c.amount)+" HP";
                if(target!=pack::Target::Ally)result+="   "+String(s.hp.current)+" " LV_SYMBOL_RIGHT " "+std::min(int(s.hp.maximum),s.hp.current+c.amount);
            }
            record(b,c,Face::Item,result);
        }
        break;
    case Face::Turn:
        if(s.turn.tracking){
            for(int i=0;i<3;++i)if(dial::Capsule{180,91+52*i,i==1?238:201,44}.contains(x,y)){
                c.op=Op::MarkTurn;c.timing=pack::Timing(i+1);
                if(auto err=session::commit(character,b,c))notice=ui::tr(err);else pulse();
                return E::Paint;
            }
            if(right.contains(x,y))face=Face::Next;
            if(left.contains(x,y)){c.op=Op::EndTracking;record(b,c,Face::Turn,ui::tr("Turn tracking off"));}
        }else if(mainButton.contains(x,y)){
            c.op=Op::StartTurn;if(auto err=session::commit(character,b,c))notice=ui::tr(err);else pulse();
        }
        break;
    case Face::Next:
        if(mainButton.contains(x,y)){c.op=Op::StartTurn;record(b,c,Face::Turn,ui::tr("Resources ready"));}
        break;
    case Face::Receipt:
        if(right.contains(x,y)){face=returnTo;notice="";}
        else if(left.contains(x,y)){
            Op op;
            if(session::last(character,op) && op==receiptOp && session::undo(character)){face=returnTo;notice="";pulse();}
            else notice=ui::tr("Could not undo");
        }
        break;
    }
    return E::Paint;
}
}
