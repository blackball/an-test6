;+
; NAME:
;   detect_bright
; PURPOSE:
;   detect objects in image of bright object
; CALLING SEQUENCE:
;   
; INPUTS:
;   image - [nx, ny] input image
; COMMENTS:
;   Assumes a sky-subtracted image
; REVISION HISTORY:
;   11-Jan-2006  Written by Blanton, NYU
;-
;------------------------------------------------------------------------------
pro detect_bright, imfile, dbset=dbset

base=(stregex(imfile, '(.*)\.fits.*', /sub, /extr))[1]

if(NOT keyword_set(dbset)) then begin
    dbset={base:base, $
           guess:0., $
           psmooth:20L, $
           glim:40., $
           parent:-1L, $
           xstars:fltarr(128), $
           ystars:fltarr(128), $
           nstars:0L, $
           xgals:fltarr(128), $
           ygals:fltarr(128), $
           ngals:0L}
    
;; get parents
    dparents, imfile

;; read in parents 
    pcat=mrdfits(base+'-pcat.fits',1)
    isig=where(pcat.msig gt 20.)
    xs=pcat.xnd-pcat.xst+1L
    
;; look at all small images for psf
    isort=sort(xs[isig])
    ipsf=isig[isort[where(findgen(n_elements(isig))/float(n_elements(isig)) $
                          lt 0.75)]]
    psf=fltarr(n_elements(ipsf))
    k=0
    for i=0L, n_elements(ipsf)-1L do begin
        image=mrdfits(base+'-parents.fits',1+ipsf[i])
        sigma=dsigma(image)
        ivar=image-image+1./sigma^2
        simage=dsmooth(image,1.)
        ssigma=dsigma(simage)
        dpeaks, simage, xc=xc, yc=yc, sigma=ssigma, minpeak=10.*ssigma, $
          /refine, npeaks=nc
        if(nc gt 0) then begin
            psf[k]=dpsfapprox(image, ivar, xc[0], yc[0] )
            k=k+1
        endif
    endfor
    dbset.guess=median(psf[0:k-1])

;; look for biggest object
    mxs=max(xs, imxs)
    dbset.parent=imxs
endif else begin
    dbset=mrdfits(base+'-dbset.fits', 1)
endelse

if(dbset.nstars gt 0) then begin 
    xstars=dbset.xstars[0:dbset.nstars-1]
    ystars=dbset.ystars[0:dbset.nstars-1]
endif
if(dbset.ngals gt 0) then begin 
    xgals=dbset.xgals[0:dbset.ngals-1]
    ygals=dbset.ygals[0:dbset.ngals-1]
endif
dchildren, dbset.base, dbset.parent, guess=dbset.guess, $
  psmooth=dbset.psmooth, xstars=xstars, ystars=ystars, $
  xgals=xgals, ygals=ygals

dbset.nstars=n_elements(xstars)
if(xstars[0] eq -1) then dbset.nstars=0
dbset.ngals=n_elements(xgals)
if(xgals[0] eq -1) then dbset.ngals=0
if(dbset.nstars gt 0) then begin
    dbset.xstars[0:dbset.nstars-1]=xstars
    dbset.ystars[0:dbset.nstars-1]=ystars
endif
if(dbset.ngals gt 0) then begin
    dbset.xgals[0:dbset.ngals-1]=xgals
    dbset.ygals[0:dbset.ngals-1]=ygals
endif
  
mwrfits, dbset, base+'-dbset.fits', /create

end
;------------------------------------------------------------------------------
