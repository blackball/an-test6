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
pro detect_bright, imfile, dbset=dbset, nodeblend=nodeblend, cz=cz

base=(stregex(imfile, '(.*)\.fits.*', /sub, /extr))[1]


if(NOT keyword_set(dbset)) then begin
    dbset={base:base, $
           cz:0., $
           psmooth:20L, $
           glim:40., $
           parent:-1L, $
           xstars:fltarr(128), $
           ystars:fltarr(128), $
           nstars:0L, $
           xgals:fltarr(128), $
           ygals:fltarr(128), $
           ngals:0L}

    hdr=headfits(imfile,ext=0)
    xyad, hdr, 0., 0., ra1, dec1
    xyad, hdr, 0., 10., ra2, dec2
    spherematch,ra1,dec1,ra2,dec2, 10., m1, m2, d12
    app=d12*3600./10.
    
;; get parents
    dparents, imfile

;; read in parents 
    pcat=mrdfits(base+'-pcat.fits',1)
    isig=where(pcat.msig gt 20.)
    xs=pcat.xnd-pcat.xst+1L
    
;; look for biggest object
    mxs=max(xs, imxs)
    dbset.parent=imxs

    if(keyword_set(cz)) then begin
        dbset.cz=cz
        dbset.psmooth=3600.*180./!DPI*4./(1000.*dbset.cz/70.)/app
    endif

    dfitpsf, imfile
endif else begin
    dbset=mrdfits(base+'-dbset.fits', 1)
endelse
psf=mrdfits(base+'-bpsf.fits')


if(0) then begin
if(dbset.nstars gt 0) then begin 
    xstars=dbset.xstars[0:dbset.nstars-1]
    ystars=dbset.ystars[0:dbset.nstars-1]
endif
if(dbset.ngals gt 0) then begin 
    xgals=dbset.xgals[0:dbset.ngals-1]
    ygals=dbset.ygals[0:dbset.ngals-1]
endif
endif
dchildren, dbset.base, dbset.parent, $
  psmooth=dbset.psmooth, xstars=xstars, ystars=ystars, $
  xgals=xgals, ygals=ygals, psf=psf, nodeblend=nodeblend

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
