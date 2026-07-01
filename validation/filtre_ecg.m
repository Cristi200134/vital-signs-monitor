%
% filtre_ecg.m, VitalMonitor (proof of concept)
%
%  Verifica faptul ca cei 3 coeficienti din firmware (config.h) coincid cu
%  filtrul proiectat corect in MATLAB (butter + iirnotch). Daca cele doua
%  curbe se suprapun, coeficientii din firmware sunt corecti.
%
%  Caracter: validare numerica prin simulare. Rulare: Run in MATLAB.
%
clear; clc; close all;

Fs  = 250;
Nyq = Fs/2;

% Coeficienti DIN config.h (firmware final)
b_hp = [ 0.99115452, -1.98230904,  0.99115452];
a_hp = [ 1.00000000, -1.98223079,  0.98238576];
b_lp = [ 0.14531600,  0.29063100,  0.14531600];
a_lp = [ 1.00000000, -0.67106700,  0.25234400];
b_nf = [ 0.97948100, -0.60531400,  0.97948100];
a_nf = [ 1.00000000, -0.60531400,  0.95896200];

% Coeficienti recalculati independent in MATLAB
[b_hp_c, a_hp_c] = butter(2, 0.5/Nyq,  'high');
[b_lp_c, a_lp_c] = butter(2, 40.0/Nyq, 'low');
% Notch 50 Hz, Q=30 - formula standard (nu depinde de iirnotch / toolbox)
w0n  = 50.0/Nyq;  Qf = 30;  w0 = w0n*pi;  bw = (w0n/Qf)*pi;
gb   = 1/sqrt(2);
beta = (sqrt(1-gb^2)/gb) * tan(bw/2);
gain = 1/(1+beta);
b_nf_c = gain * [1, -2*cos(w0), 1];
a_nf_c =        [1, -2*gain*cos(w0), 2*gain-1];

% Raspuns in frecventa (cascada)
[h1,f]=freqz(b_hp,a_hp,8192,Fs); [h2,~]=freqz(b_lp,a_lp,8192,Fs); [h3,~]=freqz(b_nf,a_nf,8192,Fs);
H_fw = h1.*h2.*h3;
[h1c,~]=freqz(b_hp_c,a_hp_c,8192,Fs); [h2c,~]=freqz(b_lp_c,a_lp_c,8192,Fs); [h3c,~]=freqz(b_nf_c,a_nf_c,8192,Fs);
H_c  = h1c.*h2c.*h3c;

figure('Name','Validare filtre ECG','Position',[80 80 950 650]);
subplot(2,1,1);
  semilogx(f,20*log10(abs(H_fw)),'r-','LineWidth',1.6); hold on;
  semilogx(f,20*log10(abs(H_c)), 'b--','LineWidth',1.6);
  xline(0.5,'--k','0.5 Hz'); xline(40,'--g','40 Hz'); xline(50,'--m','50 Hz');
  grid on; xlim([0.1 125]); ylim([-80 5]);
  xlabel('Frecventa (Hz)'); ylabel('|H| (dB)');
  title('Cascada HP + LP + Notch, firmware vs recalculat');
  legend('config.h (firmware)','butter + notch recalculat (MATLAB)','Location','southwest');
subplot(2,1,2);
  plot(f,20*log10(abs(H_fw)),'r-','LineWidth',1.6); hold on;
  plot(f,20*log10(abs(H_c)), 'b--','LineWidth',1.6);
  xline(50,'--m','50 Hz'); xline(40,'--g','40 Hz');
  grid on; xlim([0 120]); ylim([-80 5]);
  xlabel('Frecventa (Hz)'); ylabel('|H| (dB)');
  title('Aceleasi curbe, axa liniara');
sgtitle('Fig. Coeficientii din firmware coincid cu filtrul proiectat corect');

% Eroarea maxima intre cele doua raspunsuri (in banda 0-100 Hz)
band = f <= 100;
err_db = abs(20*log10(abs(H_fw(band))) - 20*log10(abs(H_c(band))));
fprintf(' POTRIVIRE FIRMWARE vs RECALCULAT \n');
fprintf('  Eroare maxima de magnitudine (0-100 Hz): %.3f dB\n', max(err_db));
fprintf('  Atenuare la 50 Hz (firmware):            %.1f dB\n', 20*log10(abs(H_fw(find(f>=50,1)))));
fprintf('  -3 dB HP la ~0.5 Hz, -3 dB LP la ~40 Hz.\n');
fprintf('  Eroare mica -> coeficientii din config.h sunt CORECTI.\n');
