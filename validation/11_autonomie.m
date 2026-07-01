%
%  11_autonomie.m
% VitalMonitor. Estimarea autonomiei bateriei pe scenarii de utilizare
%
%  Caracter: calcul din curenti de catalog / valori tipice (proof of concept),
%  nu masuratoare integrala. Bateria si curentii reflecta configuratia reala
%  (LiPo 1800 mAh). Autonomia = capacitate / curent mediu pe scenariu.
%  Rulare: Run in MATLAB.
%
clear; clc; close all;

comp = {'ESP32 WiFi AP','ADS1115','MAX30102','DS18B20','AD8232', ...
        'OLED 0.96"','DS3231 RTC','Card SD activ','LED RGB (medie)', ...
        'Buzzer idle','TP4056 quiescent'};
I_mA = [160, 0.15, 1.0, 1.5, 0.35, 15.0, 0.2, 50.0, 5.0, 0.0, 3.0];

V_bat = 3.7; C_mAh = 1800;
I_tot = sum(I_mA); P_tot = I_tot*V_bat/1000;

fprintf(' CONSUM PE COMPONENTA \n');
for i=1:numel(comp); fprintf('  %-18s %6.2f mA\n', comp{i}, I_mA(i)); end
fprintf('  %-18s %6.2f mA  (%.2f W)\n', 'TOTAL', I_tot, P_tot);

scen = {'Monitorizare activa (SD+WiFi+OLED)','Overnight (OLED off, SD off)', ...
        'Standby (fara SD/OLED/WiFi)','Fara WiFi (senzori + SD)'};
I_sc = [I_tot, I_tot-15-50, I_tot-15-50-160+20, I_tot-160+20];
fprintf('\n AUTONOMIE (%d mAh @ %.1f V) \n', C_mAh, V_bat);
for i=1:numel(scen); fprintf('  %-38s %.1f h\n', scen{i}, C_mAh/I_sc(i)); end

figure('Name','Autonomie baterie','Position',[60 60 1000 680]);
subplot(2,1,1);
  barh(I_mA,0.6,'FaceColor',[0.2 0.5 0.8]); set(gca,'YTickLabel',comp,'YDir','reverse');
  grid on; xlabel('Curent (mA)');
  title(sprintf('Consum per componenta, total %.0f mA @ %.1f V = %.2f W', I_tot, V_bat, P_tot));
subplot(2,1,2);
  h = C_mAh ./ I_sc; bar(h,0.5,'FaceColor',[0.2 0.7 0.4]); hold on;
  yline(8,'--r','tura 8 h'); yline(12,'--g','tura 12 h');
  set(gca,'XTickLabel',{'Activ','Overnight','Standby','Fara WiFi'}); grid on;
  ylabel('Autonomie (ore)'); ylim([0 max(h)*1.15]);
  for i=1:numel(h); text(i,h(i)+0.3,sprintf('%.1f h',h(i)),'HorizontalAlignment','center','FontWeight','bold'); end
  title('Autonomie pe scenariu de operare');
sgtitle('Fig. Estimarea autonomiei bateriei (calcul, proof of concept)');
