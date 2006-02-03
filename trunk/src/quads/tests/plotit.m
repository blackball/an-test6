
catalog;
catra  = ra  * 180/pi;
catdec = dec * 180/pi;

field;
fieldra  = fieldradec(:,1);
fielddec = fieldradec(:,2);

hold off;
plot(catra, catdec, '.', 'Color', [0.8, 0.8, 0.8]);
hold on;
plot(fieldra, fielddec, '.', 'Color', [0.6, 0.6, 1.0]);

ra1=radecs(:,1); dec1=radecs(:,2); ra2=radecs(:,3); dec2=radecs(:,4);
plot(ra1, dec1, 'ro', ra2, dec2, 'bo');
line([ra1';ra2'], [dec1';dec2'])   
%axis([10 11 10 11]);
axis([10.2 10.8 10.2 10.8]);

realx=[10.25, 10.25, 10.75, 10.75, 10.25];
realy=[10.25, 10.75, 10.75, 10.25, 10.25];
line(realx, realy, 'LineStyle', ':', 'Color', 'k');

hold off;
