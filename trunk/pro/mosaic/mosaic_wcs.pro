;+
; NAME:
;   mosaic_wcs
; PURPOSE:
;   Put correct WCS into KPNO Mosaic camera images.
; INPUTS:
;   filename    - name (ie, full path) of input image
;   newfilename - name (ie, full path) for output image
; COMMENTS:
;   - This makes a jpeg file for any image whose astrometric solution is
;     suspect.
;   - Solutions were improved markedly when I removed close groups of
;     stars from FIND; they are generally saturated stars and bleed
;     trails.
; BUGS:
;   - Takes CD matrix and make it -CD!  Aargh!
;   - Comment header not yet fully written.
;   - MANY things hard-coded.
;   - Are there zero-index issues with FIND or my image section?
; REVISION HISTORY:
;   2005-04-07  started - Hogg (NYU)
;-
function mosaic_onechip_wcs,image,hdr,usno,jpeg=jpeg
naxis1= sxpar(hdr,'NAXIS1')
naxis2= sxpar(hdr,'NAXIS2')
filter= strmid(sxpar(hdr,'FILTER'),0,1)

; find stars in this image
sigma= djsig(image[500:1500,1500:2500]) ; section hard-wired
fwhm= 6.0   ; hard-coded at random
if (filter EQ 'g') then hmin= 0.2 ; hard-coded, don't know what this means
if (filter EQ 'r') then hmin= 0.3
if (filter EQ 'i') then hmin= 0.6
nstarrange= [200,600]  ; hard-coded range of star catalog size
sharplim= [-10.0,10.0]  ; don't know what this means -- CR rejection
roundlim= [-100.0,100.0]  ; don't know what this means
tmpimage= image
bw_est_sky, tmpimage,tmpsky
tmpimage= tmpimage-temporary(tmpsky)
repeat begin
    find, tmpimage,xx,yy,flux,sharp,round, $
      hmin,fwhm,roundlim,sharplim,/silent
    splog, 'FIND found',n_elements(xx),' "stars" in the image'

; hack out large groups of close stars
    range= (max(xx)-min(xx)) > (max(yy)-min(yy))
; HACK to make spheregroup run as a 2d planar FOF
    linklen= 0.1/sqrt(n_elements(xx))
    ingroup= spheregroup((xx-mean(xx))/range,(yy-mean(yy))/range, $
                         linklen,multgroup=multgroup)
    good= where(multgroup[ingroup] EQ 1)
    xx= xx[good]
    yy= yy[good]
    splog, '2-d FOF left',n_elements(xx),' "stars" in the image'

; hack out vertical lines of stars
    ingroup= spheregroup((xx-mean(xx))/range,0.0*xx,linklen/2048.0, $
                         multgroup=multgroup)
    good= where(multgroup[ingroup] EQ 1)
    xx= xx[good]
    yy= yy[good]
    splog, '1-d FOF left',n_elements(xx),' "stars" in the image'

; get numbers right
    nstars= n_elements(xx)
    if (nstars LT nstarrange[0]) then hmin= 0.8*hmin
    if (nstars GT nstarrange[1]) then hmin= 2*hmin
endrep until ((nstars GE nstarrange[0]) AND (nstars LE nstarrange[1]))

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
    dtheta= 6.0/3600.0
    if (ii ge (niter/2)) then dtheta= 2.0/3600.0
    order= 1
    if (ii ge (niter/2)) then order= 2
    verbose= 0
    if (ii eq niter) then verbose= 1
    newgsa = hogg_astrom_tweak(gsa,usno.ra,usno.dec,xx,yy,order=order, $
                               verbose=verbose,nmatch=nmatch, $
                               sigmax=sigmax,sigmay=sigmay,dtheta=dtheta)
    gsa= newgsa
endfor

; make new header
newhdr= hdr
gsssputast, newhdr,gsa
sxaddpar, newhdr,'MWCNMTCH',nmatch,'mosaic_wcs.pro number of matched stars'
splog, 'MWCNMTCH',nmatch
sxaddpar, newhdr,'MWCXRMS',sigmax,'mosaic_wcs.pro rms in x direction (pix)'
splog, 'MWCXRMS',sigmax
sxaddpar, newhdr,'MWCYRMS',sigmay,'mosaic_wcs.pro rms in y direction (pix)'
splog, 'MWCYRMS',sigmay
sxaddhist, 'GSSS WCS added by the http://astrometry.net/ team',newhdr

; make QA plot if the fit is bad and jpeg is set
if ((((sigmax > sigmay) GT 4.0) OR $
     (nmatch LT 50)) AND $
    keyword_set(jpeg)) then begin
    simage= (image-median(image))
    overlay=0
    hogg_usersym, 20,thick=2
    hogg_image_overlay_plot, xx,yy,naxis1,naxis2,overlay, $
      psym=8,symsize=4.0,factor=1
    gsssadxy, gsa,usno.ra,usno.dec,usnox,usnoy
    hogg_usersym, 4,thick=4
    hogg_image_overlay_plot, usnox,usnoy,naxis1,naxis2,overlay, $
      psym=8,symsize=4.0,factor=1
    hogg_image_overlay_grid, newhdr,overlay,dra=0.10,ddec=0.05, $
      nra=3,ndec=3,/gsss,factor=1
    rbf= 2
    overlay= nw_rebin_image(overlay,rbf)
    overlay[*,*,0]= 0
    overlay[*,*,2]= 0
    nw_rgb_make, simage,simage,simage,name=jpeg,overlay=overlay, $
      scales=[0.4,0.3,0.2]/sigma,nonlinearity=3,rebinfactor=rbf, $
      quality=90
endif

return, newhdr
end

pro mosaic_wcs, filename,newfilename
if (newfilename EQ filename) then begin
    splog, 'ERROR: no over-writing allowed'
    return
endif
hdr0= headfits(filename)
mwrfits, 0,newfilename,hdr0

for hdu=1,8 do begin
    splog, 'working on '+newfilename
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
   prefix= strmid(newfilename,0,strpos(newfilename,'.fit',/reverse_search))
   jpeg= prefix+'_chip'+strtrim(string(hdu),2)+'.jpg'

; get WCS and write fits
    newhdr= mosaic_onechip_wcs(image,hdr,usno,jpeg=jpeg)
    mwrfits, image,newfilename,newhdr
endfor
return
end
