#pragma once
#include <Arduino.h>
#include "rim.h"

enum class ArcLabel { Status, Level, Name, Classes, Cache, Action, PlayTitle, Return };

void arcText(const String &text, ArcLabel track, uint32_t color);
String connectionArc(const String &network, const String &suffix);
String displayNetworkName(const String &ssid);
void returnArc(const char *caption);
