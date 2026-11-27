#pragma once
#include "book_ui.h"
namespace resources {
void open(uint32_t id,bool rest);
uint32_t owner();
void draw(const Sheet &s,const String &notice);
bookui::Event touch(const Sheet &s,int x,int y,int dx,int dy);
bool turn(int step);
void describe(const Sheet &s);
}
