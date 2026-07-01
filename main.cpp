//main.cpp — vitalmonitor (orchestrator freertos)
//workflow 
//  standby - [btn_rec sau "sesiune_noua" prin ws] - monitoring - [stop]
//arhitectura dual-core:
//  core 1: ecg(5) > spo2(4) > alarme(3) > temp(2) > oled(2) > btn(1)
//  core 0: websocket(3) > sd(2)
//mutex-uri:
//  g_mutex— acces campuri vitaldata (max 10ms wait)
//  g_i2c_mutex— serializeaza bus-ul i2c intre ads1115, max30102, rtc, oled

#define VITAL_DATA_IMPL
#include "vital_data.h"
#include "config.h"

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <SD.h>
#include "esp_wifi.h"

#include "ecg_filter.h"
#include "ecg_sensor.h"
#include "spo2_sensor.h"
#include "temp_sensor.h"
#include "rtc_sensor.h"
#include "sd_logger.h"
#include "oled_display.h"
#include "ws_server.h"
#include "led_control.h"
#include "alarm_manager.h"

//obiecte globale
AsyncWebServer  g_server(WEB_PORT);
AsyncWebSocket  g_ws(WS_PATH);

EcgSensor    g_ecg;
Spo2Sensor   g_spo2;
TempSensor   g_temp;
RtcSensor    g_rtc;
SdLogger     g_sd;
OledDisplay  g_oled;
WsServer     g_wsrv;
LedControl   g_led;
AlarmManager g_alarm;

//task ecg — core 1, prioritate 5, 250 sps
//vtaskdelayuntil - 4ms intre esantioane
//i2c-ul protejat de g_i2c_mutex in ecgsensor::citestesi()
void taskEcg(void* pv) {
    g_ecg.init();
    TickType_t tWake = xTaskGetTickCount();

    while (true) {
        bool activ = false;
        if (LOCK_VITALS()) { activ = g_vitals.monitoring_active; UNLOCK_VITALS(); }
        g_ecg.citesteSi(&g_vitals, g_mutex, activ);
        vTaskDelayUntil(&tWake, pdMS_TO_TICKS(4));
    }
}

//task spo2 — core 1, prioritate 4, 50 sps
// g_i2c_mutex in spo2sensor::citestesi()
//cu mutex, spo2 asteapta pana ecg elibereaza bus-ul (4ms max)
void taskSpo2(void* pv) {
    g_spo2.init();
    TickType_t tWake = xTaskGetTickCount();

    while (true) {
        bool activ = false;
        if (LOCK_VITALS()) { activ = g_vitals.monitoring_active; UNLOCK_VITALS(); }
        if (activ) g_spo2.citesteSi(&g_vitals, g_mutex);
        vTaskDelayUntil(&tWake, pdMS_TO_TICKS(20));
    }
}

//task temperatura + rtc — core 1, prioritate 2, la 2s
//ds18b20 - onewire, fara i2c, fara mutex
//rtc - i2c, protejat in rtcsensor::actualizeazaora()
void taskTemp(void* pv) {
    g_temp.init();

    while (true) {
        //rtc-ul ruleaza mereu (avem nevoie pentru ceas si timestamp csv)
        g_rtc.actualizeazaOra(&g_vitals, g_mutex);

        bool activ = false;
        if (LOCK_VITALS()) { activ = g_vitals.monitoring_active; UNLOCK_VITALS(); }
        if (activ) g_temp.citesteSi(&g_vitals, g_mutex);

        vTaskDelay(pdMS_TO_TICKS(TEMP_INTERVAL_MS));
    }
}

