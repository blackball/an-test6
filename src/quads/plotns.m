% ./noisesim4 -H -e 1 > ns.m
ns;

%K = size(sx);
sd=sqrt(sx.^2 + sy.^2);

effnoise = hypot(fnoise, inoise);

errest = effnoise .* hypot(3/4 + 3/4, rads);

plot(rads, sd, rads, errest);
legend('simulated', 'est', 'Location', 'NorthWest');
xlabel('Quad Radiuses');
ylabel('Pixel Error');

