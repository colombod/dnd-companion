#pragma once
#include <ArduinoJson.h>
namespace defenseui {
void open();
bool overview();
bool back();
void draw(JsonDocument &book,uint32_t id,int armorSpell);
bool healthTouched(uint32_t id,int x,int y);
bool castTouched(int x,int y);
bool turn(int step);
bool tap(JsonDocument &book,uint32_t id,int x,int y);
}
