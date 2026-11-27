#pragma once
#include <ArduinoJson.h>
#include <Arduino.h>
void networksBegin();
void networksPoll(bool setupActive);
const char *connectNetwork(const String &ssid, const String &password, bool reuse);
const char *saveNetwork(const String &ssid,const String &pass,bool reuse);
const char *forgetNetwork(const String &ssid);
void scanNetworks();
void networkStatus(JsonDocument &doc);
