% fitstomatlab ~/stars/GALEX_FIELDS/galex-fuv.xy.fits > galex_fuv.m
% fitstomatlab ~/stars/GALEX_FIELDS/galex-fuv.rdls.fits > galex_fuv_rdls.m
% galex_stats
% fitstomatlab an-galex-cut.fits > galexcut.m
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

p=plot(BAND, MAG, 'b.');
bands=unique(BAND);
set(gca, 'XTick', bands);
for i=1:length(bands), bandchars{i}=char(bands(i)); end
set(gca, 'XTickLabel', bandchars)
a=axis;
axis([64 88 a(3) a(4)])
input('next');

for i=1:length(bandchars),
  b=bands(i);
  inds=find(BAND == b);
  mag=MAG(inds);
  hist(mag, 25);
  txt=['Band ', bandchars{i}];
  title(txt);

  input('next');
end

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
  if (length(ind))
    FUVMAG(i) = sum(FUV_MAG(ind)) ./ length(ind);
  end
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

  input('next');
end

