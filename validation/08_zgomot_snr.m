%
%  08_zgomot_snr.m
% VitalMonitor. Buget de zgomot si SNR pentru lantul de achizitie ECG
%                   AD8232 -> ADS1115 (16-bit, 250 SPS) -> Butterworth 0.5-40Hz
%
%  Caracter: estimare din valorile de catalog (proof of concept), nu
%  masuratoare. Foloseste GAIN_TWOTHIRDS (+-6.144V) ca in config.h.
%  Rulare: Run in MATLAB.
%
clear; clc; close all;

% ADS1115
Vref = 6.144; N = 16; Fs = 250;
LSB  = Vref / 2^(N-1);                 % GAIN_TWOTHIRDS
SNR_ideal = 6.02*N + 1.76;
noise_adc_uV = 128;                    % uV RMS, datasheet ADS1115 @ 250 SPS

fprintf(' ADS1115 \n');
fprintf('  LSB = %.2f uV  |  SNR ideal 16-bit = %.1f dB\n', LSB*1e6, SNR_ideal);
fprintf('  Zgomot @250 SPS = %.0f uV RMS = %.2f LSB\n', noise_adc_uV, noise_adc_uV/(LSB*1e6));

% AD8232
G = 100; en = 30e-9; BW = 150;
noise_in_uV  = en*sqrt(BW)*1e6;
noise_out_uV = noise_in_uV*G;
fprintf('\n AD8232 \n');
fprintf('  Gain=%d, en=%.0f nV/sqrt(Hz) -> zgomot iesire %.1f uV RMS\n', G, en*1e9, noise_out_uV);

% Zgomot total si SNR
noise_tot_uV = sqrt(noise_adc_uV^2 + noise_out_uV^2);
Vrpeak_mV = 1.0; V_adc_mV = Vrpeak_mV*G;
SNR_raw = 20*log10((V_adc_mV*1e-3)/(noise_tot_uV*1e-6));
BWf = 40-0.5; SNR_gain = 10*log10((Fs/2)/BWf);
SNR_filt = SNR_raw + SNR_gain;
noise_filt_uV = noise_tot_uV*sqrt(BWf/(Fs/2));

fprintf('\n SNR (estimat) \n');
fprintf('  Zgomot total brut: %.1f uV RMS\n', noise_tot_uV);
fprintf('  SNR inainte de filtru: %.1f dB\n', SNR_raw);
fprintf('  Banda filtru 0.5-40 Hz -> reducere zgomot %.1f dB\n', SNR_gain);
fprintf('  SNR dupa filtru: %.1f dB (zgomot %.1f uV RMS)\n', SNR_filt, noise_filt_uV);
fprintf('  Semnal minim detectabil (SNR>6dB): %.1f uV la electrozi\n', noise_filt_uV*10^(6/20)/G);

figure('Name','SNR achizitie','Position',[60 60 950 560]);
subplot(2,1,1);
  v=[noise_adc_uV, noise_out_uV, noise_tot_uV, noise_filt_uV];
  bar(v,0.5,'FaceColor',[0.2 0.5 0.8]); grid on;
  set(gca,'XTickLabel',{'ADS1115','AD8232','Total brut','Dupa filtru'});
  ylabel('Zgomot RMS (uV)'); title('Buget de zgomot, lant ECG');
subplot(2,1,2);
  bar([SNR_raw SNR_filt],0.5,'FaceColor',[0.2 0.7 0.4]); grid on;
  set(gca,'XTickLabel',{'Inainte de filtru','Dupa filtru 0.5-40Hz'});
  ylabel('SNR (dB)'); ylim([0 80]); title('SNR estimat al lantului de achizitie');
sgtitle('Fig. Buget de zgomot si SNR (estimare din catalog, proof of concept)');
