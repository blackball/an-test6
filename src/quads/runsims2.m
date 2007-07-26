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

fwhm = 2 * sqrt(2 * log(2));

maxerr = 2;
errs=[0:0.1:maxerr] .* (1./fwhm);
errstr=sprintf('-e %g ', errs);
system(['noisesim2 -n 5000 ', errstr, ' -a 4.0 -l 4.0 -u 5.0 > sim2a.m']);
sim2a;
n1=noise;
%e1=codestd;
e1=codedistmean;
std1=codediststd;
ab1=abinvalid;
cd1=cdinvalid;
system(['noisesim2 -n 5000 ', errstr, ' -a 4.5 -l 4.0 -u 5.0 > sim2b.m']);
sim2b;
n2=noise;
%e2=codestd;
e2=codedistmean;
std2=codediststd;
ab2=abinvalid;
cd2=cdinvalid;
system(['noisesim2 -n 5000 ', errstr, ' -a 5.0 -l 4.0 -u 5.0 > sim2c.m']);
sim2c;
n3=noise;
%e3=codestd;
e3=codedistmean;
std3=codediststd;
ab3=abinvalid;
cd3=cdinvalid;

n1 = n1 * fwhm;
n2 = n2 * fwhm;
n3 = n3 * fwhm;

s1 = n1' \ e1';
s2 = n2' \ e2';
s3 = n3' \ e3';

ss1 = n1' \ std1';
ss2 = n2' \ std2';
ss3 = n3' \ std3';

xx=[min(n1),max(n1)];
%plot(n1, e1, 'bo-', xx, xx.*s1, 'b:', n2, e2, 'ro-', xx, xx.*s2, 'r:', n3, e3, 'ko-', xx, xx.*s3, 'k:');
p1=plot(n1, e1, 'bo', xx, xx.*s1, 'b-', n2, e2, 'ro', xx, xx.*s2, 'r-', n3, e3, 'ko', xx, xx.*s3, 'k-');
hold on;
p2=plot([n1; n1]', [e1-std1; e1+std1]', 'b--');
p3=plot([n1; n1]', [e2-std2; e2+std2]', 'r--');
p4=plot([n1; n1]', [e3-std3; e3+std3]', 'k--');
hold off;
xlabel('Star Jitter (arcsec FWHM)');
ylabel('Code Error');
title('Error propagates nearly linearly');
l1=sprintf('Best fit: slope %.3g', s1);
l2=sprintf('Best fit: slope %.3g', s2);
l3=sprintf('Best fit: slope %.3g', s3);
ls1=sprintf('+- one sigma: slope %.2g', ss1);
ls2=sprintf('+- one sigma: slope %.2g', ss2);
ls3=sprintf('+- one sigma: slope %.2g', ss3);
legend([p1(1:2); p2(1); p1(3:4); p3(1); p1(5:6); p4(1)], {'AB = 4.0 arcmin', l1, ls1, 'AB = 4.5 arcmin', l2, ls2, 'AB = 5.0 arcmin', l3, ls3}, 'Location', 'NorthWest');
axis([0 maxerr 0 0.016]);
print -depsc 'simplot5b.eps';

subplot(111);



c1 = n1' \ cd1';
c2 = n2' \ cd2';
c3 = n3' \ cd3';
plot(n1, cd1, 'bo', xx, xx.*c1, 'b-', n2, cd2, 'ro', xx, xx.*c2, 'r-', n3, cd3, 'ko', xx, xx.*c3, 'k-');
xlabel('Star Jitter (arcsec FWHM)');
ylabel('Proportion of Codes that become Invalid');
title('A linear number of quads become invalid due to C,D motion');
l1=sprintf('Best fit: slope %.3g', c1);
l2=sprintf('Best fit: slope %.3g', c2);
l3=sprintf('Best fit: slope %.3g', c3);
axis([0 maxerr 0 0.016]);
legend({'AB = 4.0 arcmin', l1, 'AB = 4.5 arcmin', l2, 'AB = 5.0 arcmin', l3}, 'Location', 'NorthWest');
print -depsc 'simplot7b.eps';

