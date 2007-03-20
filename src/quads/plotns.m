% ./noisesim4 -H -e 1 > ns.m
ns;
sx=[];
sy=[];
[n,K]=size(d);
for (k=1:K),
  %plot(dx(:,k),dy(:,k), 'b.'),
  %axis equal;
  sx(k)=std(dx(:,k));
  sy(k)=std(dy(:,k));
  md(k)=mean(d(:,k));
  %input('hello\n');
end
%plot([1:K],sx, [1:K],sy);

%errest = hypot(1, rads);
%errest = 2*hypot(1, rads);
%plot(rads, sx, rads, sy, rads, errest);
%plot(rads, sd, rads, errest);
%sd = hypot(sx,sy);

%errest2 = hypot(1.8, 2.1*rads);
%errest3 = hypot(2, 2*rads);

effnoise = hypot(fnoise, inoise);

%errest3 = hypot(fnoise, effnoise*rads);

%qdiam = 682;

errest3 = effnoise .* hypot(3/4 + 2/3, 0.8*rads);

plot(rads, md, rads, errest3);
legend('simulated', 'est', 'Location', 'NorthWest');
xlabel('Quad Radiuses');
ylabel('Pixel Error');

%plot(rads, md, rads, errest2, rads, errest3);
%legend('simulated', 'est1', 'est2', 'Location', 'NorthWest');
%plot(rads, sd, rads, errest2);
