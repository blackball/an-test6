
ra1=radecs(:,1); dec1=radecs(:,2); ra2=radecs(:,3); dec2=radecs(:,4);
plot(ra1,dec1,'ro', ra2,dec2,'bo');
line([ra1';ra2'], [dec1';dec2'])   
