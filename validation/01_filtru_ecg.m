%
%  01_filtru_ecg.m
% VitalMonitor. Validare prin simulare a filtrului Butterworth cascadat
%
%  Caracter: proof of concept. Coeficientii sunt identici cu cei din
%  firmware (config.h). Semnalul de test este sintetic, generat controlat;
%  nu este o inregistrare reala. Compararea cu standardele reale (banda de
%  monitorizare, interferenta de retea) se discuta in capitolul de validare.
%
%  Ce arata scriptul:
%   1. Raspunsul in frecventa al celor 3 sectiuni si al cascadei
%   2. Stabilitatea (diagrama pol-zero, toti polii in cercul unitate)
%   3. Test pe ECG sintetic + zgomot (50 Hz, deriva DC, EMG)
%   4. SNR inainte vs dupa filtrare (pe semnalul sintetic)
%   5. Atenuarea la frecvente critice
%
%  Rulare: deschide in MATLAB si apasa Run.
%
clear; clc; close all;
rng(42);                         % reproductibilitate

Fs = 250;                        % SPS, frecventa de esantionare ECG

% Coeficienti identici cu config.h (firmware final)
% HP Butterworth ord.2, fc = 0.5 Hz ; LP ord.2, fc = 40 Hz ; Notch 50 Hz Q=30
HP_B = [ 0.99115452, -1.98230904,  0.99115452];
HP_A = [ 1.00000000, -1.98223079,  0.98238576];
LP_B = [ 0.14531600,  0.29063100,  0.14531600];
LP_A = [ 1.00000000, -0.67106700,  0.25234400];
NF_B = [ 0.97948100, -0.60531400,  0.97948100];
NF_A = [ 1.00000000, -0.60531400,  0.95896200];

% Cascada = produsul functiilor de transfer (convolutia coeficientilor)
B_tot = conv(conv(HP_B, LP_B), NF_B);
A_tot = conv(conv(HP_A, LP_A), NF_A);

% 1. Raspuns in frecventa
[H_HP, f] = freqz(HP_B, HP_A, 4096, Fs);
[H_LP, ~] = freqz(LP_B, LP_A, 4096, Fs);
[H_NF, ~] = freqz(NF_B, NF_A, 4096, Fs);
[H_tot,~] = freqz(B_tot, A_tot, 4096, Fs);

figure('Name','Raspuns in frecventa filtru ECG','Position',[50 50 1100 650]);
subplot(2,2,1); plot(f,20*log10(abs(H_HP)),'b','LineWidth',1.5);
  xline(0.5,'r--','0.5 Hz'); grid on; xlim([0 125]); ylim([-80 5]);
  title('HP Butterworth ord.2 (fc = 0.5 Hz)'); xlabel('Hz'); ylabel('dB');
subplot(2,2,2); plot(f,20*log10(abs(H_LP)),'g','LineWidth',1.5);
  xline(40,'r--','40 Hz'); grid on; xlim([0 125]); ylim([-80 5]);
  title('LP Butterworth ord.2 (fc = 40 Hz)'); xlabel('Hz'); ylabel('dB');
subplot(2,2,3); plot(f,20*log10(abs(H_NF)),'m','LineWidth',1.5);
  xline(50,'r--','50 Hz'); grid on; xlim([0 125]); ylim([-80 5]);
  title('Notch IIR (f0 = 50 Hz, Q = 30)'); xlabel('Hz'); ylabel('dB');
subplot(2,2,4); plot(f,20*log10(abs(H_tot)),'k','LineWidth',2);
  xline(0.5,'b--'); xline(40,'g--'); xline(50,'r--'); grid on;
  xlim([0 125]); ylim([-80 5]);
  title('Cascada HP + LP + Notch'); xlabel('Hz'); ylabel('dB');
sgtitle('Fig. Raspunsul in frecventa al filtrului Butterworth cascadat');

% 2. Stabilitate (pol-zero)
figure('Name','Pol-zero','Position',[50 50 700 600]);
zplane(B_tot, A_tot); grid on;
title('Fig. Diagrama pol-zero (toti polii in cercul unitate = STABIL)');

poles = roots(A_tot);
fprintf(' STABILITATE \n');
for i = 1:numel(poles)
    fprintf('  |p%d| = %.6f  %s\n', i, abs(poles(i)), ...
        ternary(abs(poles(i))<1,'STABIL','INSTABIL'));
