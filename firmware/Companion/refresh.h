#pragma once
#include "sheet.h"

struct RefreshResult { Sheet sheet; char error[80]; bool notModified; uint8_t *book; size_t bookSize; };
uint32_t refreshKiB();
bool startRefresh(uint32_t id, const SourceTag &tag);
bool takeRefresh(RefreshResult &result);
