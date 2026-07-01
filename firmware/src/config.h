#pragma once
//config centralizat —hardware (pini, constante, praguri) 
//versiune firmware
#define FW_VERSION   "1.0.0"
#define DEVICE_NAME  "VitalMonitor"

//pin map
//pinii speciali de pe esp32:
//gpio5  —strapping pin (boot: spi_clk_div), nu folosim ca sd_cs
//gpio12 —strapping pin (boot: flash voltage), nu poate fi input la boot
//gpio34,35,36,39 -input only, fara pull-up intern
//gpio2  —led intern pe devkit, ok dupa boot
//i2c (oled 0x3c, ads1115 0x48, ds3231 0x68, max30102 0x57)
#define PIN_SDA          21
#define PIN_SCL          22

//onewire pentru ds18b20
#define PIN_DS18B20       4

//ad8232 lead-off
#define PIN_LO_PLUS      34   //input only, fara pull-up intern
#define PIN_LO_MINUS     35   //input only, fara pull-up intern

//spi pentru cardul sd
//cs e pe gpio27 
//pull-up extern de 10k pe linia cs evita init fals la boot
#define PIN_SD_CS        27
#define PIN_SD_MOSI      23
#define PIN_SD_MISO      19
#define PIN_SD_SCK       18

//led rgb catod comun (high = aprins)
//verde-monitorizare activa, totul ok
//rosu-alarma critica (clipeste rapid)
//albastru-standby
#define PIN_LED_R        25
#define PIN_LED_G        33
#define PIN_LED_B        32

//buzzer pasiv
#define PIN_BUZZER       26


//btn_men (gpio36 albastru) —paginile oled (p1->p2->p3->p1)

//input only- pull-up extern 10k la 3.3v
//btn_sil (gpio39 galben) — silence buzzer 1 min
//  input only, pull-up extern 10k la 3.3v

//btn_sel (gpio14 negru) — overnight
// iese automat cand o val fiziologica iese din range
//pull-up intern
//btn_rec (gpio13 rosu) — start/stop sesiune
//btn_ev (gpio15 ttp223) — eveniment pacient (panic button)
//marcheaza ev in csv si notifica interfata

#define PIN_BTN_MEN      36   //albastru 
#define PIN_BTN_SIL      39   //galben  
#define PIN_BTN_SEL      14   //negru    
#define PIN_BTN_REC      13   //rosu    
#define PIN_BTN_EV       15   //ttp223  

//timpi butoane
#define BTN_DEBOUNCE_MS    200   //debounce general
#define BTN_SIL_MS       60000  //silence buzzer 1 minut

//debug toggles
#define BAT_DEBUG          1     //1 = log [bat] in serial la fiecare citire baterie

//wifi ap
#define WIFI_AP_SSID    "VitalMonitor"
#define WIFI_AP_PASS    "vital1234"
#define WIFI_AP_CHANNEL  1
#define WIFI_AP_HIDDEN   0    //0 = vizibil, 1 = ascuns
#define WIFI_AP_MAX_CONN 2    //maxim 2 clienti simultan

//webserver
#define WEB_PORT         80
#define WS_PATH          "/ws"

//frecvente esantionare
#define ECG_SPS          250  //hz, ads1115
#define SPO2_SPS          50  //hz, 
#define TEMP_INTERVAL_MS 2000 //ms, ds18b20 (conversie ~750ms)
#define SD_LOG_INTERVAL_MS 1000 //ms, o inregistrare pe secunda
#define NUMERIC_TX_MS    2000 //ms, cat de des trim numerele (bpm, temp, spo2, rr)
#define WAVE_TX_MS        100 //ms, cat de des trim batch-ul ecg/spo2

