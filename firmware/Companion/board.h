#pragma once
#include <Arduino.h>

void boardBegin();
int wheelMovement();
bool finger(uint16_t &x, uint16_t &y);
bool pulse();
void boardStatus();
void samplePower();
String powerSummary();
int railMillivolts();
extern "C" unsigned display_transfers();
