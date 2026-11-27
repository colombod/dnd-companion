#include "../firmware/Companion/profile_state.h"
#include <cassert>

int main(){
    JsonDocument book;
    deserializeJson(book,R"({"schema":4,"gear":{"pactEnabled":true,"pactMarks":2},"actions":[
        {"weapon":true,"itemId":7,"toHit":3,"pactRoll":{"toHit":8}},
        {"weapon":true,"itemId":8,"toHit":2,"pactRoll":{"toHit":8}}]})");
    assert(session::setBondChoice(1,7) && session::setBondChoice(2,8));
    assert(reconcilePlayState(1,book).item==7);
    book["gear"]["pactEnabled"]=false;
    assert(!reconcilePlayState(1,book).item && !session::bondChoice(1));
    book["gear"]["pactEnabled"]=true;
    assert(!reconcilePlayState(1,book).item && reconcilePlayState(2,book).item==8);
    book["actions"].as<JsonArray>().remove(1);
    assert(!reconcilePlayState(2,book).item && !session::bondChoice(2));
    // The earlier firmware allowed selecting raw marks on a book with no grant information.
    book["schema"]=3;book.remove("gear");
    assert(session::setBondChoice(1,7));
    assert(!reconcilePlayState(1,book).item && !session::bondChoice(1));
}
