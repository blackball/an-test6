clear;
ns;
plot(fieldstars(:,1), fieldstars(:,2), 'bo', indexproj(:,1), indexproj(:,2), 'r.')
input('Next...')
%plot(L./H, E, 'b.')
%xlabel('L/H');
%ylabel('E');
plot((L./H).^2, E.^2, 'b.')
xlabel('(L/H)^2');
ylabel('E^2');
%input('Next...')

X = (L./H).^2;
Y = E.^2;

I = find(X > 25);

XF = X(I);
YF = Y(I);

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

input('Next...')

plot(X, Y - (C(1) + C(2).*X), 'b.')
xlabel('(L/H)^2');
ylabel('E^2 - fit(E^2)');

