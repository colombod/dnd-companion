#pragma once
#include "book_ui.h"
#include "vitals.h"

namespace healthui {
void open(const Sheet &sheet,bool effects);
uint32_t owner();
void draw(const Sheet &sheet);
bool tick(uint32_t now);
bool turn(const Sheet &sheet,int steps);
bookui::Event touch(const Sheet &sheet,int x,int y,int dx,int dy);
String reminder(const vitals::Aids &a,vitals::Roll roll,int skill=-1);
}
