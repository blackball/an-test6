
ra1=radecs(:,1); dec1=radecs(:,2); ra2=radecs(:,3); dec2=radecs(:,4);
plot(ra1,dec1,'ro', ra2,dec2,'bo');
line([ra1';ra2'], [dec1';dec2'])   
axis([10 11 10 11]);

realx=[10.25, 10.25, 10.75, 10.75, 10.25];
realy=[10.25, 10.75, 10.75, 10.25, 10.25];
line(realx, realy, 'LineStyle', ':', 'Color', 'k');
