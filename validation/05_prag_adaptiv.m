%
%  05_prag_adaptiv.m
% VitalMonitor. Validare prin simulare a pragului adaptiv Pan-Tompkins
%
%  important: firmware-ul (ecg_sensor.h) foloseste pragul clasic Pan-Tompkins
%  cu doua estimari exponentiale (SPKI / NPKI), nu un simplu EMA. Acest script
%  reproduce exact logica din firmware:
%     daca peak este semnal:  SPKI = 0.125*peak + 0.875*SPKI
%     daca peak este zgomot:  NPKI = 0.125*peak + 0.875*NPKI
%     prag:                   T_I1 = NPKI + 0.25*(SPKI - NPKI)
%     refractar fiziologic:   200 ms ; interval RR valid: 250-2000 ms
%
%  Caracter: proof of concept pe semnal sintetic cu amplitudine R
%  descrescatoare, pentru a evidentia adaptarea pragului. Rulare: Run in MATLAB.
%
clear; clc; close all;
rng(7);

Fs = 250;

% Coeficienti filtru (config.h)
HP_B=[0.99115452,-1.98230904,0.99115452]; HP_A=[1,-1.98223079,0.98238576];
LP_B=[0.14531600, 0.29063100,0.14531600]; LP_A=[1,-0.67106700,0.25234400];
NF_B=[0.97948100,-0.60531400,0.97948100]; NF_A=[1,-0.60531400,0.95896200];

% ECG sintetic 20 s la 72 BPM, cu amplitudine R care scade in timp
% (simuleaza atenuarea progresiva a semnalului; evidentiaza adaptarea pragului)
t = (0:1/Fs:20-1/Fs)';  N = numel(t);
rr = round(Fs*60/72);   ecg = zeros(N,1);  ref = [];
for k = 0:floor(N/rr)-1
    i0 = k*rr; ref(end+1)=i0+round(0.25*Fs); %#ok<SAGROW>
    amp = 1.0 - 0.6*(i0/N);                       % R scade de la 1.0 la ~0.4
    ecg = ecg + amp     *exp(-((t-(i0+round(0.25*Fs))/Fs).^2)/(2*0.015^2)) ...
              - 0.20*amp*exp(-((t-(i0+round(0.28*Fs))/Fs).^2)/(2*0.015^2)) ...
              + 0.30*amp*exp(-((t-(i0+round(0.45*Fs))/Fs).^2)/(2*0.06^2));
end
ecg = ecg + 0.04*randn(N,1) + 0.30*sin(2*pi*50*t);

% Front-end Pan-Tompkins (identic firmware)
x = filter(HP_B,HP_A,ecg); x = filter(LP_B,LP_A,x); x = filter(NF_B,NF_A,x);
d = filter((1/8)*[1 2 0 -2 -1],1,x);     % derivata 5 puncte
sq = d.^2;
W  = round(0.15*Fs);                     % fereastra 150 ms = 37 esantioane
mwi = filter(ones(1,W)/W,1,sq);

% Adaptare prag SPKI / NPKI (exact ca in ecg_sensor.h)
SPKI = max(mwi(1:2*Fs)); NPKI = mean(mwi(1:2*Fs));
T = NPKI + 0.25*(SPKI-NPKI);
thr = zeros(N,1); spki_h = zeros(N,1); npki_h = zeros(N,1);
tLast = -Inf; det = [];
refrac = round(0.2*Fs);                  % 200 ms
for i = 2:N-1
    if mwi(i)>mwi(i-1) && mwi(i)>=mwi(i+1)      % maxim local
        peak = mwi(i);
        if peak > T && (i - tLast) > refrac
            SPKI = 0.125*peak + 0.875*SPKI;     % R-peak (semnal)
            det(end+1) = i; tLast = i;          %#ok<SAGROW>
        else
            NPKI = 0.125*peak + 0.875*NPKI;     % zgomot
        end
        T = NPKI + 0.25*(SPKI-NPKI);
    end
    thr(i)=T; spki_h(i)=SPKI; npki_h(i)=NPKI;
end
thr(1)=thr(2); thr(end)=thr(end-1);

% Evaluare (toleranta 150 ms)
tol = round(0.15*Fs); TP=0; FP=0; mref=false(size(ref));
for dpk = det
    [m,ix]=min(abs(dpk-ref));
    if m<=tol && ~mref(ix), TP=TP+1; mref(ix)=true; else, FP=FP+1; end
end
FN = sum(~mref);
Se = TP/(TP+FN)*100; PPV = TP/(TP+FP)*100;
fprintf(' PRAG ADAPTIV SPKI/NPKI (simulare) \n');
fprintf('  Batai reale: %d | Detectate: %d | TP=%d FP=%d FN=%d\n', numel(ref),numel(det),TP,FP,FN);
fprintf('  Sensibilitate Se = %.1f%% | Predictivitate +P = %.1f%%\n', Se,PPV);
fprintf('  Refractar 200 ms -> BPM max teoretic 300; RR valid 250-2000 ms (30-240 BPM)\n');

% Figura: MWI + prag adaptiv
figure('Name','Prag adaptiv SPKI/NPKI','Position',[60 60 1100 600]);
plot(t,mwi,'b','LineWidth',0.7); hold on;
plot(t,thr,'r-','LineWidth',1.8);
plot(t,spki_h,'g--','LineWidth',1.0);
plot(t,npki_h,'m--','LineWidth',1.0);
plot(t(det),mwi(det),'r*','MarkerSize',8);
grid on; xlabel('Timp (s)'); ylabel('MWI (u.a.)');
legend('MWI','Prag T_{I1}','SPKI','NPKI','R-peak detectat','Location','northeast');
title(sprintf('Fig. Prag adaptiv Pan-Tompkins (Se=%.1f%%, +P=%.1f%%): pragul coboara cu amplitudinea semnalului',Se,PPV));
