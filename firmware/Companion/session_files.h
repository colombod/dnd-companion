#pragma once
#include <cstddef>
#include <cstdint>

// The current bank is selected by a single small Preferences record.
namespace sessionfiles {
bool read(uint32_t character,bool bank,void *record,size_t bytes);
bool write(uint32_t character,bool bank,const void *record,size_t bytes);
void erase(uint32_t character);
}
