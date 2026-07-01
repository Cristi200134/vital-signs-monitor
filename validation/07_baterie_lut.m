%
%  07_baterie_lut.m
% VitalMonitor. Validarea tabelului de interpolare a starii bateriei
%
%  Verifica LUT-ul tensiune -> procent (SoC) din firmware (ecg_sensor.h)
%  fata de o curba LiPo de referinta (segmente liniare tipice). Caracter:
%  validare prin simulare (proof of concept). Rulare: Run in MATLAB.
%
clear; clc; close all;

% LUT identic cu ecg_sensor.h
LUT_V = [4.20 4.05 3.95 3.85 3.75 3.65 3.55 3.45 3.30 3.20 3.00];
LUT_P = [100  90   80   70   60   50   40   30   15   5    0  ];

V = linspace(3.0, 4.2, 600);
% Curba LiPo de referinta (segmente liniare pe baza datasheet tipic)
SoC_ref = zeros(size(V));
for i=1:numel(V)
    v = V(i);
    if     v>=4.05, SoC_ref(i)=90+(v-4.05)/(4.20-4.05)*10;
    elseif v>=3.75, SoC_ref(i)=60+(v-3.75)/(4.05-3.75)*30;
    elseif v>=3.55, SoC_ref(i)=40+(v-3.55)/(3.75-3.55)*20;
    elseif v>=3.30, SoC_ref(i)=15+(v-3.30)/(3.55-3.30)*25;
    elseif v>=3.20, SoC_ref(i)= 5+(v-3.20)/(3.30-3.20)*10;
    else            SoC_ref(i)= 0+(v-3.00)/(3.20-3.00)*5;
    end
end
SoC_fw = max(0, min(100, interp1(LUT_V, LUT_P, V, 'linear','extrap')));
e = SoC_fw - SoC_ref;

fprintf(' LUT BATERIE (firmware vs referinta) \n');
fprintf('  Eroare medie absoluta: %.2f %%\n', mean(abs(e)));
fprintf('  Eroare maxima:         %.2f %%\n', max(abs(e)));
fprintf('  La 3.20 V (prag 5%%):  fw=%.0f%%\n',  interp1(V,SoC_fw,3.20));
fprintf('  La 3.30 V (prag 15%%): fw=%.0f%%\n',  interp1(V,SoC_fw,3.30));

figure('Name','LUT baterie','Position',[60 60 950 620]);
subplot(2,1,1);
  plot(V,SoC_ref,'b-','LineWidth',2); hold on;
  plot(V,SoC_fw,'r--','LineWidth',2);
  plot(LUT_V,LUT_P,'ko','MarkerFaceColor','k','MarkerSize',6);
  grid on; xlabel('Tensiune (V)'); ylabel('SoC (%)');
  legend('LiPo referinta','LUT firmware','Puncte LUT','Location','southeast');
  title('Stare de incarcare: LUT firmware vs referinta');
subplot(2,1,2);
  plot(V,e,'r-','LineWidth',1.5); hold on; yline(3,'--k'); yline(-3,'--k');
  grid on; xlabel('Tensiune (V)'); ylabel('Eroare (%)'); ylim([-10 10]);
  title('Eroarea interpolarii LUT');
sgtitle('Fig. Validarea LUT-ului bateriei (proof of concept)');
