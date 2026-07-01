% 02_qrs_mitbih.m  (MIT-BIH, fara WFDB Toolbox)
% VitalMonitor. Validarea detectiei de varfuri R fata de baza MIT-BIH Arrhythmia.
%
% Detectorul reproduce logica din firmware (ecg_sensor.h): filtrare HP/LP/notch,
% derivata pe 5 puncte, ridicare la patrat, fereastra mobila de 150 ms, prag
% adaptiv SPKI/NPKI si refractar de 200 ms. Este varianta simplificata, fara
% searchback si fara praguri pe semnalul filtrat.
%
% Cum se ruleaza (MATLAB Online sau desktop). Nu cere WFDB Toolbox: fisierele
% MIT-BIH se descarca cu websave si se citesc cu un parser propriu, ca sa ocolim
% bug-ul cu spatiul din calea /MATLAB Drive. Are nevoie de internet si, pentru
% resample, de Signal Processing Toolbox; daca resample lipseste, foloseste
% interpolare. Apesi Run. Fara internet, ruleaza varianta sintetica.
%
% Verificare rapida a citirii: record 100 are ~2273 batai, record 234 ~2753,
% record 119 ~1987. Daca numarul iese aproape, citirea e corecta.
clear; clc; close all; rng(11);
Fs = 250;
HP_B=[0.99115452,-1.98230904,0.99115452]; HP_A=[1,-1.98223079,0.98238576];
LP_B=[0.14531600, 0.29063100,0.14531600]; LP_A=[1,-0.67106700,0.25234400];
NF_B=[0.97948100,-0.60531400,0.97948100]; NF_A=[1,-0.60531400,0.95896200];
C = {HP_B,HP_A,LP_B,LP_A,NF_B,NF_A};
tol = round(0.15*Fs);

base = 'https://physionet.org/files/mitdb/1.0.0/';
% subset reprezentativ, fara inregistrarile cu stimulator (102,104,107,217).
% Pentru un test rapid lasa 3-4 inregistrari; pentru benchmark complet, extinde lista.
recs = {'100','101','103','105','106','108','109','111','113','115', ...
        '116','117','119','121','122','123','200','201','203','205', ...
        '208','210','213','215','220','223','228','230','233','234'};
beatCodes = [1 2 3 4 5 6 7 8 9 10 11 12 13 25 30 34 35 38 41]; % adnotari de tip bataie
wo = weboptions('Timeout',60);

% test de internet pe prima inregistrare
haveNet = true;
try
    websave([recs{1} '.hea'], [base recs{1} '.hea'], wo);
catch
    haveNet = false;
end

