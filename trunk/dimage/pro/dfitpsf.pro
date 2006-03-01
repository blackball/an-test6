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
pro dfitpsf, image, invvar, extract=extract, psf=psf, psfc=psfc, $
             xpsf=xpsf, ypsf=ypsf

nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]
sigma=1./sqrt(median(invvar))
softbias=50.*sigma
help,softbias

atlas=extract.atlas
atlas_ivar=extract.atlas_ivar

atlas=(atlas) > 1.e-6

; Set source object name
soname=filepath('libdimage.'+idlutils_so_ext(), $
                root_dir=getenv('DIMAGE_DIR'), subdirectory='lib')

nc=3L
np=4L
natlas=63L
psf=fltarr(natlas,natlas)
psfc=fltarr(nc, n_elements(extract))
psft=fltarr(natlas,natlas,nc)
xpsf=fltarr(np,nc)
ypsf=fltarr(np,nc)
retval=call_external(soname, 'idl_dfitpsf', float(atlas), $
                     float(atlas_ivar), long(natlas), long(natlas), $
                     long(n_elements(extract)), float(psfc), float(psft), $
                     float(xpsf), float(ypsf), long(nc), long(np))
                     
stop

end
;------------------------------------------------------------------------------
