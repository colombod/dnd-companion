#pragma once
#include <WebServer.h>
#include "sheet.h"
void photoRoutes(WebServer &server);
void photoPoll();
const Sheet *getSheet(uint32_t id);
void artworkChanged();
