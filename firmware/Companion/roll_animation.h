#pragma once
#include <cstdint>

namespace rollanim {
void begin(int steps,uint32_t now);
void sample(int steps,uint32_t now);
void draw();
bool finished(uint32_t now);
unsigned duration();
int strength();
int direction();
}
