;+
; NAME:
;   detect_bright
; PURPOSE:
;   detect objects in image of bright object
; CALLING SEQUENCE:
;   dimage, image
; INPUTS:
;   image - [nx, ny] input image
; COMMENTS:
;   Assumes a sky-subtracted image
; REVISION HISTORY:
;   11-Jan-2006  Written by Blanton, NYU
;-
;------------------------------------------------------------------------------
pro detect_bright, image

nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]

;; do general object detection
sigma=dsigma(image)
invvar=fltarr(nx,ny)+1./sigma^2
dobjects, image, invvar, object=oimage, plim=25., dpsf=1.

;; now choose the object that includes the center
iobj=where(oimage eq oimage[nx/2L, ny/2L])
iimage=randomn(seed, nx, ny)*sigma
iimage[iobj]=image[iobj]
iivar=fltarr(nx,ny)+1./sigma^2

;; smooth, resample, and find peaks at a reasonable scale 
tt=rebin(dsmooth(iimage, 8), 91,91, /sample)
ttsig=dsigma(tt)
dpeaks, tt, xc=xc, yc=yc, sigma=ttsig, minpeak=5.*ttsig
xpeaks=xc*4.
ypeaks=yc*4.

;; deblend on those peaks
deblend, iimage, iivar, nchild=nchild, xcen=xcen, ycen=ycen, $
  children=children, templates=templates, minpeak=5.*ttsig, $
  xpeaks=xpeaks, ypeaks=ypeaks

;; 


end
;------------------------------------------------------------------------------
