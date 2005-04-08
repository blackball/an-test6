pro mosaic_average_flat,filelist,avzero,avdark,flatname

nfiles=n_elements(filelist)

   
hdr0=headfits(filelist[0])
naxis=sxpar(hdr0,'NAXIS')
if (naxis eq 2) then begin
    firsthdu=0
endif else begin
    mwrfits,0,flatname,/create
    firsthdu=1
endelse

lasthdu=sxpar(hdr0,'NEXTEND')

for hdu=firsthdu,lasthdu do begin
splog, 'working on hdu',hdu

zero=mrdfits(avzero,hdu)
dark=mrdfits(avdark,hdu)

hdr=headfits(filelist[0],exten=hdu)
xsize=sxpar(hdr,'NAXIS1')
ysize=sxpar(hdr,'NAXIS2')
flat=intarr(xsize,ysize,nfiles)

for i=0,nfiles-1 do begin
splog, 'reading file', filelist[i]
hdr=headfits(filelist[i])
darktime=sxpar(hdr,'DARKTIME')
exptime=sxpar(hdr,'EXPTIME')
splog, 'darktime',darktime

flat[*,*,i]=(mosaic_mrdfits(filelist[i],hdu)-zero-dark*darktime)/exptime
endfor

avsigclip=djs_avsigclip(temporary(flat),sigre=3,maxiter=10)

mwrfits,avsigclip,flatname

endfor

return

end