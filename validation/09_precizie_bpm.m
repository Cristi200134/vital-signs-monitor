%
%  09_precizie_bpm.m
%  VitalMonitor - Precizia masurarii BPM (cuantizare temporala + mediere)
%
%  Sursa de eroare: rezolutia ceasului millis() = 1 ms. Firmware-ul mediaza
%  pe 16 intervale RR (RR_N din ecg_sensor.h), reducand eroarea de sqrt(16).
%  Caracter: calcul analitic (proof of concept). Rulare: Run in MATLAB.
%
%  nota: nu numiti fisierul la fel ca o variabila din el (ex. BPM.m + BPM).
%        Pastrati numele 09_precizie_bpm.m sau orice nume distinct.
%
clear; clc; close all;

dt_ms   = 1;
RR_N    = 16;
bpm_vec = 30:1:240;
RR      = 60000 ./ bpm_vec;                          % interval RR (ms)
dBPM    = 60000./(RR-dt_ms) - 60000./(RR+dt_ms);     % spread la +-1 ms
dBPM_avg = dBPM / sqrt(RR_N);                        % dupa medierea pe 16 RR

fprintf(' PRECIZIE BPM (cuantizare millis + medie 16 RR) \n');
puncte = [40 60 72 100 150 180 220];
for k = 1:length(puncte)
    b = puncte(k);
    [~, ix] = min(abs(bpm_vec - b));
    fprintf('  %3d BPM (RR=%4.0f ms): 1 masurare +/- %.3f | medie 16 RR +/- %.4f BPM\n', ...
        b, RR(ix), dBPM(ix)/2, dBPM_avg(ix)/2);
end
fprintf('  Medierea pe 16 intervale reduce eroarea de %.1f ori.\n', sqrt(RR_N));

figure('Name', 'Precizie BPM');

subplot(2,1,1);
plot(bpm_vec, dBPM/2, 'r-', 'LineWidth', 1.5);
hold on;
plot(bpm_vec, dBPM_avg/2, 'b-', 'LineWidth', 2);
plot([30 240], [1 1], 'k--');
plot([30 240], [0.5 0.5], 'g--');
hold off;
grid on;
xlabel('BPM');
ylabel('Eroare (+/- BPM)');
xlim([30 240]);
legend('1 masurare', 'medie 16 RR', '+/-1 BPM', '+/-0.5 BPM');
title('Eroarea de cuantizare a BPM');

subplot(2,1,2);
mu = 72;
sg = 8;
x = 40:0.1:120;
y = exp(-(x - mu).^2 / (2*sg^2)) / (sg*sqrt(2*pi));   % Gaussian, fara toolbox
plot(x, y, 'b-', 'LineWidth', 2);
grid on;
xlabel('BPM');
ylabel('Densitate');
title('Distributie HR repaus N(72, 8) ca referinta');
