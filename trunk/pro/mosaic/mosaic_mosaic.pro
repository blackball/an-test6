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
;   saturation    - pixel value (above bias) to use for saturation;
;                   default 1.7e4
;   bitmaskname   - name of bitmask file; default stoopid
; BUGS:
;   - Invvar is totally made up.
;   - Method for finding overlapping data is approximate and not
;     robust.
; REVISION HISTORY:
;   2005-04-09  written - Hogg and Masjedi
;-
pro mosaic_mosaic, filelist,filename,racen,deccen,dra,ddec, $
                   bigast=bigast,saturation=saturation, $
                   bitmaskname=bitmaskname

; create RA---TAN, DEC--TAN wcs header for mosaic
if (NOT keyword_set(bigast)) then begin
    pixscale=.25/3600.0
    bigast= smosaic_hdr(racen,deccen,dra,ddec,pixscale=pixscale,npixround=8)
endif
if (NOT keyword_set(saturation)) then saturation= 1.7e4

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
if NOT keyword_set(bitmaskname) then bitmaskname= 'mosaic_bitmask.fits'
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

; create inverse variance map and crush bad pixels
            sigma= djsig(data[501:1500,1501:2500])
            help, sigma
            invvar= fltarr(datanaxis1,datanaxis2)+1.0/(sigma^2)
            mask= where(bitmask[*,*,(hdu-1)] GE 1,nmask)
            help, nmask
            if (nmask GT 0) then invvar[mask]= 0.0
            saturmask= ((exptime*data) GT saturation)
            saturmask= (smooth(float(saturmask),3,/edge_truncate) GT 0.01)
            satur= where(temporary(saturmask),nsatur)
            help, nsatur
            if (nsatur GT 0) then invvar[satur]= 0.0

; subtract sky
            bw_est_sky, data,sky
            data= data-temporary(sky)

; reject CRs
            seeingsigma= 2.5    ; guess
            psfvals= exp(-0.5*[1.0,sqrt(2.0)]^2/seeingsigma^2)
            reject_cr, data,invvar,psfvals,cr,nrejects=ncr,nsig=3.0
            help, ncr
            if (ncr GT 0) then begin
                crmask= 0*byte(invvar)
                crmask[cr]= 1
                invvar[where(smooth(float(temporary(crmask)),3, $
                                    /edge_truncate) GT 0.01,ncr)]= 0.0
            endif
            help, ncr

; find data x,y values for the mosaic pixels and insert
            smosaic_remap, (data*invvar),gsa,bigast,outimg,offset, $
              degree=3, $
              weight=invvar,outweight=outwgt, $
              reflimits=reflim,outlimits=outlim
            image[reflim[0,0]:reflim[0,1],reflim[1,0]:reflim[1,1]] = $
              image[reflim[0,0]:reflim[0,1],reflim[1,0]:reflim[1,1]] + $
              outimg[outlim[0,0]:outlim[0,1],outlim[1,0]:outlim[1,1]]
            weight[reflim[0,0]:reflim[0,1],reflim[1,0]:reflim[1,1]] = $
              weight[reflim[0,0]:reflim[0,1],reflim[1,0]:reflim[1,1]] + $
              outwgt[outlim[0,0]:outlim[0,1],outlim[1,0]:outlim[1,1]]
        endelse
    endfor
endfor

; make image and write fits file
image= image/(weight+(weight EQ 0))
mkhdr, hdr,image
putast, hdr,bigast
sxaddhist, 'KPNO Mosaic data mosaiced by Hogg and Masjedi',hdr
mwrfits, image,filename,hdr,/create
mwrfits, weight,filename
return
end
