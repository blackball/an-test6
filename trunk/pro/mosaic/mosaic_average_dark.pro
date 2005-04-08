pro mosaic_average_dark,filelist,avzero,darkname

nfiles=n_elements(filelist)

   
hdr0=headfits(filelist[0])
naxis=sxpar(hdr0,'NAXIS')
if (naxis eq 2) then begin
    firsthdu=0
endif else begin
    mwrfits,0,darkname,/create
    firsthdu=1
endelse

lasthdu=sxpar(hdr0,'NEXTEND')

for hdu=firsthdu,lasthdu do begin

hdr=headfits(filelist[0],exten=hdu)
xsize=sxpar(hdr,'NAXIS1')
ysize=sxpar(hdr,'NAXIS2')
dark=intarr(xsize,ysize,nfiles)

for i=0,nfiles-1 do begin
dark[*,*,i]=mrdfits(filelist[i],hdu)


avsigclip=djs_avsigclip(zero,sigre=3,maxiter=10)

mwrfits,avsigclip,zeroname

endfor

return

end