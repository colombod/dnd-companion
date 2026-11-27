#pragma once
#include "sheet.h"
#include "move_roll.h"
#include <Arduino.h>

namespace play {
enum class View { Select, Ready, Rolling, Result, Tray, PoolResult };
enum class Event { None, Paint, Back, Refresh };
void open(uint32_t id,bool saves);
void openTray(uint32_t id);
void openMove(uint32_t id,const String &name,const String &label,const moves::Roll &roll);
bool fromMove();
uint32_t owner();
void draw(const Sheet &sheet,const String &status);
Event touch(const Sheet &sheet,int x,int y,int dx,int dy);
int trayDieAt(int x,int y);
bool removeDie(int die);
bool wheel(const Sheet &sheet,int turns,uint32_t now);
bool tick(uint32_t now);
void describe(const Sheet &sheet);
void forgetMix(uint32_t id);
void sheetLandmark(bool right,int pair);
bool hitsLandmark(int x,int y,bool saves);
}
