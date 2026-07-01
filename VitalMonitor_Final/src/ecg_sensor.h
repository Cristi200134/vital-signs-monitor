#pragma once
//ecg: ad8232 amplificator analog + ads1115 adc i2c (0x48), 250 sps
//fix definitiv i2c: ads1115 in mod continuu
//one-shot ;250sps tinea bus-ul i2c blocat 4ms/4ms spo2 nu mai citea 
//modul continuu citeste registrul rezultat in aprox.0.5ms, fara asteptare
//i2c liber 3.5ms din 4ms → spo2, rtc, oled au timp suficient
//baterie (canal a1): citita o data pe minut, dupa care repornim modul continuu pe a0
//date din config
//detectie r-peak: algoritm pan-tompkins  standard)
//derivata -> patrat -> integrare 150ms -> prag adaptiv spki/npki
//rata fals-pozitiva sub 1%  in zgomot muscular sau artefacte emg

#include <Adafruit_ADS1X15.h>
#include "vital_data.h"
#include "config.h"
#include "ecg_filter.h"

class EcgSensor {
public:
    bool init() {
        if (!LOCK_I2C()) return false;
        bool ok = ads.begin(0x48);
        UNLOCK_I2C();

        if (!ok) {
            Serial.println("[ecg] ads1115 negasit (i2c 0x48)!");
            return initOk = false;
        }
        ads.setGain(ADS_GAIN);
        ads.setDataRate(RATE_ADS1115_250SPS);
        initOk = true;
        Serial.println("[ecg] ads1115 ok — 250 sps continuu");

        //prima citire de baterie + pornim modul continuu pe a0
        citesteBaterie(nullptr, nullptr);
        return true;
    }

    //apelat din taskecg la fiecare 4ms mereu ( indiferent de starea monitoring)
    void citesteSi(VitalData* v, SemaphoreHandle_t mtx, bool monitoring) {
        if (!initOk) return;

        //baterie: o data pe minut (15000 esantioane × 4ms = 60s)
        if (++batCount >= 15000) { batCount = 0; citesteBaterie(v, mtx); }

        //lead-off: citim direct din gpio, fara i2c
        bool lo = (digitalRead(PIN_LO_PLUS) == HIGH ||
                   digitalRead(PIN_LO_MINUS) == HIGH);
        if (lo) {
            filt.reset();
            if (LOCK_VITALS()) {
                v->lead_off      = true;
                v->ecg_batch_idx = 0;
                UNLOCK_VITALS();
            }
            return;
        }

        if (!monitoring) {
            if (LOCK_VITALS()) { v->lead_off = false; UNLOCK_VITALS(); }
            return;
        }

        //citim rezultatul din registru, modul continuu, ~0.5ms ads1115
        //(vechi: readadc_singleended() astepta 4ms -> i2c blocat -> spo2 moare)
        if (!LOCK_I2C()) return;
        int16_t raw = ads.getLastConversionResults();
        UNLOCK_I2C();

        float sig = filt.process(raw);
        detecteazaVarfR(sig, v);

        if (LOCK_VITALS()) {
            v->lead_off = false;
            v->ecg_batch[v->ecg_batch_idx] = (int16_t)constrain(sig, -32768, 32767);
            v->ecg_batch_idx++;
            if (v->ecg_batch_idx >= VitalData::ECG_BATCH) {
                v->ecg_batch_idx   = 0;
                v->ecg_batch_ready = true;
            }
            UNLOCK_VITALS();    // ECG_BATCH resetează la 0 ; marcheaza true si se repeta loop ul
        }
    }

private:
    Adafruit_ADS1115 ads;
    EcgFilter        filt;
    bool             initOk   = false;
    uint16_t         batCount = 14999; //primul citestesi -> trigger imediat la baterie

