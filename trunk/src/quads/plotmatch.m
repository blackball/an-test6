% load inputs...

fminx = min(field(:,1));
fmaxx = max(field(:,1));
fminy = min(field(:,2));
fmaxy = max(field(:,2));

scale = (maxx - minx) / (fmaxx - fminx);

fx = (field(:,1) - fminx) * scale + minx;
fy = (field(:,2) - fminy) * scale + miny;

I=[1:min(100,length(field))];
J=[101:min(200,length(field))];
K=[201:length(field)];

plot(starxy(:,1), starxy(:,2), 'b.', ...
     [minx,maxx], [miny,maxy], 'ro', ...
     fx(I), fy(I), 'go', ...
     fx(J), fy(J), 'yo', ...
     fx(K), fy(K), 'yx');

legend('Index stars', 'Proposed field limits', 'First 100 field stars', ...
	'Next 100 field stars', 'Remaining field stars')