if haveNet
    fprintf('Validare MIT-BIH (%d inregistrari, citire proprie)\n', numel(recs));
    fprintf('%-6s %7s %5s %4s %4s %7s %7s %7s\n','Rec','Batai','TP','FP','FN','Se%','+P%','F1%');
    TPg=0; FPg=0; FNg=0; nrec=0; firstSig=[];
    for k=1:numel(recs)
        rec=recs{k};
        try
            [ecg,Fso,beats] = read_mitbih(rec, base, wo, beatCodes);
        catch e
            fprintf('  %-6s  sarit (%s)\n', rec, e.message); continue;
        end
        ecg250 = to250(ecg, Fso, Fs);
        ref = round(beats * Fs / Fso); ref = ref(ref>0 & ref<=numel(ecg250));
        det = detect_qrs(ecg250, Fs, C);
        [TP,FP,FN] = matchbeats(det, ref, tol);
        Se=100*TP/max(TP+FN,1); PP=100*TP/max(TP+FP,1); F1=2*Se*PP/max(Se+PP,eps);
        TPg=TPg+TP; FPg=FPg+FP; FNg=FNg+FN; nrec=nrec+1;
        fprintf('%-6s %7d %5d %4d %4d %7.2f %7.2f %7.2f\n',rec,numel(ref),TP,FP,FN,Se,PP,F1);
        if isempty(firstSig), firstSig=struct('ecg',ecg250,'ref',ref,'det',det,'rec',rec); end
    end
    Seg=100*TPg/max(TPg+FNg,1); PPg=100*TPg/max(TPg+FPg,1); F1g=2*Seg*PPg/max(Seg+PPg,eps);
    fprintf('---------------------------------------------------------------\n');
    fprintf('AGREGAT (pooled, %d inreg.):  TP=%d  FP=%d  FN=%d\n', nrec,TPg,FPg,FNg);
    fprintf('  Sensibilitate Se = %.2f %%\n', Seg);
    fprintf('  Predictivitate +P = %.2f %%\n', PPg);
    fprintf('  Scor F1          = %.2f %%\n', F1g);
    fprintf('  (Referinta Pan-Tompkins 1985 pe MIT-BIH: Se=99.3%%, +P=99.9%%)\n');

    if ~isempty(firstSig)
        ecg=firstSig.ecg; ref=firstSig.ref; det=firstSig.det;
        t=(0:numel(ecg)-1)/Fs; seg=1:min(10*Fs,numel(ecg));
        bg=[0.12 0.12 0.12];   % fundal inchis (ca ECG-ul alb sa fie vizibil)
        fig=figure('Name','Validare MIT-BIH','Position',[50 50 1200 420],'Color',bg);
        ax=axes('Parent',fig); hold(ax,'on');
        plot(ax,t(seg),ecg(seg),'w','LineWidth',1);                 % ECG alb
        r=ref(ref<=max(seg)); d=det(det<=max(seg));
        plot(ax,t(r),ecg(r),'o','Color',[0.2 1 0.4],'MarkerSize',8,'LineWidth',1.3);
        plot(ax,t(d),ecg(d),'*','Color',[0.35 0.65 1],'MarkerSize',8);
        grid(ax,'on');
        set(ax,'Color',bg,'XColor','w','YColor','w','GridColor',[0.6 0.6 0.6],'GridAlpha',0.4);
        xlim(ax,[0 10]); xlabel(ax,'Timp [s]'); ylabel(ax,'ECG [u.a.]');
        lg=legend(ax,'ECG','R de referinta (MIT-BIH)','R detectat','Location','best');
        set(lg,'TextColor','w','Color',[0.2 0.2 0.2]);
        ti=title(ax,sprintf('Detectie QRS pe MIT-BIH (rec %s); agregat: Se=%.2f%%, +P=%.2f%%, F1=%.2f%%',...
              firstSig.rec, Seg,PPg,F1g)); set(ti,'Color','w');
        set(fig,'InvertHardcopy','off');   % pastreaza fundalul inchis la salvare
        saveas(fig,'Fig_validare_mitbih.png');
        fprintf('\nFigura (ecran, fundal inchis) salvata: Fig_validare_mitbih.png\n');

        % varianta pentru tipar: fundal alb, ECG negru, ca restul figurilor din lucrare
        fig2=figure('Name','Validare MIT-BIH (tipar)','Position',[50 50 1200 420],'Color','w');
        ax2=axes('Parent',fig2); hold(ax2,'on');
        plot(ax2,t(seg),ecg(seg),'k','LineWidth',1);                 % ECG negru
        plot(ax2,t(r),ecg(r),'o','Color',[0 0.5 0],'MarkerSize',8,'LineWidth',1.3);
        plot(ax2,t(d),ecg(d),'*','Color',[0 0.45 0.85],'MarkerSize',8);
        grid(ax2,'on'); xlim(ax2,[0 10]); xlabel(ax2,'Timp [s]'); ylabel(ax2,'ECG [u.a.]');
        legend(ax2,'ECG','R de referinta (MIT-BIH)','R detectat','Location','best');
        title(ax2,sprintf('Detectie QRS pe MIT-BIH (rec %s); agregat: Se=%.2f%%, +P=%.2f%%, F1=%.2f%%',...
              firstSig.rec, Seg,PPg,F1g));
        saveas(fig2,'Fig_validare_mitbih_print.png');
        fprintf('Figura (tipar, fundal alb) salvata: Fig_validare_mitbih_print.png\n');
    end
else
    fprintf('Fara internet -> mod SINTETIC (proof of concept).\n');
    dur=300; t=(0:1/Fs:dur-1/Fs)'; N=numel(t); bpm=72+8*sin(2*pi*t/20);
    ecg=zeros(N,1); ref=[]; i=round(0.2*Fs);
    while i<N-round(1.2*Fs)
        rr=round(Fs*60/bpm(min(i,N))); ref(end+1)=i; %#ok<SAGROW>
        for kk=max(1,i-round(0.2*Fs)):min(N,i+round(0.5*Fs))
            dt=(kk-i)/Fs;
            ecg(kk)=ecg(kk)+0.15*exp(-((dt+0.08)^2)/(2*0.04^2))-0.10*exp(-((dt+0.02)^2)/(2*0.01^2)) ...
                  +1.00*exp(-(dt^2)/(2*0.015^2))-0.25*exp(-((dt-0.03)^2)/(2*0.015^2)) ...
                  +0.30*exp(-((dt-0.20)^2)/(2*0.06^2));
        end
        i=i+rr;
    end
    ref=ref(:); ecg=ecg+0.40*sin(2*pi*50*t)+0.60*cumsum(randn(N,1))/500+0.08*randn(N,1);
    det=detect_qrs(ecg,Fs,C); [TP,FP,FN]=matchbeats(det,ref,tol);
    Se=100*TP/max(TP+FN,1); PP=100*TP/max(TP+FP,1); F1=2*Se*PP/max(Se+PP,eps);
    fprintf('SINTETIC: batai=%d TP=%d FP=%d FN=%d  Se=%.2f%%  +P=%.2f%%  F1=%.2f%%\n',numel(ref),TP,FP,FN,Se,PP,F1);
