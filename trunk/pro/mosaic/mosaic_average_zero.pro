pro mosaic_average_zero,filelist,averagezero=averagezero

nfiles=n_elements(filelist)


hdr0=headfits(filelist[0])
naxis=sxpar(hdr0,'NAXIS')
if (naxis eq 2) then firsthdu=0 else firsthdu=1
lasthdu=sxpar(hdr0,'NEXTEND')

for hdu=fisthdu,lasthdu do begin

zero=mrdfist(filelist[0],hdu)
zeroarr=
for i=0,nfiles-1 do begin
