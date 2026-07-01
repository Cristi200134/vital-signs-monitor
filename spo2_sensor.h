#pragma once
//spo2 din max30102 (i2c 0x57, 50 sps)
//pipeline per esantion (20ms):
//  1. lock_i2c -> sensor.check() -> getir/red -> unlock_i2c
//  2. verifica daca degetul e pe senzor (ir > 50000)
//  3. calcul spo2 din raportul r = (ac_red/dc_red) / (ac_ir/dc_ir)
//  4. acumulare waveform pentru websocket
//  5. detectie respiratie din modulatia dc a ir-ului

#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>
#include "vital_data.h"
#include "config.h"

static const int  SPO2_WIN      = 100;     //fereastra calcul spo2
static const float IR_MIN_DEGET = 50000.0f; //sub pragul asta consideram ca nu e deget

class Spo2Sensor {
public:
    bool init() {
        if (!LOCK_I2C()) return false;
        bool ok = sensor.begin(Wire, I2C_SPEED_FAST);
        if (ok) {
            sensor.setup(60, 4, 2, 50, 411, 4096);
            sensor.setPulseAmplitudeRed(0x1F);
            sensor.setPulseAmplitudeIR(0x1F);
        }
        UNLOCK_I2C();

        if (!ok) {
            Serial.println("[spo2] max30102 negasit (i2c 0x57)!");
            return initOk = false;
        }
        initOk = true;
        Serial.println("[spo2] max30102 ok — 50 sps");
        return true;
    }

