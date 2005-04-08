;+
; NAME:
;   mosaic_wcs
; PURPOSE:
;   Put correct WCS into KPNO Mosaic camera images.
; BUGS:
;   - Comment header not yet fully written.
;   - Some things hard-wired.
; REVISION HISTORY:
;   2005-04-07  started - Hogg (NYU)
;-
function mosaic_onechip_wcs,image,hdr
extast, hdr,astr
xy2ad, 1000.0,2000.0,astr,racen,deccen ; position hard-wired
usno= usno_read(racen,deccen,10.0/60.0,catname='USNO-B1.0') ; 10 arcmin
newhdr= hdr
return, newhdr
end

pro mosaic_wcs,filename,newfilename
filename= 'obj082.fits'
newfilename= 'hogg082.fits'
if (newfilename EQ filename) then begin
    splog, 'ERROR: no over-writing allowed'
    return
endif
hdr0= headfits(filename)
mwrfits, 0,newfilename,hdr0
for hdu=1,8 do begin
    image= mrdfits(filename,hdu,hdr)
    newhdr= mosaic_onechip_wcs(image,hdr)
    mwrfits, image,newfilename,newhdr
endfor
return
end
