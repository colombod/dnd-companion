#pragma once
#include <ArduinoJson.h>
#include <Arduino.h>
void networksBegin();
void networksPoll();
const char *connectNetwork(const String &ssid, const String &password, bool reuse);
const char *forgetNetwork(const String &ssid);
void scanNetworks();
void networkStatus(JsonDocument &doc);