    //citire tensiune baterie pe canalul a1 (divizor 100k/100k)
    //one-shot pe a1, dupa care repornim modul continuu pe a0 (canal ecg)
    void citesteBaterie(VitalData* v, SemaphoreHandle_t mtx) {
     
        //mod continuous pe a1 ~20ms (5 conversii), citim ultima
        //(care e stabila) si revenim la continuous pe a0
        //ecg pierde 5 esantioane, e nesemnificativ
        if (!LOCK_I2C()) return;
        ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_1, /*continuous=*/true);
        UNLOCK_I2C();

        //5 conversii × 4ms (250 sps) = 20ms — primele 1-2 instabile, ultimele 3 stabile
        vTaskDelay(pdMS_TO_TICKS(20));

        if (!LOCK_I2C()) return;
        int16_t raw = ads.getLastConversionResults();              //ultima conversie a1
        ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0, /*continuous=*/true);
        UNLOCK_I2C();

        if (raw < 500 || raw > 28000) {
#if BAT_DEBUG
            Serial.printf("[bat] raw=%d invalid (out of [500,28000])\n", raw);
#endif
            return;
        }

        float volt = (float)raw * BAT_VOLT_FACTOR; // multimetru citire * val de ref,
        batBuf[batBufIdx] = volt;
        batBufIdx = (batBufIdx + 1) % BAT_AVG_N;
        if (batBufFull < BAT_AVG_N) batBufFull++;    //Ring buffer

        float avg = 0.0f;
        for (int i = 0; i < batBufFull; i++) avg += batBuf[i];
        avg /= batBufFull;

        //lipo soc lut — interpolare liniara intre puncte
        static const float LUT_V[] = {4.20f, 4.05f, 3.95f, 3.85f, 3.75f, 3.65f, 3.55f, 3.45f, 3.30f, 3.20f, 3.00f};
        static const float LUT_P[] = {100.f, 90.0f, 80.0f, 70.0f, 60.0f, 50.0f, 40.0f, 30.0f, 15.0f, 5.00f, 0.00f};
        static const int   LUT_N   = 11;

        float pct = 0.0f;
        if (avg >= LUT_V[0]) {
            pct = 100.0f;
        } else if (avg <= LUT_V[LUT_N - 1]) {
            pct = 0.0f;
        } else {
            for (int i = 0; i < LUT_N - 1; i++) {
                if (avg <= LUT_V[i] && avg >= LUT_V[i + 1]) {
                    pct = LUT_P[i + 1] + (avg - LUT_V[i + 1]) /
                          (LUT_V[i] - LUT_V[i + 1]) * (LUT_P[i] - LUT_P[i + 1]);
                    break;
                }
            }
        }

        uint8_t pctU = (uint8_t)constrain(pct, 0.0f, 100.0f);
#if BAT_DEBUG
        Serial.printf("[bat] raw=%d v=%.2f avg=%.2f pct=%d%%\n", raw, volt, avg, pctU);