    //apelat din taskspo2 la fiecare 20ms
    void citesteSi(VitalData* v, SemaphoreHandle_t mtx) {
        if (!initOk) return;

        //citire i2c — serializata cu mutex (evita contention cu ecg)
        if (!LOCK_I2C()) return;
        sensor.check();
        bool avail  = sensor.available();
        uint32_t ir = 0, red = 0;
        if (avail) {
            ir  = sensor.getIR();
            red = sensor.getRed();
            sensor.nextSample();
        }
        UNLOCK_I2C();

        if (!avail) return;

        bool deget = (ir > (uint32_t)IR_MIN_DEGET);
        if (!deget) {
            irIdx = redIdx = 0;
            dcTracker = 0.0f; acPeak = 1.0f;
            //reset state-ul pentru respiratie ca la urmatoarea atingere sa porneasca curat
            respFast = 0.0f; respSlow = 0.0f; respPrev = 0.0f;
            respLastPeak = 0; respCount = 0; respIdx = 0;
            if (LOCK_VITALS()) {
                v->spo2 = 0; v->spo2_valid = false;
                v->pi   = 0; v->spo2_batch_idx = 0;
                v->resp_rpm = 0; v->resp_valid = false;
                UNLOCK_VITALS();
            }
            return;
        }

        //calcul respiratie din modulatia dc a ir-ului
        //dc-ul ir variaza aprox.0.5-2% cu respiratia (volumul sangvin in deget)
        //bandpass: respfast (lp 0.8 hz, scoate hr) minus respslow (lp 0.016 hz, scoate drift)
        //rezultat: 0.016-0.8 hz = 1-48 rpm
        if (respFast == 0.0f) {
            respFast = (float)ir;
            respSlow = (float)ir;
        } else {
            respFast = respFast * 0.9f   + (float)ir * 0.1f;
            respSlow = respSlow * 0.998f + respFast  * 0.002f;
        }
        float respSig = respFast - respSlow;

        //rising zero-crossing → eveniment respirator
        uint32_t tNow = millis();
        if (respSig > 0 && respPrev <= 0 && (tNow - respLastPeak) > 1500) {
            if (respLastPeak > 0) {
                uint32_t period = tNow - respLastPeak;
                if (period > 1500 && period < 10000) {  //6-40 rpm
                    respIntervals[respIdx] = (uint16_t)period;
                    respIdx = (respIdx + 1) % 5;
                    if (respCount < 5) respCount++;

                    if (respCount >= 3) {
                        uint32_t sum = 0;
                        for (uint8_t i = 0; i < respCount; i++) sum += respIntervals[i];
                        float rpm = 60000.0f / ((float)sum / respCount);
                        if (rpm >= 6.0f && rpm <= 40.0f) {
                            if (LOCK_VITALS()) {
                                v->resp_rpm   = rpm;
                                v->resp_valid = true;
                                UNLOCK_VITALS();
                            }
                        }
                    }
                }
            }
            respLastPeak = tNow;
        }
        respPrev = respSig;

        //timeout 30s fara respiratie -> invalid
        if (respLastPeak > 0 && (tNow - respLastPeak) > 30000 && respCount > 0) {
            if (LOCK_VITALS()) { v->resp_valid = false; v->resp_rpm = 0.0f; UNLOCK_VITALS(); }
            respCount = 0;
        }

        //acumulare in fereastra alunecatoare
        irBuf[irIdx   % SPO2_WIN] = ir;
        redBuf[redIdx % SPO2_WIN] = red;
        irIdx++; redIdx++;

        //calcul spo2 dupa ce s-a umplut fereastra
        if (irIdx >= SPO2_WIN) {
            float dcIR = 0, dcRed = 0;
            for (int i = 0; i < SPO2_WIN; i++) { dcIR += irBuf[i]; dcRed += redBuf[i]; }
            dcIR /= SPO2_WIN; dcRed /= SPO2_WIN;

            float acIR = 0, acRed = 0;
            for (int i = 0; i < SPO2_WIN; i++) {
                float dIR = irBuf[i] - dcIR, dRed = redBuf[i] - dcRed;
                acIR += dIR * dIR; acRed += dRed * dRed;
            }
            acIR  = sqrtf(acIR  / SPO2_WIN);
            acRed = sqrtf(acRed / SPO2_WIN);

            float R    = (acRed / dcRed) / (acIR / dcIR);
            float spo2 = constrain(-45.06f * R * R + 30.35f * R + 94.85f, 70.0f, 100.0f);
            float pi   = constrain(acIR / dcIR * 100.0f, 0.0f, 20.0f);

            if (LOCK_VITALS()) {
                v->spo2 = spo2; v->spo2_valid = true; v->pi = pi;
                UNLOCK_VITALS();
            }
        }

        //waveform pentru websocket — ac-coupled si auto-scaled
        //scoatem componenta dc -> ramane doar pulsul (ac)
        //auto-scalare la 25000 peak -> orice amplitudine arata bine in grafic
        dcTracker = dcTracker * 0.98f + (float)ir * 0.02f;
        float ac  = (float)ir - dcTracker;
        acPeak    = acPeak * 0.995f + fabsf(ac) * 0.005f;
        float scale = (acPeak > 100.0f) ? (25000.0f / acPeak) : 100.0f;
        uint16_t wv = (uint16_t)constrain(ac * scale + 32768.0f, 0.0f, 65535.0f);

        if (LOCK_VITALS()) {
            v->spo2_wave[v->spo2_batch_idx++] = wv;
            if (v->spo2_batch_idx >= VitalData::SPO2_BATCH) {
                v->spo2_batch_idx   = 0;
                v->spo2_batch_ready = true;
            }
            UNLOCK_VITALS();
        }
    }

private:
    MAX30105 sensor;
    bool     initOk = false;

    uint32_t irBuf[SPO2_WIN]  = {};
    uint32_t redBuf[SPO2_WIN] = {};
    uint32_t irIdx  = 0;
    uint32_t redIdx = 0;

    float    dcTracker = 0.0f;
    float    acPeak    = 1.0f;

    //state pentru detectia de respiratie
    float    respFast = 0.0f;
    float    respSlow = 0.0f;
    float    respPrev = 0.0f;
    uint32_t respLastPeak = 0;
    uint16_t respIntervals[5] = {};
    uint8_t  respIdx   = 0;
    uint8_t  respCount = 0;
};
