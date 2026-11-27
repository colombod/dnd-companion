#pragma once
#include "sheet.h"
#include <Arduino.h>
namespace bookui {
enum class Event { None, Paint, Back, Refresh, Roll, Health };
void open(const Sheet &sheet,bool actions);
uint32_t owner();
void draw(const Sheet &sheet,const String &status);
Event touch(const Sheet &sheet,int x,int y,int dx,int dy);
bool turn(int steps);
void describe();
JsonDocument &currentBook(const Sheet &sheet);
}
