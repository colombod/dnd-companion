#pragma once
#include "equipment_import.h"
#include "cast_import.h"
#include "session_store.h"

// Only call with a verified book from the active storage bank.
inline equipment::WeaponBond reconcilePlayState(uint32_t id,JsonDocument &book){
    uint32_t saved=session::bondChoice(id);
    auto bond=equipment::resolveBond(book.as<JsonObjectConst>(),saved);
    if(saved && (bond.marked<=1 || saved!=bond.item))session::setBondChoice(id,0);
    if(casting::playable(book["schema"]|0u))session::reconcile(id,casting::build(book));
    return bond;
}
