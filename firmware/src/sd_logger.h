#pragma once
//sd logger — scriere csv pe cardul microsd (spi)
//toate operatiile sd se intampla exclusiv in tasksd (core 0)
//nu apelam deschidesesiune/inchide din alte task-uri sau callback-uri

#include <SD.h>
#include <SPI.h>
#include "vital_data.h"
#include "config.h"

class SdLogger {
public:
    //apelat o singura data din tasksd la pornire
    bool init() {
        SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
        if (!SD.begin(PIN_SD_CS)) {
            Serial.println("[sd] card negasit (cs=" + String(PIN_SD_CS) + ")");
            return _ok = false;
        }
        if (!SD.exists(SD_LOG_DIR)) SD.mkdir(SD_LOG_DIR);

        uint64_t total = SD.totalBytes(), used = SD.usedBytes();
        _libGb = (float)(total - used) / (1024.0f * 1024.0f * 1024.0f);
        Serial.printf("[sd] ok — liber: %.2f gb\n", _libGb);
        return _ok = true;
    }

    //apelat din tasksd la tranzitia monitoring_active false->true
    bool deschideSesiune(const VitalData& snap) {
        if (!_ok) return false;
        inchide();

        char dt[20] = "sesiune";
        strncpy(dt, snap.datetime_str, 19); dt[19] = '\0';
        for (int i = 0; dt[i]; i++)
            if (dt[i] == ' ' || dt[i] == ':') dt[i] = '-';

        char pac[24] = "necunoscut";
        if (snap.patient_name[0] != '\0') {
            strncpy(pac, snap.patient_name, 23); pac[23] = '\0';
            for (int i = 0; pac[i]; i++)
                if (pac[i] == ' ' || pac[i] == '/' || pac[i] == '\\') pac[i] = '_';
        }

        snprintf(_numeFisier, sizeof(_numeFisier), "%s_%s.csv", dt, pac);
        char cale[128];
        snprintf(cale, sizeof(cale), "%s/%s", SD_LOG_DIR, _numeFisier);

        _fisier = SD.open(cale, FILE_WRITE);
        if (!_fisier) {
            Serial.printf("[sd] nu pot crea fisierul: %s\n", cale);
            return false;
        }

        _fisier.println("Data,Ora,BPM,SpO2%,Temp.C,Resp.rpm,Alarma,Electrozi,Ev");
        _fisier.flush();
        _deschis = true; _nrRanduri = 0;
        Serial.printf("[sd] sesiune noua: %s\n", cale);
        return true;
    }

    //apelat din tasksd la fiecare ciclu cand monitoring e activ
    void scrieRand(const VitalData& snap) {
        if (!_deschis) return;

        char data[12] = "---", ora[10] = "---";
        if (strlen(snap.datetime_str) >= 19) {
            strncpy(data, snap.datetime_str,      10); data[10] = '\0';
            strncpy(ora,  snap.datetime_str + 11,  8); ora[8]   = '\0';
        }

        char temp[8] = "--";
        if (snap.temp_valid && snap.temp_c > -40.0f && snap.temp_c < 85.0f)
            snprintf(temp, sizeof(temp), "%.1f", snap.temp_c);

        char resp[8] = "--";
        if (snap.resp_valid && snap.resp_rpm > 0)
            snprintf(resp, sizeof(resp), "%.0f", snap.resp_rpm);

        _fisier.printf("%s,%s,%.0f,%.0f,%s,%s,%s,%s,%s\n",
            data, ora,
            snap.hr_valid   ? snap.bpm  : 0.0f,
            snap.spo2_valid ? snap.spo2 : 0.0f,
            temp, resp,
            snap.alarm_level > 0 ? snap.alarm_msg : "",
            snap.lead_off ? "DA" : "NU",
            _eventFlag ? "DA" : ""
        );
        _fisier.flush();
        _eventFlag = false;

        if (++_nrRanduri % 60 == 0) verificaSpatiu();
    }

    //apelat din tasksd la tranzitia monitoring_active true->false
    void inchide() {
        if (!_deschis) return;
        _fisier.flush(); _fisier.close();
        _deschis = false;
        Serial.printf("[sd] sesiune inchisa: %s (%lu randuri)\n", _numeFisier, _nrRanduri);
    }

    void marcheazaEveniment() { _eventFlag = true; }

    bool   esteMontat()   const { return _ok; }
    float  spatioLiber()  const { return _libGb; }
    String fisierCurent() const { return String(_numeFisier); }

private:
    File     _fisier;
    bool     _ok = false, _deschis = false, _eventFlag = false;
    uint32_t _nrRanduri = 0;
    float    _libGb     = 0.0f;
    char     _numeFisier[80] = "";

    void verificaSpatiu() {
        uint64_t total = SD.totalBytes(), used = SD.usedBytes();
        _libGb = (float)(total - used) / (1024.0f * 1024.0f * 1024.0f);
        _aproapePlin = (_libGb * 1024.0f < SD_MIN_FREE_MB);
        Serial.printf("[sd] liber: %.2f gb%s\n", _libGb,
                      _aproapePlin ? " — atentie: card aproape plin!" : "");
    }
public:
    bool aproapePlin() const { return _aproapePlin; }
private:
    bool _aproapePlin = false;
};
