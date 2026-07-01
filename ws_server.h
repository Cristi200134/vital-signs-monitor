#pragma once
//ws_server — trimite mesaje pe websocket spre interfata
//doua tipuri de mesaje:
// "wave" la 100ms — batch 25 ecg + 5 spo2 (waveform)
//"num"  la 2s    — bpm, spo2, temp, resp, baterie, alarma
//optimizare: jsondocument-urile sunt membri reutilizati, cu .clear() intre apeluri
//asa evitam fragmentarea heap-ului pe sesiuni lungi
//cleanupclients() se apeleaza din taskwebsocket (core 0), nu din loop() (core 1) altfel apare race condition pe lista clientilor

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "vital_data.h"

class WsServer {
public:
    //batch ecg + spo2 (100ms)
    void trimiteBatch(VitalData* v, SemaphoreHandle_t mtx, AsyncWebSocket* ws) {
        if (ws->count() == 0) return;

        bool ecgOk = false, spo2Ok = false;
        int16_t  ecgBuf[VitalData::ECG_BATCH]  = {};
        uint16_t spo2Buf[VitalData::SPO2_BATCH] = {};

        if (xSemaphoreTake(mtx, pdMS_TO_TICKS(5)) == pdTRUE) {
            ecgOk  = v->ecg_batch_ready;
            spo2Ok = v->spo2_batch_ready;
            if (ecgOk)  { memcpy(ecgBuf,  v->ecg_batch, sizeof(ecgBuf));  v->ecg_batch_ready  = false; }
            if (spo2Ok) { memcpy(spo2Buf, v->spo2_wave,  sizeof(spo2Buf)); v->spo2_batch_ready = false; }
            xSemaphoreGive(mtx);
        }

        if (!ecgOk && !spo2Ok) return;

        docBatch.clear();
        docBatch["tip"] = "wave";
        if (ecgOk) {
            JsonArray a = docBatch["ecg"].to<JsonArray>();
            for (int i = 0; i < VitalData::ECG_BATCH; i++) a.add(ecgBuf[i]);
        }
        if (spo2Ok) {
            JsonArray a = docBatch["spo2_val"].to<JsonArray>();
            for (int i = 0; i < VitalData::SPO2_BATCH; i++) a.add(spo2Buf[i]);
        }

        char buf[512];
        size_t len = serializeJson(docBatch, buf, sizeof(buf));
        ws->textAll(buf, len);
    }

    //date numerice (2s)
    void trimiteNumeric(VitalData* v, SemaphoreHandle_t mtx, AsyncWebSocket* ws) {
        if (ws->count() == 0) return;
        VitalData snap;
        if (xSemaphoreTake(mtx, pdMS_TO_TICKS(10)) != pdTRUE) return;
        snap = *v;
        xSemaphoreGive(mtx);
        trimiteSnapshot(snap, ws);
    }

    //trimite la un singur client nou conectat
    void trimiteNumericLaClient(VitalData* v, SemaphoreHandle_t mtx,
                                AsyncWebSocketClient* client)
    {
        VitalData snap;
        if (xSemaphoreTake(mtx, pdMS_TO_TICKS(10)) != pdTRUE) return;
        snap = *v;
        xSemaphoreGive(mtx);

        docNum.clear();
        construiesteNumeric(docNum, snap);
        char buf[768];
        size_t len = serializeJson(docNum, buf, sizeof(buf));
        client->text(buf, len);
    }

    //notificare comanda buton fizic
    void trimiteCmdButon(AsyncWebSocket* ws, const char* cmd) {
        if (ws->count() == 0) return;
        docBtn.clear();
        docBtn["tip"] = "btn";
        docBtn["cmd"] = cmd;
        char buf[64];
        size_t len = serializeJson(docBtn, buf, sizeof(buf));
        ws->textAll(buf, len);
    }

private:
    //jsondocument-uri reutilizate ( reutilizam .clear())
    JsonDocument docBatch;
    JsonDocument docNum;
    JsonDocument docBtn;

    void construiesteNumeric(JsonDocument& doc, const VitalData& v) {
        doc["tip"]        = "num";
        doc["bpm"]        = v.hr_valid   ? roundf(v.bpm)  : 0.0f;
        doc["hr_valid"]   = v.hr_valid;
        doc["spo2"]       = v.spo2_valid ? roundf(v.spo2) : 0.0f;
        doc["spo2_valid"] = v.spo2_valid;
        doc["pi"]         = v.pi;
        doc["temp"]       = v.temp_valid ? v.temp_c   : 0.0f;
        doc["temp_valid"] = v.temp_valid;
        doc["resp"]       = v.resp_valid ? v.resp_rpm : 0.0f;
        doc["resp_valid"] = v.resp_valid;
        doc["baterie"]    = v.bat_pct;
        doc["ws_clients"] = v.ws_clients;                  //numar real de clienti conectati
        doc["rssi"]       = v.ws_clients > 0 ? -45 : 0;    // compatib. cu versiunea veche
        doc["elec_lipsa"] = v.lead_off;
        doc["monitoring"] = v.monitoring_active;
        doc["overnight"]  = v.overnight_active;
        doc["probe_count"]= v.probe_count;
        doc["uptime_s"]   = v.uptime_s;                  //durata sesiunii curente (secunde)
        doc["datetime"]   = v.datetime_str;           //"yyyy-mm-dd hh:mm:ss"
        doc["sd_gb"]      = v.sd_free_gb;
        doc["sd_ok"]      = v.sd_ok;
        doc["sd_full"]    = v.sd_almost_full;          //true cand < sd_min_free_mb liber

        JsonObject alm  = doc["alarma"].to<JsonObject>();
        alm["nivel"]    = v.alarm_level;
        alm["text"]     = v.alarm_msg;

        if (v.patient_name[0] != '\0') {
            JsonObject pac = doc["pacient"].to<JsonObject>();
            pac["nume"]    = v.patient_name;
            pac["id"]      = v.patient_id;
            if (v.patient_diag[0] != '\0') pac["diag"] = v.patient_diag;
        }
    }

    void trimiteSnapshot(const VitalData& snap, AsyncWebSocket* ws) {
        docNum.clear();
        construiesteNumeric(docNum, snap);
        char buf[768];
        size_t len = serializeJson(docNum, buf, sizeof(buf));
        ws->textAll(buf, len);
    }
};
