#pragma once
//oled ssd1306 128×64 pe i2c 0x3c
//layout-ul ecranului:
//y=0-9   header (rec / hh:mm:ss / bat% / wifi)
//y=10    separator orizontal
//y=12-58 continut (5 randuri × 9px = 45px)
//y=61-63 indicatori pagina (3 patratele 4×3)
//3 pagini, ciclate cu btn_men (gpio36):
//header pe toate paginile
//p1 vitale — bpm, spo2/pi, temp/resp, sesiune, alarma
//p2 sistem — wifi, sd, baterie, pacient, probe
//p3 stare  — fata animata centrata 

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include "vital_data.h"
#include "config.h"

#define HDR_H      10
#define SEP_Y      10
#define CONT_Y     12
#define ROW_H       9
#define NR_PAG      3

class OledDisplay {
public:
    OledDisplay() : disp(OLED_WIDTH, OLED_HEIGHT, &Wire, -1) {}

    bool init() {
        if (!LOCK_I2C()) return false;
        bool ok = disp.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
        if (ok) {
            disp.clearDisplay();
            disp.setTextColor(SSD1306_WHITE);
            disp.setTextSize(1);
            //init. centrat
            disp.setCursor(16, 22); disp.print("VitalMonitor");
            disp.setCursor(46, 36); disp.print("v" FW_VERSION);
            disp.display();
        }
        UNLOCK_I2C();

        if (!ok) { Serial.println("[oled] ssd1306 negasit!"); return initOk = false; }
        initOk = true;
        Serial.println("[oled] ssd1306 ok");
        delay(1000);
        return true;
    }

    void nextPagina() { pagina = (pagina + 1) % NR_PAG; }

    //ecranul de overnight: doar "overnight" + ora curenta
    void afiseazaOvernight(const VitalData& v) {
        if (!initOk) return;

        disp.clearDisplay();
        disp.setTextColor(SSD1306_WHITE);

        //titlu "overnight" centrat (size 2: 9 chars × 12 = 108 px - x=10)
        disp.setTextSize(2);
        disp.setCursor(10, 12);
        disp.print("OVERNIGHT");

        //ora curenta (size 2: 8 chars × 12 = 96 px -> x=16)
        char t[9] = "--:--:--";
        if (strlen(v.datetime_str) >= 19)
            snprintf(t, sizeof(t), "%.8s", v.datetime_str + 11);
        disp.setCursor(16, 38);
        disp.print(t);

        if (LOCK_I2C()) { disp.display(); UNLOCK_I2C(); }
    }

    //apelat din taskoled la fiecare 500ms
    void afiseaza(const VitalData& v) {
        if (!initOk) return;

        //hash skip — daca toate campurile relevante sunt aceleasi, sarim redraw-ul
        //si transmiterea pe i2c. economie aprox.60% i2c in standby
        uint32_t h = calculHashEcran(v);
        if (h == ultimHash) return;
        ultimHash = h;

        disp.clearDisplay();
        disp.setTextColor(SSD1306_WHITE);
        disp.setTextSize(1);

        drawHeader(v);

        switch (pagina) {
            case 0: drawP1Vitals(v); break;
            case 1: drawP2Sistem(v); break;
            case 2: drawP3Stare(v);  break;
        }

        //indicatori pagina (3 patratele jos-dreapta)
        for (int i = 0; i < NR_PAG; i++) {
            int bx = 128 - NR_PAG * 5 + i * 5;
            if (i == pagina) disp.fillRect(bx, 61, 4, 3, SSD1306_WHITE);
            else             disp.drawRect(bx, 61, 4, 3, SSD1306_WHITE);
        }

        if (LOCK_I2C()) { disp.display(); UNLOCK_I2C(); }
    }

private:
    Adafruit_SSD1306 disp;
    bool     initOk     = false;
    uint8_t  pagina     = 0;
    uint32_t ultimHash  = 0;