end

% functii locale
function [ecg,Fso,beats] = read_mitbih(rec, base, wo, beatCodes)
    exts={'.hea','.dat','.atr'};
    for e3=1:3
        f=[rec exts{e3}];
        if exist(f,'file')~=2, websave(f,[base rec exts{e3}],wo); end
    end
    % header
    L=regexp(fileread([rec '.hea']),'[\r\n]+','split'); L=L(~cellfun(@isempty,L));
    t1=strsplit(strtrim(L{1})); nsig=str2double(t1{2}); Fso=str2double(t1{3});
    t2=strsplit(strtrim(L{2}));
    if ~strncmp(t2{2},'212',3), error('format %s neacceptat (doar 212)', t2{2}); end
    % semnal (format 212, canalul 0)
    fid=fopen([rec '.dat'],'rb'); raw=fread(fid,inf,'*uint8'); fclose(fid);
    ng=floor(numel(raw)/3); raw=double(raw(1:3*ng));
    b1=raw(1:3:end); b2=raw(2:3:end); b3=raw(3:3:end);
    s1=b1+mod(b2,16)*256;  s1(s1>2047)=s1(s1>2047)-4096;   % canal 0
    ecg=s1;  %#ok<NASGU>  (canalul 0 al MIT-BIH; pentru detectie nu conteaza scalarea)
    if nsig<2, ecg=s1; end
    % adnotari (.atr)
    fid=fopen([rec '.atr'],'r','l'); D=fread(fid,inf,'uint16=>double'); fclose(fid);
    i=1; tabs=0; samp=[]; cod=[];
    while i<=numel(D)
        A=D(i); i=i+1; typ=floor(A/1024); I=mod(A,1024);
        if typ==0 && I==0, break; end
        if typ==59                         % SKIP: interval pe 32 biti, apoi tipul real
            if i+1>numel(D), break; end
            I=D(i)*65536+D(i+1); i=i+2;
            if i>numel(D), break; end
            A2=D(i); i=i+1; typ=floor(A2/1024);
            tabs=tabs+I; samp(end+1)=tabs; cod(end+1)=typ; %#ok<AGROW>
        elseif typ==60 || typ==61 || typ==62   % NUM/SUB/CHN: modificatori, fara timp
            % nimic
        elseif typ==63                          % AUX: I octeti de text, sarim
            i=i+ceil(I/2);
        else
            tabs=tabs+I; samp(end+1)=tabs; cod(end+1)=typ; %#ok<AGROW>
        end
    end
    beats = samp(ismember(cod, beatCodes));
    beats = beats(:);
    ecg = s1;
end

function y = to250(x, Fso, Fs)
    if exist('resample','file')==2
        y = resample(double(x), Fs, round(Fso));
    else
        tin=(0:numel(x)-1)/Fso; tout=(0:1/Fs:tin(end))';
        y = interp1(tin, double(x), tout, 'linear');
    end
end

function det = detect_qrs(ecg, Fs, C)
    ecg = ecg - mean(ecg);   % centrare: semnalul real de la AD8232 e centrat, nu pe ~1000 ADC
    [HP_B,HP_A,LP_B,LP_A,NF_B,NF_A]=C{:};
    x=filter(HP_B,HP_A,ecg); x=filter(LP_B,LP_A,x); x=filter(NF_B,NF_A,x);
    d=filter((1/8)*[1 2 0 -2 -1],1,x); sq=d.^2;
    W=round(0.15*Fs); mwi=filter(ones(1,W)/W,1,sq); delay=round((5+W)/2);
    n=numel(mwi); sk=round(1.0*Fs); ini=sk+1:min(sk+2*Fs,n);  % sare peste tranzitoriul de filtru
    SPKI=max(mwi(ini)); NPKI=mean(mwi(ini));
    T=NPKI+0.25*(SPKI-NPKI); refrac=round(0.2*Fs); last=-Inf; det=[];
    for i=2:n-1
        if mwi(i)>mwi(i-1) && mwi(i)>=mwi(i+1)
            pk=mwi(i);
            if pk>T && (i-last)>refrac
                SPKI=0.125*pk+0.875*SPKI; det(end+1)=i-delay; last=i; %#ok<AGROW>
            else
                NPKI=0.125*pk+0.875*NPKI;
            end
            T=NPKI+0.25*(SPKI-NPKI);
        end
    end
    det=sort(det(det>0))';
end

function [TP,FP,FN]=matchbeats(det,ref,tol)
    mref=false(numel(ref),1); mdet=false(numel(det),1);
    for j=1:numel(det)
        if isempty(ref), break; end
        [m,ix]=min(abs(det(j)-ref));
        if m<=tol && ~mref(ix), mref(ix)=true; mdet(j)=true; end
    end
    TP=sum(mdet); FP=sum(~mdet); FN=sum(~mref);
end
