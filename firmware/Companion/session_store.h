#pragma once
#include "session_rules.h"
#include <cstdint>
namespace session {
bool readable(uint32_t id);
State read(uint32_t id,const Build &b);
vitals::Health health(uint32_t id);
const char *commit(uint32_t id,const Build &b,Change c);
bool undo(uint32_t id);
bool last(uint32_t id,Op &op);
void reconcile(uint32_t id,const Build &build);
uint32_t bondChoice(uint32_t character);
bool setBondChoice(uint32_t character,uint32_t item);
void forget(uint32_t id);
}
