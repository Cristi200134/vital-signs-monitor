%
%  10_alarme_false.m
% VitalMonitor. Rata teoretica a alarmelor false vs pragurile din config.h
%
%  Caracter: simulare statistica (proof of concept). Pentru fiecare parametru
%  se presupune o distributie normala a valorilor la un pacient stabil si se
%  calculeaza probabilitatea ca o valoare normala sa cada in afara pragurilor.
%
%  nota onestitate: tinta de < 1% este un criteriu propriu de proiectare.
%  Standardul IEC 60601-1-8 reglementeaza prioritatile, culorile si semnalele
%  sonore ale alarmelor, nu un procent fix de alarme false. Compararea reala
%  cu cerintele standardului se face in capitolul de validare.
%
%  Rulare: Run in MATLAB.
%
clear; clc; close all;

% nume, mu, sigma, prag_low, prag_high, unitate (praguri din config.h)
P = {
 'HR',   72,  8,   55,  110, 'BPM';
 'SpO2', 98,  1.5, 94,  Inf, '%';
 'Temp', 36.8,0.4, 35,  38.5,'C';
 'Resp', 15,  3,   10,  25,  'rpm';
};
target = 1.0;                          % tinta proprie (%)

fprintf(' RATA ALARME FALSE (simulare, tinta proprie < %.0f%%) \n', target);
fprintf('%-6s %-6s %-6s %-9s %-9s %-12s %-6s\n','Param','Mean','Sigma','Prag LOW','Prag HIGH','P(fals)','Stare');
figure('Name','Alarme false','Position',[60 60 1050 700]);
for i = 1:size(P,1)
    mu=P{i,2}; sg=P{i,3}; lo=P{i,4}; hi=P{i,5}; u=P{i,6};
    p_lo = normcdf(lo,mu,sg);
    p_hi = 1 - normcdf(hi,mu,sg);
    if ~isfinite(hi), p_hi = 0; end
    pfa = (p_lo+p_hi)*100;
    fprintf('%-6s %-6.1f %-6.1f %-9.1f %-9.1f %-11.3f%% %-6s\n', ...
        P{i,1},mu,sg,lo,hi,pfa, ternary(pfa<target,'OK','PESTE'));

    x = mu-5*sg:0.01:mu+5*sg; pdf = normpdf(x,mu,sg);
    subplot(2,2,i);
    fill([x fliplr(x)],[pdf zeros(size(pdf))],[.6 .8 1],'EdgeColor','none'); hold on;
    il = x<lo; if any(il); fill([x(il) fliplr(x(il))],[pdf(il) zeros(1,sum(il))],'r','FaceAlpha',.6); end
    if isfinite(hi); ih = x>hi; if any(ih); fill([x(ih) fliplr(x(ih))],[pdf(ih) zeros(1,sum(ih))],'r','FaceAlpha',.6); end; end
    xline(lo,'--r'); if isfinite(hi); xline(hi,'--r'); end
    grid on; xlabel(u); title(sprintf('%s  P(fals)=%.3f%%',P{i,1},pfa));
end
sgtitle('Fig. Distributii fiziologice vs praguri de alarma (simulare)');

fprintf('\n  HR si Resp depasesc tinta de 1%% cu pragurile actuale.\n');
fprintf('  Praguri sugerate pentru P<1%% pe fiecare parte (z=2.576):\n');
fprintf('    HR:   %.0f - %.0f BPM (actual 55-110)\n', 72-2.576*8, 72+2.576*8);
fprintf('    Resp: %.0f - %.0f rpm (actual 10-25)\n', 15-2.576*3, 15+2.576*3);
fprintf('  Pragurile clinice raman justificate medical; discutia se face in teza.\n');

function s = ternary(c,a,b); if c; s=a; else; s=b; end; end
