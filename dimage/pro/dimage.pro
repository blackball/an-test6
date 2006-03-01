;+
; NAME:
;   dimage
; PURPOSE:
;   process image
; CALLING SEQUENCE:
;   dimage, image
; INPUTS:
;   image - [nx, ny] input image
; REVISION HISTORY:
;   11-Jan-2006  Written by Blanton, NYU
;-
;------------------------------------------------------------------------------
pro dimage, image

nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]

;; Preprocess: 
;;   * try to guess saturation
;;   * try to guess PSF size
;;   - set noise level (and test, roughly)
;;   * try to find bad (low) pixels
invvar=dinvvar(image, hdr=hdr, satur=satur)

;; Do first pass: 
;;   * eliminate cosmics
;;   - find small objects and their crude centers
;;   * find better centers
;;   * measure simple flux
smooth=dmedsmooth(image, invvar, box=40L)
simage=image-smooth
dobjects, simage, invvar, object=oimage
dextract, simage, invvar, object=oimage, extract=extract, small=31L
dcenter, extract, xcen=xcen, ycen=ycen
dpsfflux, extract, xcen=xcen, ycen=ycen, gauss=2., flux=flux

stop

;; Now fit PSF to these objects
dfitpsf, simage, invvar, extract=extract

;; Do second pass
;;   * redetect and eliminate cosmics


save


end
;------------------------------------------------------------------------------
