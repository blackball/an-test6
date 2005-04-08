pro mosaic_average_zero,filelist,zeroname

nfiles=n_elements(filelist)

if (nfiles eq 1) then begin
    print,"can't average only one file!"
    return
endif
   
hdr0=headfits(filelist[0])
naxis=sxpar(hdr0,'NAXIS')
if (naxis eq 2) then begin
    firsthdu=0
endif else begin
    mwrfits,0,zeroname,/create
    firsthdu=1
endelse

lasthdu=sxpar(hdr0,'NEXTEND')

for hdu=firsthdu,lasthdu do begin

hdr=headfits(filelist[0],exten=hdu)
xsize=sxpar(hdr,'NAXIS1')
ysize=sxpar(hdr,'NAXIS2')
zero=intarr(xsize,ysize,nfiles)

for i=0,nfiles-1 do zero[*,*,i]=mrdfits(filelist[i],hdu)

avsigclip=djs_avsigclip(zero,sigre=3,maxiter=10)

mwrfits,avsigclip,zeroname

endfor

return

end