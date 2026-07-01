#pragma once
//filtru butterworth iir, cascada de 3 sectiuni biquad

//1. high-pass ord.2  fc = 0.5 hz   (scoate deriva dc si modulatia respiratorie)
//2. low-pass  ord.2  fc = 40  hz   (scoate emg si artefactele de miscare)
//3. notch iir q=30   f0 = 50  hz   (scoate interferenta de la retea)
//                       q=30 → banda eliminata ingusta de ~1.7 hz (49.15-50.85 hz)
//                              semnalul util ramane intact

//implementare: direct form II transpusa (numerica stabila)

//b,a = butter(2, 0.5/125.0, 'highpass')
//b,a = butter(2, 40.0/125.0, 'lowpass')
//b,a = iirnotch(50.0, 30.0, 250.0)

#include "config.h"

//o sectiune biquad iir
//formula: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
//forma transpusa ii: w1/w2 sunt starile interne — numeric stabil,
//fara overflow la inmultiri succesive ca la direct form i
struct BiquadSection {
    float b0, b1, b2;  //coeficienti numarator (feed-forward)
    float a1, a2;      //coeficienti numitor (feed-back, normalizati a0=1)
    float w1, w2;      //stari interne

    BiquadSection() : b0(0.0f), b1(0.0f), b2(0.0f), a1(0.0f), a2(0.0f), w1(0.0f), w2(0.0f) {}
    BiquadSection(float b0, float b1, float b2, float a1, float a2)
        : b0(b0), b1(b1), b2(b2), a1(a1), a2(a2), w1(0.0f), w2(0.0f) {}

    //proceseaza un esantion — inline pentru viteza pe core 1
    inline float process(float x) {
        float y = b0 * x + w1;
        w1 = b1 * x - a1 * y + w2;
        w2 = b2 * x - a2 * y;
        return y;
    }

    //reset starile (ex: la pierderea contactului electrozilor)
    //fara reset,la reconectare ar declansa fals pan-tompkins
    inline void reset() { w1 = 0.0f; w2 = 0.0f; }
};

//cascadeaza 3 sectiuni biquad in serie
//ordinea: hp → lp → notch
//hp-ul scoate dc inainte de lp, ca sa nu se satureze lp-ul
class EcgFilter {
public:
    EcgFilter()
        : hp(HP_B0, HP_B1, HP_B2, HP_A1, HP_A2),
          lp(LP_B0, LP_B1, LP_B2, LP_A1, LP_A2),
          nf(NF_B0, NF_B1, NF_B2, NF_A1, NF_A2)
    {}

    //filtreaza un esantion ecg brut (adc raw)
    //intra:  raw adc int16 de la ads1115 (±32767 ; ±6.144v)
    //iese:  float filtrat, pastrat in domeniu int16 pentru transmisie
    float process(int16_t raw) {
        float x = (float)raw;
        x = hp.process(x);   //1. scoate deriva dc
        x = lp.process(x);   //2. scoate emg si zgomot hf
        x = nf.process(x);   //3. scoate 50 hz din retea
        return x;
    }

    //reset la reconectare electrozi
    void reset() {
        hp.reset();
        lp.reset();
        nf.reset();
    }

private:
    BiquadSection hp;
    BiquadSection lp;
    BiquadSection nf;
};
