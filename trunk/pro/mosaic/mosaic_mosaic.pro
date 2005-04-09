;+
; NAME:
;   mosaic_mosaic
; PURPOSE:
;   Mosaic together KPNO Mosaic data.
; INPUTS:
;   racen,deccen  - center of image (J2000 deg)
;   dra,ddec      - size of image (deg)
;   filelist      - list of files to use
;   filename      - name of output file
; BUGS:
;   - only uses hdu number 7!!
;   - uses stoopid nearest-neighbor interpolation!
;   - invvar is totally made up
; REVISION HISTORY:
;   2005-04-09  started - Hogg and Masjedi
;-
pro mosaic_mosaic, racen,deccen,dra,ddec,filelist,filename

; create RA---TAN, DEC--TAN wcs header for mosaic
pixscale=.26/3600.0
bigast= smosaic_hdr(racen,deccen,dra,ddec,pixscale=pixscale)
naxis1= bigast.naxis1
naxis2= bigast.naxis2
image= fltarr(naxis1,naxis2)
weight= fltarr(naxis1,naxis2)

; create RA and Dec values for mosaic pixels
xx= indgen(naxis1)#(intarr(naxis2)+1)
yy= (intarr(naxis1)+1)#indgen(naxis2)
xy2ad, xx,yy,bigast,pixra,pixdec

; get bitmask
bitmask= mrdfits('/global/data/scr/mm1330/4meter/redux/mosaic_bitmask.fits',7)

nfile= n_elements(filelist)
for ii=0L,nfile-1 do begin
    hdr0= headfits(filelist[ii])
    exptime= sxpar(hdr0,'EXPTIME')

; read in Mosaic data image and extract GSSS astrometry
    data= mrdfits(filelist[ii],7,hdr)
    datanaxis1= sxpar(hdr,'NAXIS1')
    datanaxis2= sxpar(hdr,'NAXIS2')
    gsssextast, hdr,gsa
    
; create inverse variance map and crush bad pixels
    invvar= fltarr(datanaxis1,datanaxis2)+exptime
    bad= where(((exptime*data) GT 17000.0) OR $
               (bitmask NE 0),nbad)
    help, nbad
    if (nbad GT 0) then invvar[bad]= 0.0
    seeingsigma= 4.0/2.35 ; guess
    psfvals= exp(-0.5*[1.0,sqrt(2.0)]^2/seeingsigma^2)
    reject_cr, data,invvar,psfvals,cr,nrejects=ncr
    help, ncr
    if (ncr GT 0) then invvar[cr]= 0.0

; find data x,y values for the mosaic pixels
    gsssadxy, gsa,pixra,pixdec,datax,datay
    inimage= where((datax GT (-0.5)) AND $
                   (datax LT (datanaxis1-0.5)) AND $
                   (datay GT (-0.5)) AND $
                   (datay LT (datanaxis2-0.5)),nin)
    if (nin GT 0) then begin
        image[xx[inimage],yy[inimage]]= image[xx[inimage],yy[inimage]] $
          +data[datax[inimage],datay[inimage]] $
          *invvar[datax[inimage],datay[inimage]]
        weight[xx[inimage],yy[inimage]]= weight[xx[inimage],yy[inimage]] $
          +invvar[datax[inimage],datay[inimage]]
    endif
endfor

; write fits file
image= image/weight
mwrfits, image,filename,/create
mwrfits, weight,filename
return
end
