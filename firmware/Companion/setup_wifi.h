#pragma once
#include <Arduino.h>

void wifiBegin();
bool setupOpen();
void setupClose();
void setupPoll();
extern String accessName, accessPassword;
extern bool setupActive;
uint32_t characterLinkId(const String &link);
const char *importLink(uint32_t id);
const char *queueLinks(const String &input);
String libraryStatus();
const char *removeLink(uint32_t id);
const char *editLink(uint32_t oldId, uint32_t newId);
const char *decideReview(uint32_t ticket, bool apply);
bool setPickerArt(const String &mode);
bool setDisplayLanguage(const String &language);
