#include <Arduino.h>
#include "LEDBoard.h"


using namespace Frangitron;

LEDBoard ledBoard;


void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    ledBoard.init();
}

void loop() {
    ledBoard.loop();
}


void loop1() {
    ledBoard.loop1();
}
