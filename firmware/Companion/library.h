#pragma once
#include "sheet.h"
#include <Arduino.h>

class Library {
public:
    static constexpr int capacity = 12;
    Sheet records[capacity]{};
    int count = 0, active = 0;
    bool hasSelection = false;
    void begin();
    bool readable() const;
    int find(uint32_t id) const;
    bool save(const Sheet &sheet, uint32_t replacing = 0);
    bool remove(uint32_t id);
    bool select(int index);
    const char *pickerArt() const;
    bool setPickerArt(const String &value);
private:
    uint8_t appearance=0;
};