//task oled — core 1, prioritate 2, refresh la 500ms
//disp.display() (i2c) e protejat in oleddisplay::afiseaza()
//in overnight: ecran afiseaza  "overnight" + ora curenta
void taskOled(void* pv) {
    g_oled.init();

    while (true) {
        VitalData snap = snapVitals();

        if (snap.overnight_active) g_oled.afiseazaOvernight(snap);
        else                       g_oled.afiseaza(snap);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//task websocket — core 0, prioritate 3
//"wave" la 100ms — batch ecg + spo2
//"num"  la 2s    — date numerice complete

void taskWebSocket(void* pv) {
    vTaskDelay(pdMS_TO_TICKS(2000));    //asteptam initializarea wifi

    TickType_t tWave    = xTaskGetTickCount();
    TickType_t tNumeric = xTaskGetTickCount();
    TickType_t tCleanup = xTaskGetTickCount();

    while (true) {
        TickType_t tNow = xTaskGetTickCount();

        if ((tNow - tWave) >= pdMS_TO_TICKS(WAVE_TX_MS)) {
            tWave = tNow;
            g_wsrv.trimiteBatch(&g_vitals, g_mutex, &g_ws);
        }

        if ((tNow - tNumeric) >= pdMS_TO_TICKS(NUMERIC_TX_MS)) {
            tNumeric = tNow;
            if (LOCK_VITALS()) {
                g_vitals.ws_clients = (uint8_t)g_ws.count();
                UNLOCK_VITALS();
            }
            g_wsrv.trimiteNumeric(&g_vitals, g_mutex, &g_ws);
        }

        //cleanup la 5s pe core 0 (acelasi cu ws-ul) — evita race condition
        if ((tNow - tCleanup) >= pdMS_TO_TICKS(5000)) {
            tCleanup = tNow;
            g_ws.cleanupClients();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

//task sd logger — core 0, prioritate 2, la 1s
//tranzitie monitoring false->true: deschide fisier nou
//tranzitie monitoring true->false: inchide fisierul
//incrementeaza probe_count si uptime_s la fiecare rand scris
void taskSd(void* pv) {
    bool sdOk = g_sd.init();
    if (LOCK_VITALS()) {
        g_vitals.sd_ok      = sdOk;
        g_vitals.sd_free_gb = sdOk ? g_sd.spatioLiber() : 0.0f;
        UNLOCK_VITALS();
    }

    bool wasActiv = false;

    while (true) {
        VitalData snap = snapVitals();
        bool activ = snap.monitoring_active;

        if (activ && !wasActiv)   g_sd.deschideSesiune(snap);
        else if (!activ && wasActiv) g_sd.inchide();

        if (activ) {
            g_sd.scrieRand(snap);
            if (LOCK_VITALS()) {
                g_vitals.probe_count++;
                g_vitals.uptime_s++;
                g_vitals.sd_free_gb    = g_sd.spatioLiber();
                g_vitals.sd_almost_full = g_sd.aproapePlin();
                UNLOCK_VITALS();
            }
        }

        wasActiv = activ;
        vTaskDelay(pdMS_TO_TICKS(SD_LOG_INTERVAL_MS));
    }
}

//task alarme + led + buzzer — core 1, prioritate 3, la 500ms
//in standby: led albastru clipeste lent, buzzer-ul oprit
//in monitoring: led-ul si buzzer-ul urmaresc nivelul de alarma
void taskAlarme(void* pv) {
    g_alarm.init();
    g_led.init();

    while (true) {
        VitalData snap = snapVitals();

        if (snap.monitoring_active) {
            AlarmResult res = g_alarm.evalueaza(snap);

            //auto-exit din overnight, doar pentru alarmele physio (nu lead-off/baterie)
            if (snap.overnight_active && res.isPhysio && res.level >= 2) {
                if (LOCK_VITALS()) { g_vitals.overnight_active = false; UNLOCK_VITALS(); }
                snap.overnight_active = false;
                Serial.printf("[overnight] auto-exit: %s (nivel %d)\n", res.msg, res.level);
            }

            //auto-mark csv: pe tranzitia la nivel 3 (critic), marcheaza evenimentul
            //asa coloana "ev" din csv arata "DA" la momentul aparitiei alarmei critice
            static uint8_t prevAlarmLevel = 0;
            if (res.level >= 3 && prevAlarmLevel < 3) {
                g_sd.marcheazaEveniment();
                Serial.printf("[alarm-ev] critic auto-marcat in csv: %s\n", res.msg);
            }
            prevAlarmLevel = res.level;

            if (LOCK_VITALS()) {
                g_vitals.alarm_level = res.level;
                strncpy(g_vitals.alarm_msg, res.msg, sizeof(g_vitals.alarm_msg) - 1);
                UNLOCK_VITALS();
            }

            if (snap.overnight_active) {
                //in overnight: led galben fulger scurt la 10s, buzzer mut
                bool on = (millis() % 10000) < 200;
                digitalWrite(PIN_LED_R, on ? HIGH : LOW);
                digitalWrite(PIN_LED_G, on ? HIGH : LOW);
                digitalWrite(PIN_LED_B, LOW);
                noTone(PIN_BUZZER);
            } else {
                g_led.aplicaAlarma(res.level, snap.bat_pct);
                g_alarm.sunaBuzzer(res.level);
            }
        } else {
            //standby (eventual + overnight): albastru clipeste / overnight = stins
            if (snap.overnight_active) {
                digitalWrite(PIN_LED_R, LOW);
                digitalWrite(PIN_LED_G, LOW);
                digitalWrite(PIN_LED_B, LOW);
            } else {
                bool on = (millis() / 1000) % 2;
                digitalWrite(PIN_LED_R, LOW);
                digitalWrite(PIN_LED_G, LOW);
                digitalWrite(PIN_LED_B, on ? HIGH : LOW);
            }
            noTone(PIN_BUZZER);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//task butoane — core 1, prioritate 1, polling la 50ms
//btn_men (gpio36 albastru — paginile oled
//btn_sil (gpio39 galben)  — silentiere buzzer 1 minut
//btn_sel (gpio14 negru)   — toggle overnight (single press)
//btn_rec (gpio13 rosu)   — start / stop monitorizare
//btn_ev  (gpio15 ttp223)  — marcheaza eveniment in csv
void taskButoane(void* pv) {
    bool prevMen = HIGH, prevSil = HIGH, prevSel = HIGH, prevRec = HIGH, prevEv = LOW;

    while (true) {
        bool curMen = digitalRead(PIN_BTN_MEN);
        bool curSil = digitalRead(PIN_BTN_SIL);
        bool curSel = digitalRead(PIN_BTN_SEL);
        bool curRec = digitalRead(PIN_BTN_REC);
        bool curEv  = digitalRead(PIN_BTN_EV);

        //btn_men — pagina urmatoare oled
        if (prevMen == HIGH && curMen == LOW) {
            g_oled.nextPagina();
        }

        //btn_sil — silentiere buzzer 1 minut 
        if (prevSil == HIGH && curSil == LOW) {
            g_alarm.mutePentru(BTN_SIL_MS);
            g_wsrv.trimiteCmdButon(&g_ws, "btn_sil");
            Serial.println("[btn_sil] buzzer silentiat 1 minut");
        }

        //btn_sel — toggle overnight 
        if (prevSel == HIGH && curSel == LOW) {
            bool nou = false;
            if (LOCK_VITALS()) {
                g_vitals.overnight_active = !g_vitals.overnight_active;
                nou = g_vitals.overnight_active;
                UNLOCK_VITALS();
            }
            g_wsrv.trimiteCmdButon(&g_ws, nou ? "overnight_on" : "overnight_off");
            Serial.printf("[btn_sel] overnight %s\n", nou ? "pornit" : "oprit");
        }

        //btn_rec — start / stop
        if (prevRec == HIGH && curRec == LOW) {
            if (LOCK_VITALS()) {
                g_vitals.monitoring_active = !g_vitals.monitoring_active;
                bool active = g_vitals.monitoring_active;

                if (active) {
                    g_vitals.probe_count        = 0;
                    g_vitals.uptime_s           = 0;
                    g_vitals.alarm_level        = 0;
                    g_vitals.alarm_msg[0]       = '\0';
                    g_vitals.session_start_unix = g_rtc.unixTime();
                } else {
                    //reset valori la stop — interfata  "—"
                    g_vitals.bpm        = 0.0f; g_vitals.hr_valid   = false;
                    g_vitals.spo2       = 0.0f; g_vitals.spo2_valid = false;
                    g_vitals.temp_c     = 0.0f; g_vitals.temp_valid = false;
                    g_vitals.resp_rpm   = 0.0f; g_vitals.resp_valid = false;
                    g_vitals.alarm_level = 0;   g_vitals.alarm_msg[0] = '\0';
                }
                UNLOCK_VITALS();

                g_wsrv.trimiteCmdButon(&g_ws, active ? "btn_start" : "btn_stop");
                Serial.printf("[btn_rec] monitorizare %s\n", active ? "pornita" : "oprita");
            }
        }

        //btn_ev — eveniment pacient (panic button)
        if (prevEv == LOW && curEv == HIGH) {
            g_sd.marcheazaEveniment();
            g_wsrv.trimiteCmdButon(&g_ws, "btn_event");
            //3 bipuri scurte de confirmare
            for (int i = 0; i < 3; i++) {
                tone(PIN_BUZZER, 1200, 150);
                vTaskDelay(pdMS_TO_TICKS(220));
            }
            noTone(PIN_BUZZER);
            Serial.println("[btn_ev] eveniment marcat");
        }

        prevMen = curMen; prevSil = curSil; prevSel = curSel;
        prevRec = curRec; prevEv  = curEv;
        vTaskDelay(pdMS_TO_TICKS(BTN_DEBOUNCE_MS / 4));
    }
}

//callback websocket — primeste comenzi json de la interfata
void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[ws] client #%u conectat (%s)\n",
                      client->id(), client->remoteIP().toString().c_str());
        if (LOCK_VITALS()) { g_vitals.ws_clients = (uint8_t)server->count(); UNLOCK_VITALS(); }
        g_wsrv.trimiteNumericLaClient(&g_vitals, g_mutex, client);
    }
    else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[ws] client #%u deconectat\n", client->id());
        if (LOCK_VITALS()) { g_vitals.ws_clients = (uint8_t)server->count(); UNLOCK_VITALS(); }
    }
    else if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (!info->final || info->index != 0 || info->len != len
            || info->opcode != WS_TEXT) return;

        JsonDocument doc;
        if (deserializeJson(doc, data, len)) return;
        const char* cmd = doc["cmd"] | "";

        //porneste monitorizarea cu date pacient
        if (strcmp(cmd, "sesiune_noua") == 0) {
            if (LOCK_VITALS()) {
                JsonObject pac = doc["pacient"];
                String numeFull = String(pac["prenume"] | "") + "_" +
                                  String(pac["nume"]    | "");
                strncpy(g_vitals.patient_name, numeFull.c_str(),
                        sizeof(g_vitals.patient_name) - 1);
                strncpy(g_vitals.patient_id,
                        pac["id"] | "", sizeof(g_vitals.patient_id) - 1);
                strncpy(g_vitals.patient_diag,
                        pac["diag"] | "", sizeof(g_vitals.patient_diag) - 1);

                g_vitals.monitoring_active  = true;
                g_vitals.probe_count        = 0;
                g_vitals.uptime_s           = 0;
                g_vitals.alarm_level        = 0;
                g_vitals.alarm_msg[0]       = '\0';
                g_vitals.session_start_unix = g_rtc.unixTime();
                UNLOCK_VITALS();
            }
            g_wsrv.trimiteCmdButon(&g_ws, "btn_start");
            Serial.println("[ws] sesiune noua pornita");
        }
        else if (strcmp(cmd, "stop") == 0) {
            if (LOCK_VITALS()) {
                g_vitals.monitoring_active = false;
                g_vitals.bpm = 0; g_vitals.hr_valid   = false;
                g_vitals.spo2 = 0; g_vitals.spo2_valid = false;
                g_vitals.temp_c = 0; g_vitals.temp_valid = false;
                UNLOCK_VITALS();
            }
            g_wsrv.trimiteCmdButon(&g_ws, "btn_stop");
        }
        else if (strcmp(cmd, "eveniment") == 0) {
            g_sd.marcheazaEveniment();
        }
        else if (strcmp(cmd, "silentiat") == 0) {
            bool val = doc["val"] | false;
            if (val) g_alarm.mutePentru(BTN_SIL_MS);
            else     g_alarm.dezMute();
        }
        else if (strcmp(cmd, "set_ora") == 0) {
            g_rtc.seteazaOra(
                doc["an"] | 2024, doc["luna"] | 1, doc["zi"] | 1,
                doc["h"]  | 0,    doc["min"]  | 0, doc["sec"]| 0);
        }
        else if (strcmp(cmd, "export") == 0) {
            String filename = g_sd.fisierCurent();
            JsonDocument resp;
            resp["tip"]      = "export_url";
            resp["url"]      = ("/download?file=" + filename).c_str();
            resp["filename"] = filename.c_str();
            char buf[256];
            size_t l = serializeJson(resp, buf, sizeof(buf));
            client->text(buf, l);
        }
    }
}

//initwifi, initlittlefs, initwebserver
static void initWiFi() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS,
                WIFI_AP_CHANNEL, WIFI_AP_HIDDEN, WIFI_AP_MAX_CONN);
    IPAddress ip(192, 168, 4, 1);
    WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
    Serial.printf("[wifi] ap: '%s'  ip: %s\n",
                  WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
}

static void initLittleFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("[fs] littlefs eroare!"); return;
    }
    Serial.println("[fs] littlefs ok");
}

