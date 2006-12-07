clear;
system('noisesim2 -n 1000 -m -e 2.7 > sim1.m');
sim1;

subplot(111);
plot(realcode(:,1),realcode(:,2),'r.',realcode(:,3),realcode(:,4),'b.');
line([0 0 1 1 0],[0 1 1 0 0], 'Color','k');
a=2*pi*[0:0.01:1];
line(0.5 + 1/sqrt(2)*cos(a), 0.5 + 1/sqrt(2)*sin(a), 'Color', 'k');
axis equal
legend({'True C','True D'});                  
xlabel('Code_x')
ylabel('Code_y')
title('True codes are uniformly distributed');
print -depsc 'simplot1b.eps';

plot(realcode(:,1),code(:,1)-0.2, 'r.', realcode(:,2), code(:,2), 'b.', realcode(:,3), code(:,3)+0.2, 'k.', realcode(:,4), code(:,4)+0.4, 'm.');
axis equal;
legend({'cx (-0.2)', 'cy (0.0)', 'dx (+0.2)', 'dy (+0.4)'}, 'Location', 'SouthEast');
xlabel('True code value');
ylabel('Noisy code value');
title('Noisy codes (e=2.7 arcseconds)');
print -depsc 'simplot2b.eps';

realx=[realcode(:,1);realcode(:,3)];
realy=[realcode(:,2);realcode(:,4)];
x=[code(:,1);code(:,3)];
y=[code(:,2);code(:,4)];

plot(x-realx,y-realy,'r.')
axis equal
xlabel('Code error_x');
ylabel('Code error_y');
title('Codes have nearly Gaussian noise');
print -depsc 'simplot3b.eps';

mx=1.1 * max(abs([x-realx;y-realy]));
%
subplot(2,1,1);          
[n,binx]=hist(x-realx,21);
hist(x-realx,21);
title('Codes have nearly Gaussian noise');
xlabel('Code error_x');
a=axis;
a(1)=-mx;
a(2)=mx;
axis(a);
hold on;
binw=binx(2)-binx(1);
m=mean(x-realx);
s= std(x-realx);
xx=a(1):(a(2)-a(1))./100:a(2);
yy=length(y).*binw./(s*sqrt(2*pi)).*exp(-(xx-m).^2./(2*(s.^2)));
plot(xx, yy, 'r-', 'LineWidth', 2);
%
subplot(2,1,2); 
[n,binx]=hist(y-realy, 21);
hist(y-realy, 21);
xlabel('Code error_y');
axis(a);
hold on;
binw=binx(2)-binx(1);
m=mean(y-realy);
s= std(y-realy);
xx=a(1):(a(2)-a(1))./100:a(2);
yy=length(y).*binw./(s*sqrt(2*pi)).*exp(-(xx-m).^2./(2*s.^2));
plot(xx, yy, 'r-', 'LineWidth', 2);
%
print -depsc 'simplot4b.eps';

subplot(111);

clear;
system('noisesim2 -n 10000 -e 0.0 -e 0.25 -e 0.5 -e 0.75 -e 1.0 -e 1.25 -e 1.5 -e 1.75 -e 2.0 -a 4.0 -l 4.0 -u 5.0 > sim2a.m');
sim2a;
n1=noise;
e1=codestd;
ab1=abinvalid;
cd1=cdinvalid;
system('noisesim2 -n 10000 -e 0.0 -e 0.25 -e 0.5 -e 0.75 -e 1.0 -e 1.25 -e 1.5 -e 1.75 -e 2.0 -a 4.5 -l 4.0 -u 5.0 > sim2b.m');
sim2b;
n2=noise;
e2=codestd;
ab2=abinvalid;
cd2=cdinvalid;
system('noisesim2 -n 10000 -e 0.0 -e 0.25 -e 0.5 -e 0.75 -e 1.0 -e 1.25 -e 1.5 -e 1.75 -e 2.0 -a 5.0 -l 4.0 -u 5.0 > sim2c.m');
sim2c;
n3=noise;
e3=codestd;
ab3=abinvalid;
cd3=cdinvalid;

s1 = n1' \ e1';
s2 = n2' \ e2';
s3 = n3' \ e3';

xx=[min(n1),max(n1)];
plot(n1, e1, 'bo-', xx, xx.*s1, 'b:', n2, e2, 'ro-', xx, xx.*s2, 'r:', n3, e3, 'ko-', xx, xx.*s3, 'k:');
xlabel('Star Jitter (arcsec)');
ylabel('stddev(Code error)');
title('Error propagates nearly linearly');
l1=sprintf('Best fit: slope %.3g', s1);
l2=sprintf('Best fit: slope %.3g', s2);
l3=sprintf('Best fit: slope %.3g', s3);
legend({'AB = 4.0 arcmin', l1, 'AB = 4.5 arcmin', l2, 'AB = 5.0 arcmin', l3}, 'Location', 'NorthWest');
print -depsc 'simplot5b.eps';


subplot(111);

c1 = n1' \ cd1';
c2 = n2' \ cd2';
c3 = n3' \ cd3';

plot(n1, cd1, 'bo-', xx, xx.*c1, 'b:', n2, cd2, 'ro-', xx, xx.*c2, 'r:', n3, cd3, 'ko-', xx, xx.*c3, 'k:');

xlabel('Star Jitter (arcsec)');
ylabel('Proportion of Codes that become Invalid');
title('A linear number of quads become invalid due to C,D motion');
l1=sprintf('Best fit: slope %.3g', c1);
l2=sprintf('Best fit: slope %.3g', c2);
l3=sprintf('Best fit: slope %.3g', c3);
legend({'AB = 4.0 arcmin', l1, 'AB = 4.5 arcmin', l2, 'AB = 5.0 arcmin', l3}, 'Location', 'NorthWest');
print -depsc 'simplot6b.eps';

clear;
AB=[4.0:0.025:5.0];
IA=[];
IC=[];
for i=1:length(AB),
	ab=AB(i);
	cmd=sprintf('noisesim2 -n 10000 -e 2.0 -l 4.0 -u 5.0 -a %g > sim2d%i.m', ab, i);
	%fprintf('cmd=%s\n', cmd);
	system(cmd);
	%sim2d;
	eval(sprintf('sim2d%i', i));
	%fprintf('abinv=%g\n', abinvalid);
	IA(i)=abinvalid(1);
	IC(i)=cdinvalid(1);
end

plot(AB, IA+IC, 'r.-', AB, IA, 'b.-');
axis([4, 5, 0, 0.6]);
xlabel('AB angle');
ylabel('Proportion of Invalidated Quads');
title('Quads with scales near the boundaries can become invalid (error = 2 arcsec)');
legend({'CD positions invalid', 'AB scale invalid'}, 'Location', 'North');
print -depsc 'simplot7b.eps';