#endif

        if (v && LOCK_VITALS()) {
            v->bat_pct = pctU;
            UNLOCK_VITALS();
        }
    }

    //pan-tompkins qrs detector (standard clinic, 1985)
    //
    //pipeline in 5 etape pe semnal deja bandpass (hp 0.5 hz + lp 40 hz):
    //  1. derivata 5-puncte    — scoate panta qrs in evidenta, atenueaza p si t
    //  2. ridicare la patrat   — totul pozitiv, contrast mai mare
    //  3. integrare fereastra  — neteda intr-un singur hump per bataie (150ms)
    //  4. maxim local          — varful din semnalul integrat = candidat qrs
    //  5. prag adaptiv         — invata separat spki (signal) si npki (zgomot)
    //
    //pragul: t_i1 = npki + 0.25 * (spki - npki)
    //  peak > t_i1 → r-peak (signal)
    //  altfel       → zgomot
    //spki = 0.125 * peak + 0.875 * spki (ema cu memorie lunga)
    //npki = la fel pentru zgomot
    //
    //rezistent la zgomot muscular, miscare, artefacte emg
    //rata fals-pozitiva tipic <1% vs ~5-10% la detectorul simplu de amplitudine
    void detecteazaVarfR(float sig, VitalData* v) {
        uint32_t tNow = millis();

        //1. derivata 5-puncte: y[n] = (2x[n] + x[n-1] - x[n-3] - 2x[n-4])/8
        float deriv = (2.0f * sig + ptX1 - ptX3 - 2.0f * ptX4) * 0.125f;
        ptX4 = ptX3; ptX3 = ptX2; ptX2 = ptX1; ptX1 = sig;

        //2. ridicare la patrat
        float sq = deriv * deriv;

        //3. integrare in fereastra mobila (37 esantioane = 148ms @ 250 sps)
        mwiSum -= mwiBuf[mwiIdx];
        mwiBuf[mwiIdx] = sq;
        mwiSum += sq;
        mwiIdx = (mwiIdx + 1) % MWI_N;
        float mwi = mwiSum / (float)MWI_N;

        //4. maxim local pe semnalul integrat
        bool isPeak = false;
        float peakValue = mwiPrev;
        if (mwiPrev > mwi && rising) {
            isPeak = true;           //varful era la mwiprev
            rising = false;
        } else if (mwi > mwiPrev) {
            rising = true;
        }
        mwiPrev = mwi;

        if (!isPeak) {
            //timeout 3s fara r-peak → invalid
            if ((tNow - tUltimR) > 3000UL && rrCount > 0) {
                if (LOCK_VITALS()) { v->hr_valid = false; UNLOCK_VITALS(); }
                rrCount = 0;
            }
            return;
        }

        //5. prag adaptiv spki/npki
        //refractar fiziologic 200ms (max ~300 bpm teoretic — orice peste e artefact)
        if (peakValue > threshI1 && (tNow - tUltimR) > 200) {
            //r-peak confirmat (signal)
            SPKI = 0.125f * peakValue + 0.875f * SPKI;

            uint32_t rr = tNow - tUltimR;
            tUltimR = tNow;

            if (rr > 250 && rr < 2000) {   //range valid 30-240 bpm
                rrBuf[rrIdx] = rr;
                rrIdx = (rrIdx + 1) % RR_N;
                if (rrCount < RR_N) rrCount++;

                if (rrCount >= 2) {
                    uint32_t sum = 0;
                    for (uint8_t i = 0; i < rrCount; i++) sum += rrBuf[i];
                    float bpm = 60000.0f / ((float)sum / rrCount);
                    if (bpm >= 30.0f && bpm <= 220.0f) {
                        if (LOCK_VITALS()) {
                            v->bpm      = bpm;
                            v->hr_valid = true;
                            UNLOCK_VITALS();
                        }
                    }
                }
            }
        } else {
            //zgomot — invata npki
            NPKI = 0.125f * peakValue + 0.875f * NPKI;
        }

        //recalculeaza pragul adaptiv
        threshI1 = NPKI + 0.25f * (SPKI - NPKI);
    }

    //state pan-tompkins
    //150ms @ ecg_sps — fereastra qrs tipica, autoadaptata la sample rate
    static constexpr int MWI_N = (ECG_SPS * 150) / 1000;
    static const int RR_N  = 16;     //memorie pentru media intervalelor r-r

    //linie de intarziere pentru derivata
    float ptX1 = 0, ptX2 = 0, ptX3 = 0, ptX4 = 0;

    //integrare fereastra mobila
    float mwiBuf[MWI_N] = {};
    int   mwiIdx        = 0;
    float mwiSum        = 0.0f;
    float mwiPrev       = 0.0f;
    bool  rising        = false;

    //praguri adaptive
    float SPKI     = 0.0f;
    float NPKI     = 0.0f;
    float threshI1 = 0.0f;

    //memorie r-r pentru bpm
    uint32_t tUltimR     = 0;
    uint32_t rrBuf[RR_N] = {};
    uint8_t  rrIdx       = 0;
    uint8_t  rrCount     = 0;

    //medie mobila pentru baterie
    static const int BAT_AVG_N = 8;
    float   batBuf[BAT_AVG_N] = {};
    uint8_t batBufIdx  = 0;
    uint8_t batBufFull = 0;
};
