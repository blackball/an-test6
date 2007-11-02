clear;
ns;
subplot(111);
plot(fieldstars(:,1), fieldstars(:,2), 'bo', indexproj(:,1), indexproj(:,2), 'r.')
print('-dpng', 'FI.png')
input('Next...')

plot(L./H, E, 'b.')
xlabel('L/H');
ylabel('E');
print('-dpng', 'EvsR.png')

input('Next...')

%plot((L./H).^2, E.^2, 'b.')
%xlabel('(L/H)^2');
%ylabel('E^2');
%print('-dpng', 'E2vsR2.png')
%input('Next...')

R = L ./ H;
X = R.^2;
Y = E.^2;

IS = find(R < 1);
IM = find((R > 4.5) .* (R < 5.5));
IL = find(R > 8);
SE = sort(E(IL));
me = SE(round(0.99 * length(SE)));
%me = max(E);
nbins = 20;
bins = [0:nbins] * me / nbins;
subplot(3,1,1);
ES = histc(E(IS), bins);
bar(bins, ES, 'histc');
xlabel('E for points with R < 1');
subplot(3,1,2);
EM = histc(E(IM), bins);
bar(bins, EM, 'histc');
xlabel('E for points with R in [4.5, 5.5]');
subplot(3,1,3);
EL = histc(E(IL), bins);
bar(bins, EL, 'histc');
xlabel('E for points with R > 8');
subplot(111);
print('-dpng', 'Ehists.png')

input('Next...')


%I = find(X > 25);
%XF = X(I);
%YF = Y(I);
XF = X;
YF = Y;

N=length(XF);
A=zeros(N, 2);
A(:,1) = 1;
A(:,2) = XF;
C = A \ YF;

fitX = [0:0.01:1] * max(X);
fitY = C(1) + C(2) .* fitX;

plot(X, Y, 'b.', fitX, fitY, 'r-')
xlabel('(L/H)^2');
ylabel('E^2');
print('-dpng', 'E2vsR2.png')

input('Next...')

plot(X, Y - (C(1) + C(2).*X), 'b.')
xlabel('(L/H)^2');
ylabel('Residuals: E^2 - fit(E^2)');
print('-dpng', 'ResidvsR2.png')

input('Next...')

res = Y - (C(1) + C(2).*X);

maxr = max(res);
minr = min(res);
%SR = sort(res(IL));
%maxr = SR(round(0.99 * length(SR)));
%minr = SR(round(0.01 * length(SR)));
nbins = 40;
bins = minr + [0:nbins] * (maxr - minr) / nbins;
subplot(3,1,1);
RS = histc(res(IS), bins);
bar(bins, RS, 'histc');
xlabel('E^2 residuals for points with R < 1');
subplot(3,1,2);
RM = histc(res(IM), bins);
bar(bins, RM, 'histc');
xlabel('E^2 residuals for points with R in [4.5, 5.5]');
subplot(3,1,3);
RL = histc(res(IL), bins);
bar(bins, RL, 'histc');
xlabel('E^2 residuals for points with R > 8');
print('-dpng', 'Residhists.png')
subplot(111);
