% fitstomatlab r1.match.fits"[col fieldobjs; codeerr;noverlap;nconflict;nfield;nindex; logodds;timeused]" > r1.m

% Total number of fields.
N = 9809;

T='9809 high-quality r-band SDSS fields, index 202';

subplot(111);

NF=max(fieldobjs')';
trunchist(NF, min(NF), 1, 60, 0, 1);
xlabel('Number of field objects examined');
ylabel('Number of Fields Solved');
title(T);
a=axis;
a(1) = 0;
axis(a);
print -dpng fig1.png

input('Next...\n');

CE = sqrt(codeerr);
trunchist(CE*10^3, 0, 0.1, 10, 0, 1);
xlabel('Code Error   ( x 10^{-3} )');
ylabel('Number of Fields Solved');
title(T);
print -dpng fig2.png

input('Next...\n');

hist(logodds / log(10), 50)
xlabel('Log_{10} Odds of Solution');
ylabel('Number of Fields Solved');
title(T);
print -dpng fig3.png

input('Next...\n');

hist(nindex, max(nindex) - min(nindex) + 1);
xlabel('Number of Index Stars in Field');
ylabel('Number of Fields Solved');
title(T);
print -dpng fig4.png

input('Next...\n');

hist(nfield, max(nfield)-min(nfield)+1)
xlabel('Number of index object at which max odds ratio was found');
ylabel('Number of Fields Solved');
title(T);
print -dpng fig5.png

input('Next...\n');

ND=nfield - noverlap - nconflict;

pct = [0:0.02:1];

subplot(2,1,1);

PO=noverlap ./ nfield;
H=histc(PO, pct);
bar(pct, H, 'histc');
a=axis;
a([1:2]) = [0,1];
axis(a);
xlabel('Proportion of Field Objects that Matched');
title(T);

subplot(2,1,2);

PD = ND ./ nfield;
H=histc(PD, pct);
bar(pct, H, 'histc');
a=axis;
a([1:2]) = [0,1];
axis(a);
xlabel('Proportion of Field Objects that were Distractors');
ylabel('Number of Fields Solved');

print -dpng fig8.png

subplot(111);

input('Next...\n');

tm = sort(timeused);
tmin = min(timeused(find(timeused>0)));
tmin = max(tmin, 1e-3);
tm = max(tmin/2, tm);
semilogy(100.0 * [1:length(tm)]./N, tm);
xlabel('Percentage of Fields Solved');
ylabel('CPU Time (s)');
title(T);

input('Next...\n');

semilogx(tm, 100.0 * [1:length(tm)]./N);
xlabel('CPU Time (s)');
ylabel('Percentage of Fields Solved');
title(T);

input('Next...\n');

semilogx(tm, 100.0 * (1 - [1:length(tm)]./N));
xlabel('CPU Time (s)');
ylabel('Percentage of Fields Unsolved');
title(T);

input('Next...\n');

loglog(tm, 100.0 * (1 - [1:length(tm)]./N));
xlabel('CPU Time (s)');
ylabel('Percentage of Fields Unsolved');
set(gca, 'XTick', 10.^[-3:2]);
set(gca, 'XTickLabel', {'1 ms', '10 ms', '100 ms', '1 s', '10 s', '100 s'});
set(gca, 'YTick', 10.^[-1:2]);
set(gca, 'YTickLabel', {'0.1%', '1%', '10%', '100%'});
a=axis;
a(2) = 20;
axis(a);
title(T);
print -dpng fig9.png

