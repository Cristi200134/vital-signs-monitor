#pragma once
//coduri culori:
// verde solid = monitorizare activa, totul ok
//rosu 4hz= critic (alarma nivel 3)
//albastru clipind= standby (gestionat in taskalarme)

#include <Arduino.h>
#include "config.h"

class LedControl {
public:
    void init() {
        pinMode(PIN_LED_R, OUTPUT);
        pinMode(PIN_LED_G, OUTPUT);
        pinMode(PIN_LED_B, OUTPUT);
        off();
    }

    void off() {
        digitalWrite(PIN_LED_R, LOW);
        digitalWrite(PIN_LED_G, LOW);
        digitalWrite(PIN_LED_B, LOW);
    }

    void aplicaAlarma(uint8_t nivel, uint8_t batPct) {
        uint32_t tNow = millis();

        //baterie critica suprascrie alarma clinica
        if (batPct < 5 && batPct > 0) {
            bool on = (tNow / 250) % 2;
            digitalWrite(PIN_LED_R, on ? HIGH : LOW);
            digitalWrite(PIN_LED_G, on ? HIGH : LOW);
            digitalWrite(PIN_LED_B, LOW);
            return;
        }

        switch (nivel) {
            case 0: //normal — verde solid
                digitalWrite(PIN_LED_R, LOW);
                digitalWrite(PIN_LED_G, HIGH);
                digitalWrite(PIN_LED_B, LOW);
                break;

            case 1: //info — verde solid
                digitalWrite(PIN_LED_R, HIGH);
                digitalWrite(PIN_LED_G, HIGH);
                digitalWrite(PIN_LED_B, LOW);
                break;

            case 2: { //atentionare — verde clip 1hz
                bool on = (tNow / 500) % 2;
                digitalWrite(PIN_LED_R, on ? HIGH : LOW);
                digitalWrite(PIN_LED_G, on ? HIGH : LOW);
                digitalWrite(PIN_LED_B, LOW);
                break;
            }

            case 3: { //critic — rosu clip rapid 4hz
                bool on = (tNow / 125) % 2;
                digitalWrite(PIN_LED_R, on ? HIGH : LOW);
                digitalWrite(PIN_LED_G, LOW);
                digitalWrite(PIN_LED_B, LOW);
                break;
            }

            default:
                digitalWrite(PIN_LED_R, LOW);
                digitalWrite(PIN_LED_G, HIGH);
                digitalWrite(PIN_LED_B, LOW);
        }
    }
};
