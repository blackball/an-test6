;+
; NAME:
;   hogg_acs_wcs
; PURPOSE:
;   Replace ACS drz file WCS with *correct* WCS.
; EXAMPLE:
;   newhdr= hogg_acs_wcs('./j8e951011_drz.fits.gz','./j8e951011_shag.fits')
; INPUTS:
;   filename     - ACS drz file name
;   newfilename  - name for new FITS file to write, if set
; OPTIONAL INPUTS:
;   hdu          - hdu for image in FITS file; defaults to 1
;   jpeg         - name of jpeg QA image to make, if set
; OUTPUTS:
;   hdr          - correct FITS header, with "more" correct WCS
; BUGS:
;   - only improves the shift, not the rotation or scale or shear
;   - assumes that we are working with the "_drz.fits" file
;   - parameters for "find" hard-wired
;   - does only a single image, not an array of images
; REVISION HISTORY:
;   2005-04-06  first quasi-working version - Hogg
;-
function hogg_acs_wcs, filename,newfilename,hdu=hdu,jpeg=jpeg

; read ACS image and WCS
if (n_elements(hdu) eq 0) then hdu= 1
image= mrdfits(filename,hdu,hdr)
naxis1= sxpar(hdr,'NAXIS1')
naxis2= sxpar(hdr,'NAXIS2')
extast, hdr,oldastr
xy2ad, naxis1/2,naxis2/2,oldastr,racen,deccen

; grab all USNO-B stars in the region
usno= usno_read(racen,deccen,3.0/60.0,catname='USNO-B1.0')

; find all compact sources in the ACS image
hmin= 1.0
fwhm= 20.0
sharplim= [0.01,10.0]
roundlim= [-1.0,1.0]
find, image,x1,y1,flux,sharp,roundness,hmin,fwhm,roundlim,sharplim, $
  PRINT=print,/silent

; get offset to improve WCS
ad2xy, usno.ra,usno.dec,oldastr,x2,y2
dmax= (5.0/0.04)
binsz= (0.5/0.04)
offset= offset_from_pairs(x1,y1,x2,y2,dmax=dmax,binsz=binsz,/verbose)
splog, offset
newastr= oldastr
newastr.crpix= oldastr.crpix-offset

; tweak up WCS to all-good
; [to be done]

; make figure, if desired
if keyword_set(jpeg) then begin
    ad2xy, usno.ra,usno.dec,newastr,x3,y3
    hogg_usersym, 20
    hogg_image_overlay_plot, x3,y3,naxis1,naxis2,overlay, $
      psym=8,symsize=3.0,factor=1
;    hogg_image_overlay_plot, x2,y2,naxis1,naxis2,overlay, $
;      psym=6,symsize=3.0,factor=1
;    hogg_image_overlay_plot, x1,y1,naxis1,naxis2,overlay, $
;      psym=1,symsize=3.0,factor=1
    rebin= 2
    overlay= 1.-nw_rebin_image(overlay,rebin)
    nw_rgb_make, image,image,image,name=jpeg,overlay=overlay, $
      scales=[100.,80.,60.],nonlinearity=3.,rebinfactor=rebin,quality=90
endif

; write fits file and return
if keyword_set(newfilename) then begin
    putast, hdr,newastr
    sxaddhist, 'WCS fixed by David W. Hogg (NYU) and Phil Marshall (SLAC)',hdr
    hdr0= headfits(filename)
    mwrfits, [0],newfilename,hdr0,/create
    mwrfits, image,newfilename,hdr
endif

return, hdr
end
