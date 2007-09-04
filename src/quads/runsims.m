clear;
fwhm = 2 * sqrt(2 * log(2));
jitter = 1;
system(sprintf('noisesim -n 1000 -m -e %g -r > sim1.m', jitter));
sim1;

subplot(111);
plot(icode(:,1),icode(:,2),'r.',icode(:,3),icode(:,4),'b.');
line([0 0 1 1 0],[0 1 1 0 0], 'Color','k');
a=2*pi*[0:0.01:1];
line(0.5 + 1/sqrt(2)*cos(a), 0.5 + 1/sqrt(2)*sin(a), 'Color', 'k');
axis equal
legend({'True C','True D'});                  
xlabel('Code_x')
ylabel('Code_y')
title('True codes are uniformly distributed');
print -depsc 'simplot1.eps';

plot(icode(:,1),fcode(:,1)-0.2, 'r.', icode(:,2), fcode(:,2), 'b.', icode(:,3), fcode(:,3)+0.2, 'k.', icode(:,4), fcode(:,4)+0.4, 'm.');
axis equal;
legend({'cx (-0.2)', 'cy (0.0)', 'dx (+0.2)', 'dy (+0.4)'}, 'Location', 'SouthEast');
xlabel('True code value');
ylabel('Noisy code value');
title(sprintf('Noisy codes (e=%g arcseconds FWHM)', jitter * fwhm));
print -depsc 'simplot2.eps';

realx=[icode(:,1);icode(:,3)];
realy=[icode(:,2);icode(:,4)];
x=[fcode(:,1);fcode(:,3)];
y=[fcode(:,2);fcode(:,4)];

plot(x-realx,y-realy,'r.')
axis equal
xlabel('Code error_x');
ylabel('Code error_y');
title('Codes have nearly Gaussian noise');
print -depsc 'simplot3.eps';

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
print -depsc 'simplot4.eps';

err = 1;
errstr=sprintf('-e %g ', err ./ fwhm);
system(['noisesim -n 1000 ', errstr, ' -a 4.0 -m > sim2a.m']);
sim2a;
cd1=codedists1;
system(['noisesim -n 1000 ', errstr, ' -a 4.5 -m > sim2b.m']);
sim2b;
cd2=codedists1;
system(['noisesim -n 1000 ', errstr, ' -a 5.0 -m > sim2c.m']);
sim2c;
cd3=codedists1;

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

print -depsc 'simplot5.eps';


subplot(111);
maxerr = 2;
errs=[0:0.1:maxerr] .* (1./fwhm);
errstr=sprintf('-e %g ', errs);
system(['noisesim -n 1000 ', errstr, ' -a 4.0 > sim2a.m']);
sim2a;
n1=noise;
e1=codemean;
std1=codestd;
system(['noisesim -n 1000 ', errstr, ' -a 4.5 > sim2b.m']);
sim2b;
n2=noise;
e2=codemean;
std2=codestd;
system(['noisesim -n 1000 ', errstr, ' -a 5.0 > sim2c.m']);
sim2c;
n3=noise;
e3=codemean;
std3=codestd;

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
%p1=plot(n1, e1, 'bo-', xx, xx.*s1, 'b--', n2, e2, 'ro-', xx, xx.*s2, 'r--', n3, e3, 'ko-', xx, xx.*s3, 'k--');
p1=plot(n1, e1, 'bo', xx, xx.*s1, 'b-', n2, e2, 'ro', xx, xx.*s2, 'r-', n3, e3, 'ko', xx, xx.*s3, 'k-');
hold on;
p2=plot(n1, e1+std1, 'b:', n1, e1-std1, 'b:');
p3=plot(n2, e2+std2, 'r:', n2, e2-std2, 'r:');
p4=plot(n3, e3+std3, 'k:', n3, e3-std3, 'k:');
plot([0 maxerr], [0.01 0.01], 'Color', [0.5,0.5,0.5]);
hold off;
xlabel('Star Jitter (arcsec FWHM)');
ylabel('Code Error');
title('Code error propagates nearly linearly with star jitter');
l1=sprintf('Best fit: slope %.3g', s1);
l2=sprintf('Best fit: slope %.3g', s2);
l3=sprintf('Best fit: slope %.3g', s3);
ls1=sprintf('+- one sigma: slope %.2g', ss1);
ls2=sprintf('+- one sigma: slope %.2g', ss2);
ls3=sprintf('+- one sigma: slope %.2g', ss3);
legend([p1(1:2); p2(1); p1(3:4); p3(1); p1(5:6); p4(1)], {'AB = 4.0 arcmin', l1, ls1, 'AB = 4.5 arcmin', l2, ls2, 'AB = 5.0 arcmin', l3, ls3}, 'Location', 'NorthWest');
axis([0 maxerr 0 0.02]);
print -depsc 'simplot6.eps';



