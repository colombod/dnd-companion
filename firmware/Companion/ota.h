#pragma once
#include <Arduino.h>

namespace ota {
enum class Stage { Idle, Checking, Available, Current, Installing, Restarting, Failed, RestorePreview };
struct Release {
    char version[16]{}, note[97]{}, digest[65]{}, url[384]{};
    uint32_t size{};
};
struct View {
    Stage stage = Stage::Idle;
    const char *message = "";
    Release release{};
    unsigned percent{};
};
const View &view();
bool busy();
void start(bool confirm, bool sheetBusy);
bool poll();
const char *version();
const char *buildId();
void describe();
bool needsBootCheck();
void confirmHealthyBoot(bool savesReadable);
const char *previousVersion();
void previewRestore();
void cancelPreview();
void restore(bool sheetBusy);
}
