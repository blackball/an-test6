pro mosaic_flatten,infile,avzero,avdark,avflat,flattenname

print,infile
hdr0=headfits(infile)
naxis=sxpar(hdr0,'NAXIS')
firsthdu=1
lasthdu=sxpar(hdr0,'NEXTEND')
darktime=sxpar(hdr0,'DARKTIME')
exptime=sxpar(hdr0,'EXPTIME')
splog, 'darktime',darktime

sxaddhist, $
  'zero substracted, flat substracted and flattened by M. Masjedi', $
  hdr0
mwrfits,0,flattenname,hdr0,/create

for hdu=firsthdu,lasthdu do begin
    splog, 'working on hdu',hdu

    zero=mrdfits(avzero,hdu)
    dark=mrdfits(avdark,hdu)
    flat=mrdfits(avflat,hdu)
    flatten=(mosaic_mrdfits(infile,hdu,hdr,crosstalk=crosstalk) $
             -zero-dark*darktime) $
      /(exptime*(flat+(flat le 0.)))
    mwrfits,flatten,flattenname,hdr
endfor
return
end