    //fnv-1a 32-bit hash pe campurile vizibile in oled 
    uint32_t calculHashEcran(const VitalData& v) {
        uint32_t h = 2166136261u;
        auto mix = [&h](const void* p, size_t n) {
            const uint8_t* b = (const uint8_t*)p;
            for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
        };
        mix(&v.bpm, sizeof(v.bpm));
        mix(&v.hr_valid, sizeof(v.hr_valid));
        mix(&v.spo2, sizeof(v.spo2));
        mix(&v.spo2_valid, sizeof(v.spo2_valid));
        mix(&v.pi, sizeof(v.pi));
        mix(&v.temp_c, sizeof(v.temp_c));
        mix(&v.temp_valid, sizeof(v.temp_valid));
        mix(&v.resp_rpm, sizeof(v.resp_rpm));
        mix(&v.resp_valid, sizeof(v.resp_valid));
        mix(&v.bat_pct, sizeof(v.bat_pct));
        mix(&v.alarm_level, sizeof(v.alarm_level));
        mix(v.alarm_msg, strlen(v.alarm_msg));
        mix(&v.monitoring_active, sizeof(v.monitoring_active));
        mix(&v.uptime_s, sizeof(v.uptime_s));
        mix(&v.ws_clients, sizeof(v.ws_clients));
        mix(&v.sd_ok, sizeof(v.sd_ok));
        mix(&v.sd_free_gb, sizeof(v.sd_free_gb));
        mix(&v.lead_off, sizeof(v.lead_off));
        mix(&v.probe_count, sizeof(v.probe_count));
        mix(v.patient_name, strlen(v.patient_name));
        mix(v.datetime_str, strlen(v.datetime_str));
        mix(&pagina, sizeof(pagina));
        return h;
    }

    //header (y=0-9): [rec] [hh:mm:ss] [85%] [wifi]
    void drawHeader(const VitalData& v) {
        disp.setTextSize(1);

        //stanga: rec / std
        disp.setCursor(0, 1);
        disp.print(v.monitoring_active ? "\x07REC" : " STD");

        //centru: ora
        char t[9] = "--:--:--";
        if (strlen(v.datetime_str) >= 19)
            snprintf(t, sizeof(t), "%.8s", v.datetime_str + 11);
        disp.setCursor(40, 1);
        disp.print(t);

        //dreapta: baterie + bare wifi (intre 116-126 - toate vizibile)
        char b[5]; snprintf(b, sizeof(b), "%2d%%", v.bat_pct);
        disp.setCursor(95, 1);
        disp.print(b);
        drawWifiBars(116, 8, v.ws_clients > 0);

        disp.drawFastHLine(0, SEP_Y, 128, SSD1306_WHITE);
    }

    void drawWifiBars(int x, int baseY, bool on) {
        for (int i = 0; i < 4; i++) {
            int h = 2 + i * 2, bx = x + i * 3, by = baseY - h;
            if (on || i == 0) disp.fillRect(bx, by, 2, h, SSD1306_WHITE);
            else              disp.drawRect(bx, by, 2, h, SSD1306_WHITE);
        }
    }

    //p1 vitale (5 randuri)

    //BPM:  72        LO:NU
    //SpO2: 97%   PI: 2.3
    //Temp: 36.8C  R:15rpm
    //Ses:  00:12:34
    //Alm:  OK
    void drawP1Vitals(const VitalData& v) {
        disp.setTextSize(1);
        int y = CONT_Y;

        //r1: bpm + lead-off
        disp.setCursor(0, y);
        if (v.hr_valid)   disp.printf("BPM:  %3.0f", v.bpm);
        else              disp.print("BPM:  ---");
        disp.setCursor(80, y);
        disp.print(v.lead_off ? "LO:DA" : "LO:NU");

        //r2: spo2 + pi
        y += ROW_H;
        disp.setCursor(0, y);
        if (v.spo2_valid) disp.printf("SpO2: %2.0f%%", v.spo2);
        else              disp.print("SpO2: ---");
        disp.setCursor(72, y);
        if (v.spo2_valid) disp.printf("PI:%.1f", v.pi);
        else              disp.print("PI:---");

        //r3: temp + resp pe acelasi rand
        y += ROW_H;
        disp.setCursor(0, y);
        if (v.temp_valid) disp.printf("T:%4.1fC", v.temp_c);
        else              disp.print("T:---  ");
        disp.setCursor(64, y);
        if (v.resp_valid) disp.printf("R:%2.0frpm", v.resp_rpm);
        else              disp.print("R:--rpm");

        //r4: sesiune
        y += ROW_H;
        disp.setCursor(0, y);
        if (v.monitoring_active) {
            uint32_t s = v.uptime_s;
            disp.printf("Ses:  %02lu:%02lu:%02lu",
                (unsigned long)(s / 3600),
                (unsigned long)((s % 3600) / 60),
                (unsigned long)(s % 60));
        } else {
            disp.print("Ses:  STANDBY");
        }

        //r5: alarma
        y += ROW_H;
        disp.setCursor(0, y);
        disp.print("Alm:  ");
        disp.print(v.alarm_level == 0 ? "OK" : v.alarm_msg);
    }

