;+
; NAME:
;   mosaic_mosaic
; PURPOSE:
;   Mosaic together KPNO Mosaic data.
; INPUTS:
;   filelist      - list of files to use
;   filename      - name of output file
;   racen,deccen  - center of image (J2000 deg)
;   dra,ddec      - size of image (deg)
; OPTIONAL INPUTS:
;   bigast        - *big* astrometry structure returned by smosaic_hdr
;                   or equivalent; if set, this *over-rules* racen,
;                   deccen, dra, and ddec
; BUGS:
;   - Uses stoopid nearest-neighbor interpolation!
;   - Invvar is totally made up.
;   - Method for finding overlapping data is approximate and not
;     robust.
;   - Bitmask filename hard-coded.
; REVISION HISTORY:
;   2005-04-09  started - Hogg and Masjedi
;-
pro mosaic_mosaic, filelist,filename,racen,deccen,dra,ddec,bigast=bigast

; create RA---TAN, DEC--TAN wcs header for mosaic
if (NOT keyword_set(bigast)) then begin
    pixscale=.25/3600.0
    bigast= smosaic_hdr(racen,deccen,dra,ddec,pixscale=pixscale)
endif

; initialize mosaic
naxis1= bigast.naxis1
naxis2= bigast.naxis2
image= fltarr(naxis1,naxis2)
weight= fltarr(naxis1,naxis2)

; create RA and Dec values for mosaic pixels
xx= indgen(naxis1)#(intarr(naxis2)+1)
yy= (intarr(naxis1)+1)#indgen(naxis2)
xy2ad, xx,yy,bigast,pixra,pixdec

; get bitmask
bitmaskname= '/global/data/scr/mm1330/4meter/redux/mosaic_bitmask.fits'
bitmask= mrdfits(bitmaskname,1)
for hdu=2,8 do bitmask= [[[bitmask]],[[mrdfits(bitmaskname,hdu)]]]
help, bitmask

nfile= n_elements(filelist)
for ii=0L,nfile-1 do begin
    hdr0= headfits(filelist[ii])
    exptime= sxpar(hdr0,'EXPTIME')

; read in Mosaic data header and extract GSSS astrometry
    for hdu=1,8 do begin
        hdustr= string(hdu,format='(I1.1)')
        hdr= headfits(filelist[ii],exten=hdu)
        datanaxis1= sxpar(hdr,'NAXIS1')
        datanaxis2= sxpar(hdr,'NAXIS2')
        gsssextast, hdr,gsa

; check for overlap
; NOT ROBUST
        tmpn= 10000
        tmpx= float(datanaxis1)*randomu(seed,tmpn)
        tmpy= float(datanaxis2)*randomu(seed,tmpn)
        gsssxyad, gsa,tmpx,tmpy,tmpa,tmpd
        ad2xy, tmpa,tmpd,bigast,tmpx,tmpy
        tmp= where((tmpx GT 0) AND $
                   (tmpx LT naxis1) AND $
                   (tmpy GT 0) AND $
                   (tmpy LT naxis2),overlap)
        if (overlap EQ 0) then begin
            splog, 'no overlap for '+filelist[ii]+' EXTEN='+hdustr

; if there is overlap, read the data and continue
        endif else begin
            splog, 'using '+filelist[ii]+' EXTEN='+hdustr
            data= mrdfits(filelist[ii],hdu,hdr)
            data= data-median(data)

; create inverse variance map and crush bad pixels
            invvar= fltarr(datanaxis1,datanaxis2)+exptime
            bad= where((bitmask[*,*,(hdu-1)] NE 0),nbad)
            help, nbad
            if (nbad GT 0) then invvar[bad]= 0.0
            seeingsigma= 2.5    ; guess
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

; increment arrays
            if (nin GT 0) then begin
                image[xx[inimage],yy[inimage]]= $
                  image[xx[inimage],yy[inimage]] $
                  +data[datax[inimage],datay[inimage]] $
                  *invvar[datax[inimage],datay[inimage]]
                weight[xx[inimage],yy[inimage]]= $
                  weight[xx[inimage],yy[inimage]] $
                  +invvar[datax[inimage],datay[inimage]]
            endif
        endelse
    endfor
endfor

; make image and write fits file
image= image/weight
mwrfits, image,filename,/create
mwrfits, weight,filename
return
end
