#pragma once
//rtc ds3231 pe i2c 0x68
//toate apelurile rtc.*() sunt i2c, protejate cu lock_i2c

#include <RTClib.h>
#include "vital_data.h"

class RtcSensor {
public:
    //apelat din setup() — task-urile nu au pornit inca, nu avem nevoie de mutex
    bool init() {
        if (!rtc.begin()) {
            Serial.println("[rtc] ds3231 negasit (i2c 0x68)!");
            return initOk = false;
        }
        if (rtc.lostPower()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            Serial.println("[rtc] ora resetata la momentul compilarii");
        }
        initOk = true;
        DateTime now = rtc.now();
        Serial.printf("[rtc] ok — %04d-%02d-%02d %02d:%02d:%02d\n",
                      now.year(), now.month(), now.day(),
                      now.hour(), now.minute(), now.second());
        return true;
    }

    //apelat din tasktemp la fiecare 2s — are nevoie de mutex
    void actualizeazaOra(VitalData* v, SemaphoreHandle_t mtx) {
        if (!initOk) return;

        if (!LOCK_I2C()) return;
        DateTime now = rtc.now();
        UNLOCK_I2C();

        char buf[20];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());

        if (xSemaphoreTake(mtx, pdMS_TO_TICKS(5)) == pdTRUE) {
            strncpy(v->datetime_str, buf, sizeof(v->datetime_str) - 1);
            xSemaphoreGive(mtx);
        }
    }

    //apelat din taskbutoane / tasksd — are nevoie de mutex
    uint32_t unixTime() {
        if (!initOk) return 0;
        if (!LOCK_I2C()) return 0;
        uint32_t t = rtc.now().unixtime();
        UNLOCK_I2C();
        return t;
    }

    //apelat din callback-ul ws (core 0) — are nevoie de mutex
    void seteazaOra(int an, int luna, int zi, int h, int min, int sec) {
        if (!LOCK_I2C()) return;
        rtc.adjust(DateTime(an, luna, zi, h, min, sec));
        UNLOCK_I2C();
        Serial.printf("[rtc] ora setata: %04d-%02d-%02d %02d:%02d:%02d\n",
                      an, luna, zi, h, min, sec);
    }

    bool esteBun() const { return initOk; }

private:
    RTC_DS3231 rtc;
    bool       initOk = false;
};
