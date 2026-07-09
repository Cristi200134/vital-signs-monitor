# VitalMonitor

Portable four-parameter vital-signs monitor (ECG, SpO2, temperature, respiration), built from scratch on an ESP32 for a biomedical engineering bachelor's at UMFST "G.E. Palade" Targu Mures. It reproduces the core functions of a clinical patient monitor using accessible parts. Working proof of concept, not a certified medical device.

![Platform](https://img.shields.io/badge/platform-ESP32-1f6feb)
![Firmware](https://img.shields.io/badge/firmware-C%2B%2B%20%2F%20FreeRTOS-orange)
![Validation](https://img.shields.io/badge/MIT--BIH%20F1-98.9%25-2ea44f)
![Status](https://img.shields.io/badge/status-proof%20of%20concept-lightgrey)
![License](https://img.shields.io/badge/license-MIT-blue)

## Highlights

- Four vital signs on one platform: heart rate from the ECG, SpO2, temperature, and respiration derived from the pulse signal.
- QRS detection validated on the MIT-BIH Arrhythmia Database (ANSI/AAMI EC57): F1 98.9%.
- Full engineering pack: schematic, costed bill of materials, 3D-printed enclosure, thesis.
- 3rd place at the Marisiensis International Medical Congress (UMFST, 2026).

## What it does

- ECG with heartbeat detection, SpO2 and temperature, plus a respiratory rate derived from the optical pulse signal.
- Local OLED display and a real-time web interface over the device's own WiFi access point.
- Three-level alarms with LED and buzzer, plus session logging to a microSD card with a real-time clock.

## Hardware

- ESP32 (dual core, FreeRTOS).
- ECG: AD8232 instrumentation amplifier and an external 16-bit ADC (ADS1115, 250 SPS), with a Butterworth filter cascade (high-pass 0.5 Hz, low-pass 40 Hz, notch 50 Hz, about 66 dB rejection at 50 Hz).
- SpO2 and respiration: MAX30102 optical sensor (ratio-of-ratios; respiration from the infrared DC modulation).
- Temperature: DS18B20. Clock: DS3231. Display: 128x64 OLED. Storage: microSD.
- Power: 1800 mAh LiPo, boost to 5 V, separate AMS1117 3.3 V rail for the analog section.
- Enclosure: plywood body with 3D-printed PLA panels (drawings in `hardware/`).

## Firmware

- C++/Arduino on FreeRTOS, eight tasks pinned across both cores by clinical priority, two mutexes (data and I2C), I2C at 400 kHz.
- ADS1115 runs in continuous mode so a 250 SPS ECG stream does not block the shared I2C bus.
- QRS detection: a simplified Pan-Tompkins (5-point derivative, squaring, 148 ms moving-window integration, adaptive SPKI/NPKI threshold, 200 ms refractory, averaging over 16 RR intervals).

## Validation

Heartbeat detection (simplified Pan-Tompkins) was tested on the MIT-BIH database, about 70,000 annotated beats over 30 records, with a 150 ms matching tolerance, following ANSI/AAMI EC57.

| Metric | Value | Method |
|---|---|---|
| Sensitivity (Se) | 98.66% | MIT-BIH, 30 records, ~70,000 beats |
| Positive predictivity (+P) | 99.11% | same |
| F1 score | 98.9% | same |
| ECG filter | SNR > 60 dB, stable (poles inside the unit circle) | simulation |

## Known limitations 


- The false-alarm rate is above the target I set for it.
- Latency, CPU load and battery life are budget estimates, not bench measurements.
- QRS detection was validated as a MATLAB reimplementation with the same coefficients and thresholds as the firmware, not as the compiled binary running on the chip.
- It is a research prototype, not a certified medical device.

## Repository structure

- `firmware/` ESP32 source (config, sensors, FreeRTOS tasks, web server).
- `hardware/` schematic (PDF), bill of materials, enclosure files (STL).
- `validation/` MATLAB scripts and the MIT-BIH validation.
- `docs/` thesis extract, device photos, validation captures.

## Tech

ESP32, FreeRTOS, C++, AD8232, ADS1115, MAX30102, DS18B20, MATLAB, PlatformIO.

## Author

Cristi Dica, biomedical engineer, Targu Mures. 

## License

MIT. See `LICENSE`.
