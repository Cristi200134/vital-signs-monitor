#pragma once
//evaluare alarme clinice + control buzzer
//pragurile sunt in config.h
//niveluri: 0 = nimic, 1 = info, 2 = atentionare, 3 = critic
//alarm result separa sursa alarmei in:
//physio: hr, spo2, temp, resp (declanseaza auto-exit din overnight)
//system: lead-off, baterie    (nu declanseaza exit, sunt conditii sistem)
//prioritati (cea mai grava castiga):
//spo2 < 90%    critic (physio)
//hr > 130 / <45  critic (physio)
//spo2 < 94%      atentionare (physio)
//hr > 110 / <55  atentionare (physio)
//temp > 38.5 / <35  atentionare (physio)
//resp > 25 / <10  atentionare (physio)
//electrozi off  atentionare (system)
//baterie < 5%    critic (system)
//baterie < 15%  info (system)

#include <Arduino.h>
#include "vital_data.h"
#include "config.h"

struct AlarmResult {
    uint8_t level    = 0;     //max(physio, system)
    bool    isPhysio = false; //true daca sursa e o valoare vitala masurata
    char    msg[48]  = "";    //mesajul celei mai grave alarme
};

class AlarmManager {
public:
    void init() {
        pinMode(PIN_BUZZER, OUTPUT);
        noTone(PIN_BUZZER);
        Serial.println("[alarm] manager alarme initializat");
    }

    //eval. separat alarmele fiziologice si de sistem
    AlarmResult evalueaza(const VitalData& v) {
        AlarmResult resPhysio = evaluatePhysio(v);
        AlarmResult resSystem = evaluateSystem(v);

        AlarmResult out;
        if (resPhysio.level >= resSystem.level) {
            out.level    = resPhysio.level;
            out.isPhysio = (resPhysio.level > 0);
            strncpy(out.msg, resPhysio.msg, sizeof(out.msg) - 1);
        } else {
            out.level    = resSystem.level;
            out.isPhysio = false;
            strncpy(out.msg, resSystem.msg, sizeof(out.msg) - 1);
        }

        //state intern pentru buzzer
        nivelActiv = out.level;
        sunaActiv  = !(muteUntil > 0 && millis() < muteUntil);

        return out;
    }

    //suna buzzer-ul corespunzator nivelului
    void sunaBuzzer(uint8_t nivel) {
        if (!sunaActiv) { noTone(PIN_BUZZER); return; }

        uint32_t tNow = millis();
        switch (nivel) {
            case 0:
                noTone(PIN_BUZZER);
                break;
            case 1: //info — un bip scurt la fiecare 5s
                if (tNow - tUltimBip > 5000) {
                    tUltimBip = tNow;
                    tone(PIN_BUZZER, 880, 100);
                }
                break;
            case 2: //atentionare — doua bipuri la fiecare 3s
                if (tNow - tUltimBip > 3000) {
                    tUltimBip = tNow;
                    tone(PIN_BUZZER, 1200, 150);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    tone(PIN_BUZZER, 1200, 150);
                }
                break;
            case 3: //critic — bip continuu 800ms on / 200ms off
                {
                    bool on = (tNow % 1000) < 800;
                    if (on)  tone(PIN_BUZZER, 1800);
                    else     noTone(PIN_BUZZER);
                }
                break;
            default:
                noTone(PIN_BUZZER);
                break;
        }
    }

    //mute temporar (ms)
    void mutePentru(uint32_t ms) {
        muteUntil = millis() + ms;
        noTone(PIN_BUZZER);
        Serial.printf("[alarm] mute %lu ms\n", ms);
    }

    void dezMute() {
        muteUntil = 0;
        Serial.println("[alarm] mute dezactivat");
    }

    bool esteMutat() const { return (muteUntil > 0 && millis() < muteUntil); }

private:
    uint8_t  nivelActiv = 0;
    bool     sunaActiv  = true;
    uint32_t muteUntil  = 0;
    uint32_t tUltimBip  = 0;

    //alarme fiziologice (hr, spo2, temp, resp)
    AlarmResult evaluatePhysio(const VitalData& v) {
        AlarmResult r;

        if (v.spo2_valid) {
            if (v.spo2 < ALARM_SPO2_CRIT) {
                if (r.level < 3) { r.level = 3; strcpy(r.msg, "SpO2 critic"); }
            } else if (v.spo2 < ALARM_SPO2_WARN) {
                if (r.level < 2) { r.level = 2; strcpy(r.msg, "SpO2 scazut"); }
            }
        }
        if (v.hr_valid) {
            if (v.bpm > ALARM_HR_HIGH_CRIT) {
                if (r.level < 3) { r.level = 3; strcpy(r.msg, "Tahicardie critica"); }
            } else if (v.bpm < ALARM_HR_LOW_CRIT) {
                if (r.level < 3) { r.level = 3; strcpy(r.msg, "Bradicardie critica"); }
            } else if (v.bpm > ALARM_HR_HIGH_WARN) {
                if (r.level < 2) { r.level = 2; strcpy(r.msg, "Tahicardie"); }
            } else if (v.bpm < ALARM_HR_LOW_WARN) {
                if (r.level < 2) { r.level = 2; strcpy(r.msg, "Bradicardie"); }
            }
        }
        if (v.temp_valid) {
            if (v.temp_c > ALARM_TEMP_HIGH) {
                if (r.level < 2) { r.level = 2; strcpy(r.msg, "Febra"); }
            } else if (v.temp_c < ALARM_TEMP_LOW) {
                if (r.level < 2) { r.level = 2; strcpy(r.msg, "Hipotermie"); }
            }
        }
        if (v.resp_valid) {
            if (v.resp_rpm > ALARM_RESP_HIGH) {
                if (r.level < 2) { r.level = 2; strcpy(r.msg, "Tahipnee"); }
            } else if (v.resp_rpm < ALARM_RESP_LOW) {
                if (r.level < 2) { r.level = 2; strcpy(r.msg, "Bradipnee"); }
            }
        }
        return r;
    }

    //alarme sistem (lead-off, baterie)
    AlarmResult evaluateSystem(const VitalData& v) {
        AlarmResult r;

        if (v.lead_off) {
            r.level = 2; strcpy(r.msg, "Electrozi deconectati");
        }
        if (v.bat_pct < ALARM_BAT_CRIT && v.bat_pct > 0) {
            if (r.level < 3) { r.level = 3; strcpy(r.msg, "Baterie critica"); }
        } else if (v.bat_pct < ALARM_BAT_LOW && v.bat_pct > 0) {
            if (r.level < 1) { r.level = 1; strcpy(r.msg, "Baterie slaba"); }
        }
        return r;
    }
};
