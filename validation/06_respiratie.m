%
%  06_respiratie.m
% VitalMonitor. Validare prin simulare a filtrului de respiratie
%
%  Reproduce exact logica din spo2_sensor.h (estimarea respiratiei din
%  modulatia PPG): doua filtre EMA in cascada, scaderea lor da un bandpass.
%     respFast = 0.9 *respFast + 0.1  *ir      (LP rapid, alpha=0.10)
%     respSlow = 0.998*respSlow + 0.002*respFast (LP lent, alpha=0.002)
%     respSig  = respFast - respSlow            (bandpass)
%  Rata efectiva = perioada task-ului SpO2 = 50 Hz.
%
%  Caracter: proof of concept pe semnal sintetic. Rulare: Run in MATLAB.
%
clear; clc; close all;
rng(5);

Fs = 50;                              % rata task SpO2 (20 ms)

a1 = 0.10;  a2 = 0.002;
b_f = a1; a_f = [1 -(1-a1)];
b_s = a2; a_s = [1 -(1-a2)];
[Hf,f] = freqz(b_f,a_f,4096,Fs);
[Hs,~] = freqz(b_s,a_s,4096,Fs);
Hbp = Hf - Hf.*Hs;                    % respFast - LP_slow(respFast)

tau_f = 1/(a1*Fs); tau_s = 1/(a2*Fs);
fc_f = 1/(2*pi*tau_f); fc_s = 1/(2*pi*tau_s);
fprintf(' BANDPASS RESPIRATIE (din spo2_sensor.h) \n');
fprintf('  LP rapid: alpha=%.3f  fc=%.3f Hz (taie HR > 1 Hz)\n', a1, fc_f);
fprintf('  LP lent:  alpha=%.3f  fc=%.4f Hz (taie drift DC)\n', a2, fc_s);
fprintf('  Banda rezultata: %.4f - %.3f Hz  =  %.1f - %.0f rpm\n', fc_s, fc_f, fc_s*60, fc_f*60);

% Test pe semnal sintetic: respiratie 15 rpm + componenta HR
t = (0:1/Fs:60)';
resp_hz = 15/60; hr_hz = 70/60;
ir = 100000 + 500*sin(2*pi*resp_hz*t) + 200*sin(2*pi*hr_hz*t) + 50*randn(size(t));
rF=ir(1); rS=ir(1); sig=zeros(size(t));   % init cu prima valoare IR (ca in firmware)
for i=1:numel(t)
    rF = 0.9*rF + 0.1*ir(i);
    rS = 0.998*rS + 0.002*rF;
    sig(i) = rF - rS;
end
warm = t>5;                           % tranzient scurt (initializarea corecta evita drift-ul DC)

figure('Name','Bandpass respiratie','Position',[60 60 1000 640]);
subplot(2,1,1);
  semilogx(f,20*log10(abs(Hf)),'b--','LineWidth',1.3); hold on;
  semilogx(f,20*log10(abs(Hs)),'g--','LineWidth',1.3);
  semilogx(f,20*log10(abs(Hbp)),'r-','LineWidth',2.2);
  xline(fc_s,'--k'); xline(fc_f,'--k');
  grid on; xlim([0.005 25]); ylim([-50 5]);
  xlabel('Hz'); ylabel('dB'); title('Raspuns: respFast, respSlow, bandpass');
  legend('LP rapid','LP lent','Bandpass','Location','south');
subplot(2,1,2);
  plot(t(warm),sig(warm),'r-','LineWidth',1.4); grid on;
  xlabel('Timp (s)'); ylabel('IR modulat (u.a.)');
  title(sprintf('Semnal respirator extras (%.0f rpm simulat)', resp_hz*60));
sgtitle('Fig. Estimarea respiratiei din PPG (proof of concept)');
