#pragma once
//structura de date partajata intre task-uri
//g_vitals, protejat de doua mutex-uri
// g_mutex - protejeaza campurile structurii (orice task)
//g_i2c_mutex - serializeaza accesul pe bus-ul i2c (ecg, spo2, rtc, oled)
//workflow.   standby -> start -> monitoring -> stop

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

//structura semne vitale
struct VitalData {

    //ecg (ad8232 + ads1115)
    float    bpm        = 0.0f;
    bool     hr_valid   = false;
    bool     lead_off   = false;

    //batch waveform. 250 sps × 100ms = 25 esantioane/transmisie
    static const int ECG_BATCH = 25;
    int16_t  ecg_batch[ECG_BATCH] = {};
    int      ecg_batch_idx   = 0;
    bool     ecg_batch_ready = false;

    //spo2 (max30102)
    float    spo2       = 0.0f;
    float    pi         = 0.0f; //perfusion index
    bool     spo2_valid = false;

    //50 sps × 100ms = 5 esantioane/transmisie
    static const int SPO2_BATCH = 5;
    uint16_t spo2_wave[SPO2_BATCH] = {};
    int      spo2_batch_idx   = 0;
    bool     spo2_batch_ready = false;

    //temperatura (ds18b20)
    float    temp_c     = 0.0f;
    bool     temp_valid = false;

    //respiratie (derivata din modulatia dc a ir-ului de la spo2)
    float    resp_rpm   = 0.0f;
    bool     resp_valid = false;

    //baterie
    uint8_t  bat_pct    = 0;        //0-100%

    //alarme
    uint8_t  alarm_level = 0;       //0=ok, 2=warn, 3=critic
    char     alarm_msg[48] = "";

    //sesiune
    bool     monitoring_active  = false;
    bool     overnight_active   = false;  //mod noapte: oled schimbat, buzzer mut, led minim
    uint32_t probe_count        = 0;    //contor csv
    uint32_t uptime_s           = 0;    //sec de la strt sesiune
    uint32_t session_start_unix = 0;

    //sistem
    bool     sd_ok         = false;
    bool     sd_almost_full = false;   //true cand spatiul liber < sd_min_free_mb
    float    sd_free_gb    = 0.0f;
    uint8_t  ws_clients    = 0;
    char     datetime_str[20] = "";    //"yyyy-mm-dd hh:mm:ss" + bitul 0

    //pacient
    char     patient_name[48]  = "";
    char     patient_id[16]    = "";
    char     patient_diag[128] = "";
};

//variabile globale
extern VitalData         g_vitals;
extern SemaphoreHandle_t g_mutex;       //acces campuri vitaldata
extern SemaphoreHandle_t g_i2c_mutex;   //acces bus i2c

//macros lock — logheaza timeout-ul, util pentru debug deadlock
//statement expressions: ({ ... ; result; }) returneaza ultima expresie
#define LOCK_VITALS()  ({                                                   \
    bool _ok = (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(10)) == pdTRUE);      \
    if (!_ok) Serial.println("[lock] timeout vitals (>10ms)");              \
    _ok;                                                                    \
})
#define UNLOCK_VITALS()  xSemaphoreGive(g_mutex)



#define LOCK_I2C()  ({                                                      \
    bool _ok = (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(20)) == pdTRUE);  \
    if (!_ok) Serial.println("[lock] timeout i2c (>20ms)");                 \
    _ok;                                                                    \
})
#define UNLOCK_I2C()  xSemaphoreGive(g_i2c_mutex)


//inlocuieste pattern-ul repetat: vitaldata s; if (LOCK_VITALS()) { s=g_vitals; UNLOCK_VITALS(); }
//returneaza o copie locala pentru read-only
static inline VitalData snapVitals() {
    VitalData snap;
    if (LOCK_VITALS()) { snap = g_vitals; UNLOCK_VITALS(); }
    return snap;
}

//definitii (o singura data, in main.cpp cu #define vital_data_impl)
#ifdef VITAL_DATA_IMPL
    VitalData         g_vitals;
    SemaphoreHandle_t g_mutex      = nullptr;
    SemaphoreHandle_t g_i2c_mutex  = nullptr;
#endif
