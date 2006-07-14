% fitstomatlab ~/stars/GALEX_FIELDS/galex-fuv.xy.fits > galex_fuv.m
% fitstomatlab ~/stars/GALEX_FIELDS/galex-fuv.rdls.fits > galex_fuv_rdls.m
% galex_stats
% fitstomatlab an-galex-cut.fits > galexcut.m
% fitscopy ~/local/AN/an_hp354.fits"[#row<=10000]" /tmp/hp354-10k.fits
% fitstomatlab /tmp/hp354-10k.fits > /tmp/hp354_10k.m

hp354_10k
i0=find((BAND_0 ~= 0) & (MAG_0 < 22));
i1=find((BAND_1 ~= 0) & (MAG_1 < 22));
i2=find((BAND_2 ~= 0) & (MAG_2 < 22));
i3=find((BAND_3 ~= 0) & (MAG_3 < 22));
i4=find((BAND_4 ~= 0) & (MAG_4 < 22));
BG_BAND=[BAND_0(i0); BAND_1(i1); BAND_2(i2); BAND_3(i3); BAND_4(i4)];
BG_MAG=[MAG_0(i0); MAG_1(i1); MAG_2(i2); MAG_3(i3); MAG_4(i4)];      
BG_BAND_0 = BAND_0;
BG_BAND_1 = BAND_1;
BG_BAND_2 = BAND_2;
BG_BAND_3 = BAND_3;
BG_BAND_4 = BAND_4;
BG_MAG_0   = MAG_0;
BG_MAG_1   = MAG_1;
BG_MAG_2   = MAG_2;
BG_MAG_3   = MAG_3;
BG_MAG_4   = MAG_4;

galex_fuv;
galex_fuv_rdls;
GRA=RA;
GDEC=DEC;
galexcut;

i0=find(BAND_0>0);
i1=find(BAND_1>0);
i2=find(BAND_2>0);
i3=find(BAND_3>0);
i4=find(BAND_4>0);

BAND=[BAND_0(i0); BAND_1(i1); BAND_2(i2); BAND_3(i3); BAND_4(i4)];
MAG=[MAG_0(i0); MAG_1(i1); MAG_2(i2); MAG_3(i3); MAG_4(i4)];      
CAT=[CATALOG_0(i0); CATALOG_1(i1); CATALOG_2(i2); CATALOG_3(i3); CATALOG_4(i4)];

fprintf('\n%i objects.\n%i Tycho-2 observations.\n%i USNO-B observations.\n\n', ...
	length(BAND_0), sum(CAT==2), sum(CAT==1));

plotnum=0;

subplot(1,1,1);

p=plot(BAND, MAG, 'b.');
bands=unique(BAND);
set(gca, 'XTick', bands);
for i=1:length(bands), bandchars{i}=char(bands(i)); end
set(gca, 'XTickLabel', bandchars)
a=axis;
axis([64 88 a(3) a(4)])
print('-depsc', sprintf('galexcut_ana_%i.eps', plotnum));
plotnum = plotnum + 1;
input('next');

for i=1:length(bandchars),
  b=bands(i);

  subplot(2,1,1);
  inds=find(BAND == b);
  mag=MAG(inds);
  hist(mag, 25);
  mn=min(mag);
  mx=max(mag);
  txt=['Band ', bandchars{i}, ': cut'];
  title(txt);

  subplot(2,1,2);
  inds=find(BG_BAND == b);
  mag=BG_MAG(inds);
  hist(mag, 25);
  mn=min(mn, min(mag));
  mx=max(mx, max(mag));
  txt=['Band ', bandchars{i}, ': background'];
  title(txt);

  subplot(2,1,1);
  a=axis;
  a(1)=mn; a(2)=mx;
  axis(a);

  subplot(2,1,2);
  a=axis;
  a(1)=mn; a(2)=mx;
  axis(a);

  print('-depsc', sprintf('galexcut_ana_%i.eps', plotnum));
  plotnum = plotnum + 1;
  input('next');
end

subplot(1,1,1);

% Catalog cut RA,DEC -> XYZ

RA=[RA(i0); RA(i1); RA(i2); RA(i3); RA(i4)];
DEC=[DEC(i0); DEC(i1); DEC(i2); DEC(i3); DEC(i4)];
rarad = RA .* (pi / 180);
decrad = DEC .* (pi / 180);
XYZ=zeros(length(RA),3);
XYZ(:,1)= cos(decrad) .* cos(rarad);
XYZ(:,2)= cos(decrad) .* sin(rarad);
XYZ(:,3)= sin(decrad);

% Galex RA,DEC -> XYZ

rarad = GRA .* (pi / 180);
decrad = GDEC .* (pi / 180);
GXYZ=zeros(length(GRA),3);
GXYZ(:,1)= cos(decrad) .* cos(rarad);
GXYZ(:,2)= cos(decrad) .* sin(rarad);
GXYZ(:,3)= sin(decrad);

dist_arcsec = 5;
maxdist2 = 2 * (1 - cos(dist_arcsec * pi / (180*60*60)));

