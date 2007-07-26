% noisesim -n 10000 -m -e 1 > ns1.m
% ns1;

nb=50;
hold off;
[H,X]=hist(codedists1, nb);
bar(X,H,1);
a=axis;
hold on;
cdmass = sum(H) * (max(X)-min(X))/(nb-1);
cdmean = mean(codedists1);

dx=0.01;
x=[0:dx:6];

k=2;
chi=2^(1-k/2) .* x.^(k-1) .* exp(-x.^2/2) ./ gamma(k/2);
chimean = sqrt(2) * gamma((k+1)/2) / gamma(k/2);
chi = chi .* cdmass / (sum(chi) * dx * cdmean/chimean);
p1=plot(x*cdmean/chimean, chi, 'r-', 'LineWidth', 2);

k2=2.5;
chi2=2^(1-k2/2) .* x.^(k2-1) .* exp(-x.^2/2) ./ gamma(k2/2);
chimean2 = sqrt(2) * gamma((k2+1)/2) / gamma(k2/2);
chi2 = chi2 .* cdmass / (sum(chi2) * dx * cdmean/chimean2);
p2=plot(x*cdmean/chimean2, chi2, 'c-', 'LineWidth', 2);

k3=3;
chi3=2^(1-k3/2) .* x.^(k3-1) .* exp(-x.^2/2) ./ gamma(k3/2);
chimean3 = sqrt(2) * gamma((k3+1)/2) / gamma(k3/2);
chi3 = chi3 .* cdmass / (sum(chi3) * dx * cdmean/chimean3);
p3=plot(x*cdmean/chimean3, chi3, 'm-', 'LineWidth', 2);

axis([0 0.03 0 a(4)]);

legend([p1,p2,p3], 'Chi, k=2', 'Chi, k=2.5', 'Chi, k=3', 'Location', 'NorthEast');
xlabel('Code Error');
ylabel('Counts');
title(sprintf(['Code Error and Chi Fit: SDSS with %i-pixel error and ' ...
               '%g-arcminute quads'], pixerrs(1), quadsize));

