#include "board.h"
#include "app.h"

void setup() {
    Serial.begin(115200);
    boardBegin();
    appBegin();
}

void loop() {
    appLoop();
    delay(5);
}
