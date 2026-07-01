%
%  03_spo2_bland.m
% VitalMonitor. Validare SpO2: formula vs referinta + Bland-Altman simulat
%
%  atentie (onestitate): acest script nu contine masuratori clinice reale.
%  Partea 1 valideaza determinist formula din firmware fata de tabelul de
%  referinta Maxim AN6409. Partea 2 este o simulare Monte Carlo a unor
%  perechi de masuratori in intervalul clinic relevant, pentru a ilustra
%  metoda Bland-Altman. Validarea clinica reala (subiecti, co-oximetru de
%  referinta) ramane lucrare viitoare; comparatia cu standardul IEC
%  60601-2-61 (ARMS <= 3%) se face la nivel de proof of concept.
%
%  Rulare: Run in MATLAB.
%
clear; clc; close all;
rng(3);

% PARTEA 1: formula firmware vs Maxim AN6409 (determinist)
R   = [0.40 0.50 0.60 0.70 0.80 0.90 1.00 1.10 1.20 1.30];
ref = [100   99   97.5  96   94   92   90   86   82   78 ];
spo2 = max(70, min(100, -45.06*R.^2 + 30.35*R + 94.85));   % formula din spo2_sensor.h
err  = spo2 - ref;

fprintf(' FORMULA SpO2 vs Maxim AN6409 \n');
fprintf('%-6s %-12s %-10s %-8s\n','R','SpO2 formula','Ref','Eroare');
for i=1:numel(R)
    fprintf('%-6.2f %-12.1f %-10.1f %+6.2f%%\n', R(i), spo2(i), ref(i), err(i));
end
ok = R<=0.70;       % zona unde SpO2 >= ~96%
fprintf('  Eroare |max| pentru R<=0.70 (SpO2>=96%%): %.2f%%  (sub 2%%)\n', max(abs(err(ok))));
fprintf('  Sub 94%% formula liniarizata subestimeaza -> limitare declarata.\n\n');

figure('Name','Formula SpO2','Position',[60 60 1000 420]);
subplot(1,2,1); plot(R,ref,'bo-',R,spo2,'r^--','LineWidth',1.5); grid on;
  xlabel('R (ratio of ratios)'); ylabel('SpO_2 (%)');
  legend('Maxim AN6409','Formula firmware','Location','southwest');
  title('Formula vs referinta');
subplot(1,2,2); bar(R,err,0.6); hold on; yline(3,'r--'); yline(-3,'r--'); grid on;
  xlabel('R'); ylabel('Eroare (%)'); title('Eroare formula (banda +-3%)');
sgtitle('Fig. Validarea determinista a formulei SpO_2');

% PARTEA 2: Bland-Altman pe date SIMULATE (proof of concept)
% Se simuleaza N perechi in intervalul clinic 94-100%, unde formula e valida.
% Citirea dispozitivului = referinta + un mic bias + zgomot de masurare.
N = 45;
ref_sp = 94 + 6*rand(N,1);                 % SpO2 referinta simulata (94-100%)
bias_true = -0.3;                          % mic bias sistematic ipotetic
dev_sp = ref_sp + bias_true + 0.6*randn(N,1);   % citire VitalMonitor simulata
dev_sp = min(100, dev_sp);

dlt = dev_sp - ref_sp;  avg = (dev_sp + ref_sp)/2;
bias = mean(dlt); SD = std(dlt);
LoA_hi = bias + 1.96*SD; LoA_lo = bias - 1.96*SD;
ARMS = sqrt(mean(dlt.^2));               % conform IEC 60601-2-61
MAE  = mean(abs(dlt));

fprintf(' BLAND-ALTMAN (date SIMULATE, N=%d) \n', N);
fprintf('  Bias = %+.3f%% | SD = %.3f%% | LoA = [%+.2f, %+.2f]%%\n', bias,SD,LoA_lo,LoA_hi);
fprintf('  MAE = %.3f%% | ARMS = %.3f%%\n', MAE, ARMS);
fprintf('  Criteriu IEC 60601-2-61: ARMS <= 3%% -> %s (la nivel de simulare)\n', ...
    ternary(ARMS<=3,'INDEPLINIT','NEINDEPLINIT'));
fprintf('  NOTA: rezultat din simulare; validarea clinica reala = lucrare viitoare.\n');

figure('Name','Bland-Altman SpO2 (simulat)','Position',[60 60 1100 460]);
subplot(1,2,1);
  scatter(avg,dlt,45,'b','filled'); hold on;
  yline(bias,'k-',sprintf('Bias=%+.2f%%',bias),'LineWidth',2);
  yline(LoA_hi,'r--',sprintf('+1.96SD=%+.2f%%',LoA_hi),'LineWidth',1.3);
  yline(LoA_lo,'r--',sprintf('-1.96SD=%+.2f%%',LoA_lo),'LineWidth',1.3);
  grid on; xlabel('Media (%)'); ylabel('Dispozitiv - Referinta (%)');
  ylim([-4 4]); title('Bland-Altman (date simulate)');
subplot(1,2,2);
  scatter(ref_sp,dev_sp,45,'b','filled'); hold on;
  lim=[93 101]; plot(lim,lim,'r--','LineWidth',1); grid on;
  xlabel('SpO_2 referinta (%)'); ylabel('SpO_2 VitalMonitor (%)');
  xlim(lim); ylim(lim); axis square; title('Corelatie (date simulate)');
sgtitle(sprintf('Fig. Bland-Altman SIMULAT: Bias=%+.2f%%, ARMS=%.2f%% (proof of concept)',bias,ARMS));

function s = ternary(c,a,b); if c; s=a; else; s=b; end; end
