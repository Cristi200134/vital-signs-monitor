#pragma once
//temperatura ds18b20 prin onewire (sonda)
//rezolutie 12-bit (0.0625 'c)
//task-ul apeleaza citestesi() la fiecare temp_interval_ms (2000ms)
//folosim requesttemperatures() neblocking + citire dupa 800ms

#include <OneWire.h>
#include <DallasTemperature.h>
#include "vital_data.h"
#include "config.h"

class TempSensor {
public:
    TempSensor()
        : ow(PIN_DS18B20), dal(&ow) {}

    bool init() {
        dal.begin();
        uint8_t nDev = dal.getDeviceCount();

        if (nDev == 0) {
            Serial.println("[temp] eroare: ds18b20 negasit pe onewire!");
            initOk = false;
            return false;
        }

        //rezolutie 12-bit (0.0625 'c)
        dal.setResolution(12);

        //neblocking — requesttemperatures() returneaza imediat
        //citim dupa 800ms (conversia 12-bit max 750ms)
        dal.setWaitForConversion(false);

        Serial.printf("[temp] ds18b20 ok (%d senzori, 12-bit)\n", nDev);
        initOk = true;

        //prima cerere de conversie
        dal.requestTemperatures();
        reqTime = millis();
        return true;
    }

    //apelat din tasktemp la fiecare temp_interval_ms
    void citesteSi(VitalData* vitals, SemaphoreHandle_t mtx) {
        if (!initOk) return;

        //asteptam 800ms de la cerere (conversia 12-bit dureaza max 750ms)
        if (millis() - reqTime < 800) return;

        float t = dal.getTempCByIndex(0);

        //trimitem noua cerere inainte sa scriem valoarea
        dal.requestTemperatures();
        reqTime = millis();

        //validare: ds18b20 returneaza -127 la eroare
        if (t == DEVICE_DISCONNECTED_C || t < -40.0f || t > 85.0f) {
            if (xSemaphoreTake(mtx, pdMS_TO_TICKS(5)) == pdTRUE) {
                vitals->temp_valid = false;
                xSemaphoreGive(mtx);
            }
            Serial.println("[temp] citire invalida sau senzor deconectat");
            return;
        }

        if (xSemaphoreTake(mtx, pdMS_TO_TICKS(5)) == pdTRUE) {
            vitals->temp_c     = t;
            vitals->temp_valid = true;
            xSemaphoreGive(mtx);
        }

        Serial.printf("[temp] %.2f °c\n", t);
    }

private:
    OneWire          ow;
    DallasTemperature dal;
    bool             initOk  = false;
    uint32_t         reqTime = 0;
};
