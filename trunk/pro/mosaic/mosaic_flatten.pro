pro mosaic_flatten,infile,avzero,avdark,avflat,flattenname


hdr=headfits(infile)
naxis=sxpar(hdr,'NAXIS')
if (naxis eq 2) then begin
    firsthdu=0
endif else begin
    mwrfits,hdr,flattenname,/create
    firsthdu=1
endelse

lasthdu=sxpar(hdr,'NEXTEND')

for hdu=firsthdu,lasthdu do begin
splog, 'working on hdu',hdu

zero=mrdfits(avzero,hdu)
dark=mrdfits(avdark,hdu)
flat=mrdfits(avflat,hdu)


darktime=sxpar(hdr,'DARKTIME')
exptime=sxpar(hdr,'EXPTIME')
splog, 'darktime',darktime

flatten=(mosaic_mrdfits(infile,hdu)-zero-dark*darktime)/(exptime*(flat+(flat le 0.)))

sxaddhist,'zero substracted, flat substracted and flattened by M. Masjedi',hdr

mwrfits,flatten,flattenname,hdr

endfor

return

end