#pragma once
#include "ui_language.h"
#include "phone_pages.inc"

inline const char *phonePage(){
    switch(ui::language()){
    case ui::Language::Italian:return phone_it;
    case ui::Language::Spanish:return phone_es;
    default:return phone_en;
    }
}
