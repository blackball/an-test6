clear;
ns;

do_print = 1;
fnsuff = 'c';

R = L ./ H;
X = R.^2;
Y = E.^2;

XF = X;
YF = Y;

N=length(XF);
A=zeros(N, 2);
A(:,1) = 1;
A(:,2) = XF;
C = A \ YF;

codedist
C

fitX = [0:0.01:1] * max(X);
fitY = C(1) + C(2) .* fitX;

subplot(111);
hold off;
I=[1,3,2,4,1];
plot(fieldstars(:,1), fieldstars(:,2), 'bo', ...
     indexproj(:,1), indexproj(:,2), 'r.', ...
     fieldstars(I,1), fieldstars(I,2), 'b-', ...
     indexproj(I,1), indexproj(I,2), 'r-');
legend({ 'Field stars', 'Index stars', 'Field quad', 'Index quad'});
axis equal;
xlabel('Pixel position');
ylabel('Pixel position');
if do_print,
  print('-dpng', '-r100', ['FI',fnsuff,'.png'])
end
input('Next...')

plot(L./H, E, 'b.')
xlabel('R (quad radiuses)');
ylabel('E (pixels)');
if do_print,
  print('-dpng', '-r100', ['EvsR', fnsuff, '.png'])
end
input('Next...')

%plot((L./H).^2, E.^2, 'b.')
%xlabel('(L/H)^2');
%ylabel('E^2');
%print('-dpng', '-r100', ['E2vsR2', fnsuff, '.png'])
%input('Next...')

IS = find(R < 1);
IM = find((R > 4.5) .* (R < 5.5));
IL = find(R > 8);
SE = sort(E(IL));
me = SE(ceil(0.99 * length(SE)));
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
if do_print,
  print('-dpng', '-r100', ['Ehists', fnsuff, '.png'])
end
input('Next...')


%I = find(X > 25);
%XF = X(I);
%YF = Y(I);

plot(X, Y, 'b.', fitX, fitY, 'r-')
xlabel('R^2 (quad radiuses^2)');
ylabel('E^2 (pixels^2)');
if do_print,
  print('-dpng', '-r100', ['E2vsR2', fnsuff, '.png'])
end
input('Next...')

plot(X, Y - (C(1) + C(2).*X), 'b.')
xlabel('R^2 (quad radiuses^2)');
ylabel('Residuals: E^2 - fit(E^2) (pixels^2)');
if do_print,
  print('-dpng', '-r100', ['ResidvsR2', fnsuff, '.png'])
end
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
if do_print,
  print('-dpng', '-r100', ['Residhists', fnsuff, '.png'])
end
subplot(111);