    //p2— sistem (5 randuri)
    //WiFi: 1 client
    //SD:   28.4 GB liber
    //Bat:  85%
    //Pac:  Ion_Popescu
    //Probe: 1234 salv.
    void drawP2Sistem(const VitalData& v) {
        disp.setTextSize(1);
        int y = CONT_Y;

        disp.setCursor(0, y);
        if (v.ws_clients > 0) disp.printf("WiFi: %d client(i)", v.ws_clients);
        else                  disp.print("WiFi: nec.");

        y += ROW_H;
        disp.setCursor(0, y);
        if (v.sd_ok) disp.printf("SD:   %.1f GB liber", v.sd_free_gb);
        else         disp.print("SD:   EROARE");

        y += ROW_H;
        disp.setCursor(0, y);
        disp.printf("Bat:  %d%%", v.bat_pct);

        y += ROW_H;
        disp.setCursor(0, y);
        if (v.patient_name[0] != '\0') {
            disp.printf("Pac:  %.15s", v.patient_name);
        } else {
            disp.print("Pac:  ---");
        }

        y += ROW_H;
        disp.setCursor(0, y);
        disp.printf("Probe:%lu salv.", (unsigned long)v.probe_count);
    }

    //p3 stare (fata centrata,
    //centru: cx=64 (orizontal), cy=35 (vertical pe zona de continut y=12-58)
    //raza: r=20
    //ochi: 2 cercuri pline cu raza 2 la cy-8, dx+-8
    //gura: curba de 2px in functie de alarm_level
    void drawP3Stare(const VitalData& v) {
        const int cx = 64;
        const int cy = 35;
        const int r  = 20;

        //contur fata (2px pentru efect mai placut)
        disp.drawCircle(cx, cy, r,     SSD1306_WHITE);
        disp.drawCircle(cx, cy, r - 1, SSD1306_WHITE);

        //ochi (cercuri pline)
        disp.fillCircle(cx - 8, cy - 6, 2, SSD1306_WHITE);
        disp.fillCircle(cx + 8, cy - 6, 2, SSD1306_WHITE);

        //gura (linie de 2px)
        if (v.alarm_level == 0) {
            //smiley face — parabola in jos cu varful jos
            for (int dx = -9; dx <= 9; dx++) {
                int dy = 6 - (dx * dx) / 12;
                disp.drawPixel(cx + dx, cy + dy,     SSD1306_WHITE);
                disp.drawPixel(cx + dx, cy + dy + 1, SSD1306_WHITE);
            }
        } else if (v.alarm_level <= 2) {
            //drept — linie dreapta groasa
            disp.drawFastHLine(cx - 9, cy + 7, 18, SSD1306_WHITE);
            disp.drawFastHLine(cx - 9, cy + 8, 18, SSD1306_WHITE);
        } else {
            //trist — parabola in sus cu varful sus
            for (int dx = -9; dx <= 9; dx++) {
                int dy = 10 - (dx * dx) / 12;
                disp.drawPixel(cx + dx, cy + dy,     SSD1306_WHITE);
                disp.drawPixel(cx + dx, cy + dy + 1, SSD1306_WHITE);
            }
        }

        //indicator mic "standby" in coltul jos-stanga cand nu monitorizeaza
        if (!v.monitoring_active) {
            disp.setTextSize(1);
            disp.setCursor(0, 56);
            disp.print("STANDBY");
        }
    }
};
