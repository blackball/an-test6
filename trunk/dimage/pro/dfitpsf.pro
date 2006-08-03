;+
; NAME:
;   dfitpsf
; PURPOSE:
;   fit a PSF model to an image
; CALLING SEQUENCE:
;   dfitpsf, image, invvar, extract= [, psf=, psfc=, xpsf=, ypsf= ]
; INPUTS:
;   image - [nx, ny] input image
;   invvar - [nx, ny] invverse variance image
; OUTPUTS:
;   psf - [N, N] measured PSF at center
;   psfc - [N, N, Nc] components of PSF model
;   xpsf - [Np] polynomial coeffs in x (-1., 1. units between 0, nx-1)
;   ypsf - [Np] polynomial coeffs in y (-1., 1. units between 0, ny-1)
; REVISION HISTORY:
;   11-Jan-2006  Written by Blanton, NYU
;-
;------------------------------------------------------------------------------
pro dfitpsf, imfile

base=(stregex(imfile, '(.*)\.fits.*', /sub, /extr))[1]
image=mrdfits(imfile)
nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]

;; free parameters: plim to detect, small, minbatlas, smoothing box, stardiff

;; if there are zeros at the edges, excise 'em
xst=0L
yst=0L
sigma=dsigma(image)
if(sigma eq 0) then begin
    ii=where(image gt 0., nii)
    xst=min(ii mod nx)
    yst=min(ii / nx)
    xnd=max(ii mod nx)
    ynd=max(ii / nx)
    image=image[xst:xnd, yst:ynd]
    sigma=dsigma(image)
    nx=(size(image,/dim))[0]
    ny=(size(image,/dim))[1]
endif
invvar=fltarr(nx,ny)+1./sigma^2

msimage=dmedsmooth(image, invvar, box=60)
simage=image-msimage

dobjects, simage, objects=obj, plim=20.
dextract, simage, invvar, object=obj, extract=extract, small=30

if(n_tags(extract) eq 0) then begin
    splog, 'no small enough objects in image'
    return
endif

atlas=extract.atlas
atlas_ivar=extract.atlas_ivar

batlas=(atlas) 
batlas=batlas>1.e-6

; Set source object name
soname=filepath('libdimage.'+idlutils_so_ext(), $
                root_dir=getenv('DIMAGE_DIR'), subdirectory='lib')

; do initial fit
nc=1L
np=1L
natlas=61
psf=fltarr(natlas,natlas)
psfc=fltarr(nc, n_elements(extract))
psft=fltarr(natlas,natlas,nc)
xpsf=fltarr(np,nc)
ypsf=fltarr(np,nc)
retval=call_external(soname, 'idl_dfitpsf', float(batlas), $
                     float(atlas_ivar), long(natlas), long(natlas), $
                     long(n_elements(extract)), float(psfc), float(psft), $
                     float(xpsf), float(ypsf), long(nc), long(np))

; clip non-stars
diff=fltarr(n_elements(extract))
for i=0L, n_elements(extract)-1L do begin 
    model=reform(reform(psft, natlas*natlas, nc)#psfc[*,i], natlas, natlas) 
    scale=max(model) 
    diff[i]=total((atlas[*,*,i]-model)^2/scale^2) 
endfor
isort=sort(diff)
istar=isort[where(diff[isort] lt 20. and lindgen(n_elements(isort)) lt 50)]
extract=extract[istar]

; find basic psf
atlas=extract.atlas
atlas_ivar=extract.atlas_ivar
batlas=(atlas) 
batlas=batlas>1.e-6
nc=1L
np=1L
natlas=61
psf=fltarr(natlas,natlas)
psfc=fltarr(nc, n_elements(extract))
psft=fltarr(natlas,natlas,nc)
xpsf=fltarr(np,nc)
ypsf=fltarr(np,nc)
retval=call_external(soname, 'idl_dfitpsf', float(batlas), $
                     float(atlas_ivar), long(natlas), long(natlas), $
                     long(n_elements(extract)), float(psfc), float(psft), $
                     float(xpsf), float(ypsf), long(nc), long(np))

psft=psft-median(psft)
mwrfits, reform(psft, natlas, natlas), base+'-bpsf.fits', /create


end
;------------------------------------------------------------------------------