//ads1115
//canal 0 (a0): semnal ecg de la ad8232
//canal 1 (a1): tensiune baterie prin divizor 100k/100k
#define ADS_ECG_CHANNEL   0
#define ADS_BAT_CHANNEL   1
#define ADS_GAIN          GAIN_TWOTHIRDS  //+- 6.144v in ecg sensor
//conv tens baterie: vbat = raw * factor
//calibrarea de baza: 0.0001875 = 6.144 / 32768 (gain 2/3)
//calibrat experimental pe setup (cu multimetrul pe baterie):
//raw=20283 → vbat=3.80v → factor ≈ 0.0001875 (fara *2)
#define BAT_VOLT_FACTOR   0.0001875f
#define BAT_VOLT_FULL     4.20f   //v — baterie plina (lipo)
#define BAT_VOLT_EMPTY    3.00f   //v — baterie descarcata

//praguri alarme
//hr (bpm)
#define ALARM_HR_LOW_CRIT   45   //< 45 bpm = bradicardie critica
#define ALARM_HR_LOW_WARN   55   //< 55 bpm = atentionare
#define ALARM_HR_HIGH_WARN 110   //> 110 bpm = atentionare
#define ALARM_HR_HIGH_CRIT 130   //> 130 bpm = tahicardie critica

//spo2 (%)
#define ALARM_SPO2_CRIT     90   //< 90% = hipoxie critica
#define ALARM_SPO2_WARN     94   //< 94% = atentionare

//temperatura ('c)
#define ALARM_TEMP_LOW      35.0f  //< 35'c = hipotermie
#define ALARM_TEMP_HIGH     38.5f  //> 38.5'c = febra

//respiratie (rpm)
#define ALARM_RESP_LOW      10   //< 10 rpm = bradipnee
#define ALARM_RESP_HIGH     25   //> 25 rpm = tahipnee

//baterie (%)
#define ALARM_BAT_LOW       15   //< 15% = incarcare 
#define ALARM_BAT_CRIT       5   //< 5% = inchidere 

//coeficienti filtru ecg butterworth
//cascada 3 sectiuni biquad (IIR direct form II transpusa):
//  1. high-pass  butterworth ord.2  fc = 0.5 hz ; 250 sps
//  2. low-pass   butterworth ord.2  fc = 40  hz ; 250 sps
//  3. notch IIR  q=30               f0 = 50  hz ; 250 sps

//recalculati in matlab cu:
//butter(2, 0.5/125.0, 'high')
//butter(2, 40.0/125.0, 'low')
//iirnotch(50.0/125.0, 50.0/125.0/30)
//q=30 pentru notch → banda eliminata ~1.7 hz (49.15-50.85 hz)
//asa pastram semnalul util si scoatem precis 50 hz din retea

//sectiunea hp (0.5 hz ; 250 sps)
#define HP_B0   0.99115452f
#define HP_B1  (-1.98230904f)
#define HP_B2   0.99115452f
#define HP_A1  (-1.98223079f)
#define HP_A2   0.98238576f

//sectiunea lp (40 hz ; 250 sps) ecg filter
#define LP_B0   0.14531600f
#define LP_B1   0.29063100f
#define LP_B2   0.14531600f
#define LP_A1  (-0.67106700f)
#define LP_A2   0.25234400f

//sectiunea notch (50 hz, q=30 ; 250 sps)
#define NF_B0   0.97948100f
#define NF_B1  (-0.60531400f)
#define NF_B2   0.97948100f
#define NF_A1  (-0.60531400f)
#define NF_A2   0.95896200f

//oled
#define OLED_WIDTH      128
#define OLED_HEIGHT      64
#define OLED_I2C_ADDR  0x3C

//freertos —dimensiuni stack
#define STACK_ECG       4096
#define STACK_SPO2      4096
#define STACK_TEMP      2048
#define STACK_OLED      3072
#define STACK_WEBSOCKET 8192
#define STACK_SDLOG     4096
#define STACK_ALARMS    2048
#define STACK_BUTTONS   2048

//sd card
#define SD_LOG_DIR      "/sessions"
#define SD_MIN_FREE_MB  50    // pt alerta cand spatiu liber < 50 mb