static void initWebServer() {
    g_ws.onEvent(onWsEvent);
    g_server.addHandler(&g_ws);

    g_server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(LittleFS, "/index.html", "text/html");
    });
    g_server.on("/history", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(LittleFS, "/history.html", "text/html");
    });

    //lista fisierelor csv de pe sd
    g_server.on("/files", HTTP_GET, [](AsyncWebServerRequest* r) {
        String json = "[";
        if (g_sd.esteMontat()) {
            File dir = SD.open(SD_LOG_DIR);
            if (dir && dir.isDirectory()) {
                bool first = true;
                File f = dir.openNextFile();
                while (f) {
                    if (!f.isDirectory() && String(f.name()).endsWith(".csv")) {
                        if (!first) json += ",";
                        json += "\"" + String(f.name()) + "\"";
                        first = false;
                    }
                    f.close(); f = dir.openNextFile();
                }
                dir.close();
            }
        }
        json += "]";
        r->send(200, "application/json", json);
    });

    //download fisier csv
    g_server.on("/download", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (!r->hasParam("file")) { r->send(400, "text/plain", "param lipsa"); return; }
        String fn = r->getParam("file")->value();
        if (fn.indexOf("..") >= 0 || !fn.endsWith(".csv"))
            { r->send(400, "text/plain", "invalid"); return; }
        String path = String(SD_LOG_DIR) + "/" + fn;
        if (!SD.exists(path.c_str())) { r->send(404, "text/plain", "negasit"); return; }
        r->send(SD, path.c_str(), "text/csv", true);
    });

    g_server.onNotFound([](AsyncWebServerRequest* r) {
        r->send(404, "text/plain", "404");
    });

    g_server.begin();
    Serial.printf("[http] server pe portul %d\n", WEB_PORT);
}

