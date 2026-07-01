%
%  12_latenta.m
% VitalMonitor. Bugetul de latenta end-to-end (ADC -> browser)
%
%  Caracter: BUGET teoretic calculat din perioadele task-urilor si din
%  parametrii de transmisie (proof of concept), nu masuratoare. Componenta
%  dominanta: acumularea batch-ului (100 ms) si perioada task-ului WebSocket.
%  Comparatie cu IEC 60601-2-27 (< 500 ms) la nivel de proiectare.
%  Rulare: Run in MATLAB.
%
clear; clc; close all;

etape = {'ADS1115 conversie','ECG task period','Acumulare batch (100 ms)', ...
         'WebSocket task period','JSON serialize','WiFi TX', ...
         'Browser parse','Canvas render'};
typ = [4 4 100 100 1 5 2 16];
wc  = [4 8 100 200 2 15 5 33];
Ttyp = sum(typ); Twc = sum(wc); LIM = 500;

fprintf(' LATENTA END-TO-END (buget) \n');
fprintf('%-26s %10s %10s\n','Etapa','Tipic(ms)','WC(ms)');
for i=1:numel(etape); fprintf('%-26s %10d %10d\n', etape{i}, typ(i), wc(i)); end
fprintf('%-26s %10d %10d\n','TOTAL',Ttyp,Twc);
fprintf('\nCerinta IEC 60601-2-27: < %d ms\n', LIM);
fprintf('  Tipic:      %d ms -> %s\n', Ttyp, ternary(Ttyp<LIM,'in limita','depasit'));
fprintf('  Worst-case: %d ms -> %s\n', Twc,  ternary(Twc<LIM,'in limita','depasit'));

figure('Name','Latenta','Position',[60 60 1050 480]);
subplot(1,2,1);
  barh(typ,0.5,'FaceColor',[0.2 0.6 0.9]); set(gca,'YTickLabel',etape,'YDir','reverse');
  grid on; xlabel('ms'); title(sprintf('Latenta tipica pe etapa, total %d ms',Ttyp));
subplot(1,2,2);
  barh(cumsum(wc),0.5,'FaceColor',[0.9 0.4 0.2]); set(gca,'YTickLabel',etape,'YDir','reverse');
  hold on; xline(LIM,'--r','500 ms IEC'); grid on; xlabel('ms');
  title(sprintf('Latenta cumulata worst-case, total %d ms',Twc));
sgtitle('Fig. Bugetul de latenta end-to-end (proof of concept)');

function s = ternary(c,a,b); if c; s=a; else; s=b; end; end
