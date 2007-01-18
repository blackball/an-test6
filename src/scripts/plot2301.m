sdss2301
mid=(maxcorner + mincorner)/2;
ra=180/pi * atan2(mid(:,2), mid(:,1));
ra = ra + 360;
dec=180/pi * asin(mid(:,3));
rad=180/pi * acos(1 - 0.5 * sum((maxcorner' - mincorner').^2))';
plot(ra, dec, 'b.');
%l1=line([ra-rad/2, ra-rad/2, ra+rad/2, ra+rad/2, ra-rad/2]', [dec-rad/2, dec+rad/2, dec+rad/2, dec-rad/2, dec-rad/2]');
%set(l1, 'Color', 'b');
axis equal;
xlabel('RA');
ylabel('DEC');
title('Field Centers of SDSS Run 2301');