FUVMAG=zeros(size(MAG));

for i=1:length(RA),
  dists = zeros([length(GRA),1]);
  for d=1:3,
    dists = dists + (XYZ(i,d) - GXYZ(:,d)).^2;
  end
  ind=find(dists <= maxdist2);
  %length(ind)
  if (length(ind)==0)
    [m,ind]=min(dists);
  end
  FUVMAG(i) = sum(FUV_MAG(ind)) ./ length(ind);
end

for i=1:length(bandchars),
  b=bands(i);
  inds=find(BAND == b);
  mag=MAG(inds);
  fmag=FUVMAG(inds);
  plot(mag, fmag, 'b.');
  txt=['Band ', bandchars{i}];
  xlabel([bandchars{i} ' Mag']);
  ylabel('FUV Mag');
  title(txt);

  print('-depsc', sprintf('galexcut_ana_%i.eps', plotnum));
  plotnum = plotnum + 1;

  input('next');
end



% 
BLUEMAG=zeros(size(RA));
REDMAG =zeros(size(RA));
nblue=zeros(size(RA));
nred=zeros(size(RA));
for ob=0:4,
  band=eval(sprintf('BAND_%i', ob));
  mag=eval(sprintf('MAG_%i', ob));
  inds = find((char(band) == 'O') | (char(band) == 'J'));
  BLUEMAG(inds) = BLUEMAG(inds) + mag(inds);
  nblue(inds) = nblue(inds) + 1;
  inds = find((char(band) == 'E') | (char(band) == 'F'));
  REDMAG(inds) = REDMAG(inds) + mag(inds);
  nred(inds) = nred(inds) + 1;
end

inds=find(BLUEMAG > 0);
BLUEMAG(inds) = BLUEMAG(inds) ./ nblue(inds);
inds=find(REDMAG > 0);
REDMAG(inds) = REDMAG(inds) ./ nred(inds);

inds=find((BLUEMAG > 0) & (REDMAG > 0));
plot(BLUEMAG(inds), REDMAG(inds), 'b.');
xlabel('Blue Mag');
ylabel('Red Mag');

print('-depsc', sprintf('galexcut_ana_%i.eps', plotnum));
plotnum = plotnum + 1;

input('next');


% Background Blue/Red

BG_BLUEMAG=zeros(size(BG_MAG));
BG_REDMAG =zeros(size(BG_MAG));
nblue=zeros(size(BG_MAG));
nred=zeros(size(BG_MAG));
for ob=0:4,
  band=eval(sprintf('BG_BAND_%i', ob));
  mag=eval(sprintf('BG_MAG_%i', ob));
  inds = find(((char(band) == 'O') | (char(band) == 'J')) & (mag < 22));
  BG_BLUEMAG(inds) = BG_BLUEMAG(inds) + mag(inds);
  nblue(inds) = nblue(inds) + 1;
  inds = find(((char(band) == 'E') | (char(band) == 'F')) & (mag < 22));
  BG_REDMAG(inds) = BG_REDMAG(inds) + mag(inds);
  nred(inds) = nred(inds) + 1;
end
inds=find(BG_BLUEMAG > 0);
BG_BLUEMAG(inds) = BG_BLUEMAG(inds) ./ nblue(inds);
inds=find(BG_REDMAG > 0);
BG_REDMAG(inds) = BG_REDMAG(inds) ./ nred(inds);

inds=find((BLUEMAG > 0) & (REDMAG > 0));
bginds=find((BG_BLUEMAG > 0) & (BG_REDMAG > 0));
p=plot(BG_BLUEMAG(bginds), BG_REDMAG(bginds), 'r.', BLUEMAG(inds), REDMAG(inds), 'b.');
set(p(2),'MarkerSize',10);
xlabel('Blue Mag');
ylabel('Red Mag');
legend('Background', 'Cut', 2);
axis tight;

print('-depsc', sprintf('galexcut_ana_%i.eps', plotnum));
plotnum = plotnum + 1;

input('next');

B=BLUEMAG(inds);
R=REDMAG(inds);
BGB=BG_BLUEMAG(bginds);
BGR=BG_REDMAG(bginds);

epses=[0,0.25,0.5,1,1.5,2];
for i=1:length(epses),
  eps=epses(i);
  subplot(2,1,1);
  M= B + eps.*(B-R);
  hist(M, 25);
  title(sprintf('Epsilon = %f: Cut', eps));
  mn=min(M);
  mx=max(M);
  subplot(2,1,2);
  M= BGB + eps.*(BGB-BGR);
  hist(M, 25);
  title(sprintf('Epsilon = %f: Background', eps));
  mn=min(mn, min(M));
  mx=max(mx, max(M));
  subplot(2,1,1);
  a=axis;
  a(1)=mn; a(2)=mx;
  axis(a);
  subplot(2,1,2);
  a=axis;
  a(1)=mn; a(2)=mx;
  axis(a);
  xlabel('B + eps (B - R)');

  print('-depsc', sprintf('galexcut_ana_%i.eps', plotnum));
  plotnum = plotnum + 1;
  input('next');

end

