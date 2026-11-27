#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <ArduinoJson.h>

enum class ArtLayer { Background, Icon };
const lv_img_dsc_t *backgroundFor(uint32_t id);
const lv_img_dsc_t *iconFor(uint32_t id);
bool hasBackground(uint32_t id);
uint32_t iconSource(const char *url);
bool iconCurrent(uint32_t id, uint32_t source);
bool saveArtwork(uint32_t id, ArtLayer layer, const uint8_t *jpeg,size_t length,uint32_t source=0);
bool removeBackground(uint32_t id);
void removePortrait(uint32_t id);
const lv_img_dsc_t *playBackdrop(bool dice);
const lv_img_dsc_t *bookBackdrop();
const lv_img_dsc_t *bootBackdrop();
bool writeMoveBook(uint32_t id,bool bank,const uint8_t *data,size_t size);
bool readMoveBook(uint32_t id,bool bank,uint32_t hash,JsonDocument &book);