end
fprintf('  Toti polii < 1 -> sistem STABIL BIBO.\n\n');

% 3. ECG sintetic + zgomot
t = (0:1/Fs:10-1/Fs)';  N = numel(t);
bpm = 72;  rr = round(Fs*60/bpm);
ecg_clean = zeros(N,1);
for k = 0:floor(N/rr)-1
    i0 = k*rr;
    ecg_clean = ecg_clean + 0.15*gpulse(t,(i0+round(0.16*Fs))/Fs,0.04) ...  % P
                          - 0.10*gpulse(t,(i0+round(0.22*Fs))/Fs,0.01) ...  % Q
                          + 1.00*gpulse(t,(i0+round(0.25*Fs))/Fs,0.015) ... % R
                          - 0.25*gpulse(t,(i0+round(0.28*Fs))/Fs,0.015) ... % S
                          + 0.30*gpulse(t,(i0+round(0.45*Fs))/Fs,0.06);     % T
end
% Zgomot dominat de tipurile pe care filtrul le tinteste: retea 50 Hz,
% deriva de baza, plus o componenta EMG mica (banda larga, partial inevitabila).
noise = 0.80*sin(2*pi*50*t) + 1.00*exp(-t/4)+0.40 + 0.04*randn(N,1) + 0.15*sin(2*pi*120*t);
ecg_noisy = ecg_clean + noise;

ecg_f = filter(HP_B,HP_A, ecg_noisy);
ecg_f = filter(LP_B,LP_A, ecg_f);
ecg_f = filter(NF_B,NF_A, ecg_f);

% Raspunsul filtrului pe semnalul curat (referinta pentru zgomotul ramas)
ecg_fc = filter(HP_B,HP_A, ecg_clean);
ecg_fc = filter(LP_B,LP_A, ecg_fc);
ecg_fc = filter(NF_B,NF_A, ecg_fc);

% 4. SNR pe segmentul 2-8 s (dupa tranzient)
% SNR_out compara zgomotul ramas (filtrat_zgomotos - filtrat_curat); astfel se
% izoleaza atenuarea zgomotului de modelarea intentionata a semnalului de filtru.
seg = round(2*Fs):round(8*Fs);
SNR_in  = 10*log10(mean(ecg_clean(seg).^2)/mean((ecg_noisy(seg)-ecg_clean(seg)).^2));
SNR_out = 10*log10(mean(ecg_fc(seg).^2)   /mean((ecg_f(seg)    -ecg_fc(seg)).^2));
fprintf(' SNR pe semnal sintetic \n');
fprintf('  SNR inainte de filtrare: %+.2f dB\n', SNR_in);
fprintf('  SNR dupa filtrare:       %+.2f dB\n', SNR_out);
fprintf('  Imbunatatire:            %+.2f dB\n\n', SNR_out-SNR_in);

figure('Name','ECG brut vs filtrat','Position',[50 50 1200 600]);
subplot(3,1,1); plot(t(seg),ecg_clean(seg),'g'); grid on; xlim([2 8]);
  title('ECG sintetic de referinta'); ylabel('u.a.');
subplot(3,1,2); plot(t(seg),ecg_noisy(seg),'r'); grid on; xlim([2 8]);
  title(sprintf('ECG cu zgomot (SNR = %.1f dB)',SNR_in)); ylabel('u.a.');
subplot(3,1,3); plot(t(seg),ecg_f(seg),'b','LineWidth',1.2); grid on; xlim([2 8]);
  title(sprintf('ECG filtrat (SNR = %.1f dB, +%.1f dB)',SNR_out,SNR_out-SNR_in));
  xlabel('Timp (s)'); ylabel('u.a.');
sgtitle('Fig. Performanta filtrului pe semnal sintetic (proof of concept)');

% 5. Atenuare la frecvente critice
fprintf(' ATENUARE CASCADA \n');
fc = [0.1 0.5 5 10 20 40 50 60 100];
for fq = fc
    [~,ix] = min(abs(f-fq));
    fprintf('  %6.1f Hz : %+7.1f dB\n', fq, 20*log10(abs(H_tot(ix))));
end
fprintf('  (banda utila 0.5-40 Hz aproape plata; 50 Hz puternic atenuat)\n');

% functii auxiliare
function y = gpulse(t,t0,s); y = exp(-((t-t0).^2)/(2*s^2)); end
function s = ternary(c,a,b); if c; s=a; else; s=b; end; end
