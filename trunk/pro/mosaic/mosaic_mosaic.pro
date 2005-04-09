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

if (not keyword_set(racen)) then racen=162.343
if (not keyword_set(deccen)) then deccen=51.051
if (not keyword_set(dra)) then dra=.02
if (not keyword_set(ddec)) then ddec=.02
if (not keyword_set(filelist)) then $
  filelist=['/global/data/scr/mm1330/4meter/redux/Willman1/af_obj138.fits']
if (not keyword_set(filename)) then filename= 'Willman1.fits'

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

nfile= n_elements(filelist)
for ii=0L,nfile-1 do begin
    
; read in Mosaic data image and extract astrometry
    data= mrdfits(filelist[ii],7,hdr)
    datanaxis1= sxpar(hdr,'NAXIS1')
    datanaxis2= sxpar(hdr,'NAXIS2')
    gsssextast, hdr,gsa
    
; create inverse variance map
    invvar= fltarr(datanaxis1,datanaxis2)+1
    
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
