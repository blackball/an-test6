;+
; NAME:
;   mosaic_wcs
; PURPOSE:
;   Put correct WCS into KPNO Mosaic camera images.
; INPUTS:
;   filename    - name (ie, full path) of input image
;   newfilename - name (ie, full path) for output image
; BUGS:
;   - Comment header not yet fully written.
;   - MANY things hard-coded.
;   - Are there zero-index issues with FIND or my image section?
; REVISION HISTORY:
;   2005-04-07  started - Hogg (NYU)
;-
function mosaic_onechip_wcs,image,hdr,usno,jpeg=jpeg
naxis1= sxpar(hdr,'NAXIS1')
naxis2= sxpar(hdr,'NAXIS2')

; read image *data* section
mosaic_data_section, 'foo',bar,x1,x2,y1,y2,hdr=hdr

; find stars in this image
sigma= djsig(image[500:1500,1500:2500]) ; section hard-wired
fwhm= 4.0                     ; hard-wired at 1 arcsec
hmin= 10.0*sigma              ; hard-wired, don't know what this means
sharplim= [0.2,0.5]           ; don't know what this means
roundlim= [-1.0,1.0]          ; don't know what this means
find, image[x1:x2,y1:y2],xx,yy,flux,sharp,round, $
  hmin,fwhm,roundlim,sharplim,/silent
xx= xx+x1
yy= yy+y1
splog, 'FIND found',n_elements(xx),' stars in the image'

; do a first shift with the tangent projection
extast, hdr,astr
astr.ctype[0]= 'RA---TAN'  ; hard-coded fix/hack
astr.ctype[1]= 'DEC--TAN'
pixscale= 0.258D0
astr.cd= -astr.cd          ; TERRIBLE HACK
ad2xy, usno.ra,usno.dec,astr,usnox,usnoy
offset= offset_from_pairs(xx,yy,usnox,usnoy, $
                          dmax=100.0/pixscale,binsz=5.0/pixscale,/verbose)
print, offset
astr.crpix= astr.crpix-offset

; initialize gsa based on this shifted tangent WCS
gsa= mosaic_gsainit(astr.cd,astr.crpix,astr.crval)

; iterate fit to tweak it up
niter=6
for ii=0,niter do begin
    dtheta= 3.0
    order= 1
    if (ii ge (niter/3)) then order=2
    if (ii ge (2*niter/3)) then order=3
    newgsa = hogg_astrom_tweak(gsa,usno.ra,usno.dec,xx,yy,order=order, $
                               /verbose)
    gsa= newgsa
endfor

if keyword_set(jpeg) then begin
    simage= (image-median(image))
    overlay=0
    hogg_usersym, 20,thick=2
    hogg_image_overlay_plot, xx,yy,naxis1,naxis2,overlay, $
      psym=8,symsize=4.0,factor=1
    gsssadxy, gsa,usno.ra,usno.dec,usnox,usnoy
    hogg_usersym, 4,thick=4
    hogg_image_overlay_plot, usnox,usnoy,naxis1,naxis2,overlay, $
      psym=8,symsize=4.0,factor=1
    rbf= 2
    overlay= nw_rebin_image(overlay,rbf)
    overlay[*,*,0]= 0
    overlay[*,*,2]= 0
    nw_rgb_make, simage,simage,simage,name=jpeg,overlay=overlay, $
      scales=[0.4,0.3,0.2]/sigma,nonlinearity=3,rebinfactor=rbf, $
      quality=90
endif

; make new header and return
newhdr= hdr
gsssputast, newhdr,gsa
sxaddhist, 'GSSS WCS added by the http://astrometry.net/ team',newhdr
return, newhdr
end

pro mosaic_wcs, filename,newfilename
if (newfilename EQ filename) then begin
    splog, 'ERROR: no over-writing allowed'
    return
endif
hdr0= headfits(filename)
splog, 'starting to make file '+newfilename
mwrfits, 0,newfilename,hdr0

for hdu=1,8 do begin
    splog, 'working on hdu',hdu
    image= float(mrdfits(filename,hdu,hdr))

; get usno stars
    if (NOT keyword_set(usno)) then begin
        racen= sxpar(hdr,'CRVAL1')
        deccen= sxpar(hdr,'CRVAL2')
        usno= usno_read(racen,deccen,0.5,catname='USNO-B1.0')
        splog, 'USNO_READ found',n_elements(usno),' stars near the image'
    endif

; make JPEG name (not necessary)
;    prefix= strmid(newfilename,0,strpos(newfilename,'.fit',/reverse_search))
;    jpeg= prefix+'_chip'+strtrim(string(hdu),2)+'.jpg'

; get WCS and write fits
    newhdr= mosaic_onechip_wcs(image,hdr,usno,jpeg=jpeg)
    mwrfits, image,newfilename,newhdr
endfor
return
end
