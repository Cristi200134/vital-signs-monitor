%
% spo2_hw.m, VitalMonitor (proof of concept)
%  Trei verificari numerice aliniate la firmware:
%   1. Formula SpO2 (spo2_sensor.h) vs tabelul de referinta Maxim AN6409
%   2. Reconstructia tensiunii bateriei cu factorul din config.h
%   3. Incarcarea CPU FreeRTOS pe nuclee (estimare)
%  Caracter: simulare / calcul, nu masuratori. Rulare: Run in MATLAB.
%
clear; clc; close all;

% 1. FORMULA SpO2 vs Maxim AN6409
R   = [0.40 0.50 0.60 0.70 0.80 0.90 1.00 1.10 1.20 1.30];
ref = [100   99   97.5  96   94   92   90   86   82   78 ];
sp  = max(70, min(100, -45.06*R.^2 + 30.35*R + 94.85));
err = sp - ref;
fprintf(' FORMULA SpO2 \n');
fprintf('%-6s %-12s %-8s %-8s\n','R','formula','ref','eroare');
for i=1:numel(R); fprintf('%-6.2f %-12.1f %-8.1f %+6.2f%%\n', R(i),sp(i),ref(i),err(i)); end
fprintf('  |eroare| pentru R<=0.70 (SpO2>=96%%): %.2f%%  (valida sub 2%%)\n', max(abs(err(R<=0.70))));

figure('Name','Validare SpO2','Position',[60 60 1000 420]);
subplot(1,2,1); plot(R,ref,'bo-',R,sp,'r^--','LineWidth',1.5); grid on;
  xlabel('R'); ylabel('SpO_2 (%)'); legend('Maxim AN6409','Formula','Location','southwest');
  title('Formula vs referinta');
subplot(1,2,2); bar(R,err,0.6); hold on; yline(3,'r--'); yline(-3,'r--'); grid on;
  xlabel('R'); ylabel('Eroare (%)'); title('Eroare formula (+-3%)');
sgtitle('Fig. Validarea formulei SpO_2 (proof of concept)');

% 2. FACTOR TENSIUNE BATERIE (config.h)
% config.h: ADS_GAIN = GAIN_TWOTHIRDS (+-6.144V) -> LSB = 6.144/32768
% Vbat = raw * BAT_VOLT_FACTOR, cu BAT_VOLT_FACTOR = 0.0001875 (calibrat experimental).
LSB = 6.144/32768;
BAT_VOLT_FACTOR = 0.0001875;
fprintf('\n FACTOR BATERIE (config.h) \n');
fprintf('  LSB (GAIN_TWOTHIRDS) = %.8f V\n', LSB);
fprintf('  BAT_VOLT_FACTOR      = %.8f  (= LSB; calibrat pe punctul raw=20283 -> 3.80 V)\n', BAT_VOLT_FACTOR);
raw = 20283; fprintf('  Verificare: raw=%d -> Vbat = %.3f V\n', raw, raw*BAT_VOLT_FACTOR);
fprintf('  NOTA: cu acest factor, intrarea ADS1115 vede ~tensiunea bateriei.\n');
fprintf('        De verificat pe banc raportul divizorului si incadrarea in domeniul de intrare.\n');

% 3. INCARCARE CPU FreeRTOS (estimare)
tasks = {'ECG','SpO2','Alarme','Temp','OLED','Butoane','WS','SD'};
core  = [1 1 1 1 1 1 0 0];
T_ms  = [4 20 500 2000 500 50 100 1000];
D_ms  = [0.5 1.5 1.0 1.0 3.0 0.2 2.0 5.0];
U = D_ms ./ T_ms * 100;
fprintf('\n UTILIZARE CPU (estimare) \n');
for i=1:numel(tasks); fprintf('  %-9s Core%d  U=%.2f%%\n', tasks{i},core(i),U(i)); end
fprintf('  Core 1 total: %.1f%%  |  Core 0 total: %.1f%%\n', sum(U(core==1)), sum(U(core==0)));
