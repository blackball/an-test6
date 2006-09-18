% This code tries to answer the question: If each star has equal Gaussian
% positional noise (on the plane), what is the area in codespace that it takes
% up? We answer this with the bludgeon that is monte carlo simulation.

% We fix ABCD such that the quad is valid, then sample a bunch of quads when
% there is some noise. The codes of the noisy quads give us a distance in
% codespace from the real quad; then we look at the histogram of these
% distances.

% Most catalogs have scale 300 arcseconds for quads Dedup threshold is 8
% arcsecs for USNO... this is probably worse worse case noise
N = 50000;
alpha = 300/sqrt(2);
sigma = 0.01;
sigma = 0.75;
A = alpha*[0 0]';
B = alpha*[1 1]';
C = alpha*[0.25 0.75]';
D = alpha*[0.80 0.30]';

As = repmat(A, 1,N) + randn(2,N)*sigma;
Bs = repmat(B, 1,N) + randn(2,N)*sigma; 
Cs = repmat(C, 1,N) + randn(2,N)*sigma;
Ds = repmat(D, 1,N) + randn(2,N)*sigma;

Bs = Bs - As;
Cs = Cs - As;
Ds = Ds - As;

scale = sum(Bs.*Bs);
costheta = sum(Bs) ./ scale;
sintheta = (Bs(2,:) - Bs(1,:)) ./ scale;

xxtmp = Cs(1,:);
Cs(1,:) = Cs(1,:) .* costheta + Cs(2,:) .* sintheta;
Cs(2,:) = -xxtmp .* sintheta + Cs(2,:) .* costheta;

xxtmp = Ds(1,:);
Ds(1,:) = Ds(1,:) .* costheta + Ds(2,:) .* sintheta;
Ds(2,:) = -xxtmp .* sintheta + Ds(2,:) .* costheta;

dists = sum(([Cs; Ds] - repmat([C; D]./alpha, 1,N)).^2).^.5;

hist(Cs(1,:), 30);
hold on;
kkk = axis;
axis([0 1 kkk(3) kkk(4)]);
hist(Cs(2,:), 30);
title('distribution of Cx and Cy on same axis')

figure
hist(dists, 100);
title('histogram of distance of sampled point to actual point (in codespace)')

% x(min(find(cumsum(h)/sum(h) > 0.9)))
%N = 1000000; d = 4; x = 2*rand(d, N) - 1; sum(sum(x.*x).^.5 < 1)/N *2^d;
% Conclusion:Volume of 4D sphere: 1/2 * pi^2 * r^4

% The scary calculation
%1/2*pi^2*0.015^4 * 22e6
