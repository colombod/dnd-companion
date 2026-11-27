#pragma once
#include "book_ui.h"
namespace packui {
void open(const Sheet &sheet,bool turns);
uint32_t owner();
void draw(const Sheet &sheet);
bool turn(const Sheet &sheet,int steps);
bookui::Event touch(const Sheet &sheet,int x,int y,int dx,int dy);
}
