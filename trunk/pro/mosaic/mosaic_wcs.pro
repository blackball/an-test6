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
function mosaic_onechip_wcs,image,hdr,jpeg=jpeg
naxis1= sxpar(hdr,'NAXIS1')
naxis2= sxpar(hdr,'NAXIS2')
extast, hdr,astr

; initialize GSSS WCS approximately
gsa= mosaic_gsainit(astr.cd,astr.crval,astr.crpix)
gsssxyad, gsa,1000.0,2000.0,racen,deccen ; position hard-wired

; get usno stars
usno= usno_read(racen,deccen,10.0/60.0,catname='USNO-B1.0') ; 10 arcmin
gsssadxy, gsa,usno.ra,usno.dec,usnox,usnoy

; find stars in this image
sigma= djsig(image[500:1500,1500:2500]) ; section hard-wired
fwhm= 4.0                     ; hard-wired at 1 arcsec
hmin= 3.0*sigma              ; hard-wired, don't know what this means
sharplim= [0.2,0.5]           ; don't know what this means
roundlim= [-1.0,1.0]          ; don't know what this means
find, image,xx,yy,flux,sharp,round,hmin,fwhm,roundlim,sharplim,/silent

; find and apply best shift
offset= offset_from_pairs(xx,yy,usnox,usnoy, $
                          dmax=100.0/0.26,binsz=5.0/0.26,/verbose)
gsa.ppo3= gsa.ppo3-gsa.xsz*offset[0]
gsa.ppo6= gsa.ppo6-gsa.ysz*offset[1]

; tweak up coordinate system
; [TBD]

if keyword_set(jpeg) then begin
    simage= (image-median(image))
    hogg_usersym, 20,thick=4
    overlay=0
    hogg_image_overlay_plot, xx,yy,naxis1,naxis2,overlay, $
      psym=8,symsize=4.0,factor=1
    gsssadxy, gsa,usno.ra,usno.dec,usnox,usnoy
    hogg_usersym, 3,thick=4
    hogg_image_overlay_plot, usnox,usnoy,naxis1,naxis2,overlay, $
      psym=8,symsize=4.0,factor=1
    rbf= 2
    overlay= nw_rebin_image(overlay,rbf)
    overlay[*,*,0]= 0
    overlay[*,*,2]= 0
    nw_rgb_make, simage,simage,simage,name=jpeg,overlay=overlay, $
      scales=[0.010,0.008,0.006],nonlinearity=3,rebinfactor=rbf, $
      quality=90
endif

; make new header and return
newhdr= hdr
gsssputast, newhdr,gsa
sxaddhist, 'GSSS WCS added by the http://astrometry.net/ team'
return, newhdr
end

pro mosaic_wcs, filename,newfilename
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