//setup
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n[boot] %s v%s\n", DEVICE_NAME, FW_VERSION);

    //mutex-uri
    g_mutex     = xSemaphoreCreateMutex(); configASSERT(g_mutex);
    g_i2c_mutex = xSemaphoreCreateMutex(); configASSERT(g_i2c_mutex);

    //i2c + gpio
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);

    pinMode(PIN_BTN_REC, INPUT_PULLUP);
    pinMode(PIN_BTN_SEL, INPUT_PULLUP);
    pinMode(PIN_BTN_SIL, INPUT);
    pinMode(PIN_BTN_MEN, INPUT);
    pinMode(PIN_BTN_EV,  INPUT);
    pinMode(PIN_LO_PLUS,  INPUT);
    pinMode(PIN_LO_MINUS, INPUT);

    pinMode(PIN_LED_R, OUTPUT); digitalWrite(PIN_LED_R, LOW);
    pinMode(PIN_LED_G, OUTPUT); digitalWrite(PIN_LED_G, LOW);
    pinMode(PIN_LED_B, OUTPUT); digitalWrite(PIN_LED_B, LOW);
    pinMode(PIN_BUZZER, OUTPUT); noTone(PIN_BUZZER);

    //led albastru = boot in curs
    digitalWrite(PIN_LED_B, HIGH);

    //rtc inainte de sd (timestamp-ul e necesar pentru csv)
    g_rtc.init();
    g_rtc.actualizeazaOra(&g_vitals, g_mutex);

    //wifi + http + littlefs
    initLittleFS();
    initWiFi();
    initWebServer();

    digitalWrite(PIN_LED_B, LOW);

    //self-test: led rgb + 2 bipuri
    //rosu -> verde -> albastru -> stins
    for (int pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
        digitalWrite(pin, HIGH); delay(200); digitalWrite(pin, LOW);
    }
    tone(PIN_BUZZER, 1000, 80); delay(150);
    tone(PIN_BUZZER, 1400, 80); delay(150);
    noTone(PIN_BUZZER);

    //state initial: standby (led albastru clipeste — gestionat de taskalarme)
    Serial.println("[setup] stare: standby");
    Serial.printf("[setup] wifi: '%s'  →  http://192.168.4.1\n\n", WIFI_AP_SSID);

    //creare task-uri freertos
    xTaskCreatePinnedToCore(taskEcg,       "ECG",  STACK_ECG,       nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(taskSpo2,      "SPO2", STACK_SPO2,      nullptr, 4, nullptr, 1);
    xTaskCreatePinnedToCore(taskAlarme,    "ALM",  STACK_ALARMS,    nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(taskTemp,      "TEMP", STACK_TEMP,      nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskOled,      "OLED", STACK_OLED,      nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskButoane,   "BTN",  STACK_BUTTONS,   nullptr, 1, nullptr, 1);
    xTaskCreatePinnedToCore(taskWebSocket, "WS",   STACK_WEBSOCKET, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(taskSd,        "SD",   STACK_SDLOG,     nullptr, 2, nullptr, 0);

    Serial.println("[setup] task-uri freertos pornite");
}

//loop — freertos preia controlul, loop() nu mai face nimic util
//cleanupclients() mutat in taskwebsocket (core 0) pentru a evita race condition
void loop() {
    vTaskDelay(portMAX_DELAY);
}
