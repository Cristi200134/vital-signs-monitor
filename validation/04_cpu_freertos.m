%
%  04_cpu_freertos.m
% VitalMonitor. Analiza FreeRTOS: incarcare CPU si buget de memorie
%
%  Caracter: estimare din specificatii si din perioadele task-urilor
%  (proof of concept), nu profilare masurata pe placa. Latenta end-to-end
%  este tratata in 12_latenta.m, iar autonomia in 11_autonomie.m.
%
%  Parametrii (perioade, prioritati, stive) sunt cei din main.cpp / config.h.
%  Rulare: Run in MATLAB.
%
clear; clc; close all;

% nume, core, perioada[ms], durata exec estimata[ms], prioritate, stiva[bytes]
T = {
 'taskEcg',      1,    4,   0.5, 5, 4096;
 'taskSpo2',     1,   20,   1.5, 4, 4096;
 'taskAlarme',   1,  500,   1.0, 3, 2048;
 'taskTemp',     1, 2000,   1.0, 2, 2048;
 'taskOled',     1,  500,   3.0, 2, 3072;
 'taskButoane',  1,   50,   0.2, 1, 2048;
 'taskWebSocket',0,  100,   2.0, 3, 8192;
 'taskSd',       0, 1000,   5.0, 2, 4096;
};
n = size(T,1);
names = T(:,1); core = cell2mat(T(:,2)); per = cell2mat(T(:,3));
exec  = cell2mat(T(:,4)); prio = cell2mat(T(:,5)); stack = cell2mat(T(:,6));
util = exec ./ per * 100;

fprintf(' INCARCARE CPU PE TASK (estimare) \n');
fprintf('%-14s %-5s %-9s %-9s %-7s %-6s\n','Task','Core','Per(ms)','Exec(ms)','U(%)','Stiva');
for i=1:n
    fprintf('%-14s %-5d %-9d %-9.1f %-7.2f %-6d\n', names{i},core(i),per(i),exec(i),util(i),stack(i));
end
U1 = sum(util(core==1)); U0 = sum(util(core==0));
fprintf('  -----------------------------------------------------\n');
fprintf('  Core 1 (achizitie) total: %.1f %%\n', U1);
fprintf('  Core 0 (comunicatii) total: %.1f %%\n', U0);
fprintf('  Limita recomandata pe nucleu: < 80%% -> ambele nuclee au marja mare.\n\n');

% Buget de memorie (stive alocate)
totStack = sum(stack);
fprintf(' BUGET STIVA FREEERTOS \n');
fprintf('  Suma stivelor alocate: %d bytes (%.1f KB) din 520 KB SRAM\n', totStack, totStack/1024);
fprintf('  Reprezinta %.1f%% din SRAM total; restul ramane heap + sistem.\n\n', totStack/(520*1024)*100);

% Timp de procesare per esantion ECG (estimare teoretica)
Fs = 250; Tsamp_us = 1e6/Fs;
fmac = 15;                       % 3 sectiuni biquad Direct Form II Transposed
f_cpu = 240e6;
T_filt_us = fmac / f_cpu * 1e6;  % timp teoretic minim
fprintf(' TIMP PROCESARE FILTRU ECG (teoretic) \n');
fprintf('  Buget per esantion (1/250 Hz): %.0f us\n', Tsamp_us);
fprintf('  Filtru cascadat (~%d FMAC @ 240 MHz): %.3f us teoretic\n', fmac, T_filt_us);
fprintf('  Chiar cu marja larga de implementare, filtrul ocupa o fractiune din buget.\n');

% Figuri
figure('Name','FreeRTOS CPU & memorie','Position',[60 60 1150 480]);
subplot(1,2,1);
  [us,ix] = sort(util,'descend');
  barh(us,0.6,'FaceColor',[0.2 0.5 0.8]); set(gca,'YTickLabel',names(ix),'YDir','reverse');
  grid on; xlabel('Utilizare CPU (%)');
  title(sprintf('Incarcare per task (Core1=%.1f%%, Core0=%.1f%%)',U1,U0));
subplot(1,2,2);
  bar([U1 U0],0.5,'FaceColor',[0.2 0.7 0.4]); hold on; yline(80,'r--','limita 80%');
  set(gca,'XTickLabel',{'Core 1','Core 0'}); ylim([0 100]); grid on;
  ylabel('Utilizare totala (%)'); title('Incarcare pe nuclee');
sgtitle('Fig. Analiza FreeRTOS: CPU si memorie (estimare, proof of concept)');
