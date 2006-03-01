;+
; NAME:
;   dpeaks
; PURPOSE:
;   find peaks in an image
; CALLING SEQUENCE:
;   dpeaks, image [, xcen=, ycen=, sigma=, dlim=, maxnpeaks= ]
; INPUTS:
;   image - [nx, ny] input image
; OPTIONAL INPUTS:
;   sigma - sky sigma (defaults to sigma clipped estimate)
;   dlim - limiting separation for identical peaks
;   maxnpeaks - maximum number of peaks to return
;   minpeak - minimum peak value (defaults to 1 sigma)
; OPTIONAL KEYWORDS:
;   /smooth - smooth a bit before finding
; OUTPUTS:
;   xcen, ycen - [nx, ny] positions of peaks
; REVISION HISTORY:
;   11-Jan-2006  Written by Blanton, NYU
;-
;------------------------------------------------------------------------------
pro dpeaks, image, xcen=xcen, ycen=ycen, sigma=sigma, dlim=dlim, $
            maxnpeaks=maxnpeaks, saddle=saddle, smooth=smooth, minpeak=minpeak

if(NOT keyword_set(maxnpeaks)) then maxnpeaks=32
if(NOT keyword_set(dlim)) then dlim=1.
if(NOT keyword_set(sigma)) then sigma=djsig(image, sigrej=2)
if(NOT keyword_set(minpeak)) then minpeak=sigma
if(NOT keyword_set(saddle)) then saddle=3.
if(NOT keyword_set(smooth)) then smooth=0

nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]

; Set source object name
soname=filepath('libdimage.'+kcorrect_so_ext(), $
                root_dir=getenv('DIMAGE_DIR'), subdirectory='lib')

xcen=fltarr(maxnpeaks)
ycen=fltarr(maxnpeaks)
npeaks=0L
retval=call_external(soname, 'idl_dpeaks', float(image), $
                     long(nx), long(ny), long(npeaks), float(xcen), $
                     float(ycen), float(sigma), float(dlim), float(saddle), $
                     long(maxnpeaks), long(smooth), float(minpeak))

xcen=xcen[0:npeaks-1]
ycen=ycen[0:npeaks-1]

end
;------------------------------------------------------------------------------
