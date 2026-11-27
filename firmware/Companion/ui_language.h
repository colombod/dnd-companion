#pragma once

namespace ui {
enum class Language { English, Italian, Spanish };
Language language();
const char *code();
void loadLanguage();
bool setLanguage(const char *code);
const char *tr(const char *english);
}