if 0,
a1 = n1' \ ab1';
a2 = n2' \ ab2';
a3 = n3' \ ab3';
plot(n1, ab1, 'bo', xx, xx.*a1, 'b-', n2, ab2, 'ro', xx, xx.*a2, 'r-', n3, ab3, 'ko', xx, xx.*a3, 'k-');
xlabel('Star Jitter (arcsec FWHM)');
ylabel('Proportion of Codes that become Invalid');
title('A linear number of quads become invalid due to A,B motion');
l1=sprintf('Best fit: slope %.3g', a1);
l2=sprintf('Best fit: slope %.3g', a2);
l3=sprintf('Best fit: slope %.3g', a3);
legend({'AB = 4.0 arcmin', l1, 'AB = 4.5 arcmin', l2, 'AB = 5.0 arcmin', l3}, 'Location', 'NorthWest');
print -depsc 'simplot7b.eps';
end




err = 1;
errstr=sprintf('-e %g ', err ./ fwhm);
system(['noisesim2 -n 1000 ', errstr, ' -a 4.0 -l 4.0 -u 5.0 -c > sim2a.m']);
sim2a;
cd1=codedists;
system(['noisesim2 -n 1000 ', errstr, ' -a 4.5 -l 4.0 -u 5.0 -c > sim2b.m']);
sim2b;
cd2=codedists;
system(['noisesim2 -n 1000 ', errstr, ' -a 5.0 -l 4.0 -u 5.0 -c > sim2c.m']);
sim2c;
cd3=codedists;
bins=[0.5:1:19.5]./20.0 .* 0.015;
subplot(3,1,1);
y1=hist(cd1, bins);
h1=bar(bins, y1, 1, 'b');
legend({'AB=4.0 arcmin'});
title(sprintf('Code error distributions: star jitter %g arcsec FWHM', err));
subplot(3,1,2);
y2=hist(cd2, bins);
h2=bar(bins, y2, 1, 'r');
legend({'AB=4.5 arcmin'});
subplot(3,1,3);
y3=hist(cd3, bins);
%h3=bar(bins, y3, 1, 'm');
h3=bar(bins, y3, 1, 'FaceColor', [0.2,0.2,0.2]);
legend({'AB=5.0 arcmin'});
xlabel('Code Error');
print -depsc 'simplot6b.eps';


subplot(111);
AB=unique([4.0:0.01:4.1, 4.1:0.05:4.9, 4.9:0.01:5.0]);
IA=[];
IC=[];
err = 2;
errstr=sprintf('-e %g ', err ./ fwhm);
for i=1:length(AB),
	ab=AB(i);
	cmd=sprintf(['noisesim2 -n 10000 ', errstr, '-l 4.0 -u 5.0 -a %g > sim2d%i.m'], ab, i);
	system(cmd);
	eval(sprintf('sim2d%i', i));
	IA(i)=abinvalid(1);
	IC(i)=cdinvalid(1);
end

p1=plot(AB, IC, 'ro-', AB, IA, 'bo-'); %, AB, IA+IC, 'mo-');
set(p1, 'LineWidth', 2);
axis([3.9, 5.1, 0, 0.55]);
xlabel('AB angle');
ylabel('Proportion of Invalidated Quads');
title(sprintf('Quads with scales near the boundaries can become invalid (error = %g arcsec FWHM)', err));
legend({'CD positions invalid', 'AB scale invalid'}, 'Location', 'North');
print -depsc 'simplot8b.eps';


system(sprintf(['noisesim2 -n 10000 -e %g -l 4.0 -u 5.0 -a 4.0 -m | grep scale > sim2e1.m'], 1.0 ./ fwhm));
sim2e1;
scales1 = scale;
system(sprintf(['noisesim2 -n 10000 -e %g -l 4.0 -u 5.0 -a 5.0 -m | grep scale > sim2e2.m'], 1.0 ./ fwhm));
sim2e2;
scales2 = scale;
subplot(2,1,1);
hist(scales1, 20);
title('Actual scales of quads with true scale near the limits (noise = 1 arcsec FWHM)');
ylabel('Counts');
subplot(2,1,2);
hist(scales2, 20);
xlabel('AB Scale (arcmin)');
print -depsc 'simplot9.eps';

subplot(111);
