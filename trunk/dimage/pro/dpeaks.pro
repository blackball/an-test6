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
            maxnpeaks=maxnpeaks, saddle=saddle, smooth=smooth, $
            minpeak=minpeak, refine=refine, npeaks=npeaks, $
            checkpeaks=checkpeaks

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

xcen=lonarr(maxnpeaks)
ycen=lonarr(maxnpeaks)
npeaks=0L
checkpeaks=keyword_set(checkpeaks)
retval=call_external(soname, 'idl_dpeaks', float(image), $
                     long(nx), long(ny), long(npeaks), long(xcen), $
                     long(ycen), float(sigma), float(dlim), float(saddle), $
                     long(maxnpeaks), long(smooth), long(checkpeaks), $
                     float(minpeak))

if(npeaks eq 0) then return

xcen=xcen[0:npeaks-1]
ycen=ycen[0:npeaks-1]

if(keyword_set(refine)) then begin
    xcenold=xcen
    ycenold=ycen
    xcen=fltarr(npeaks)
    ycen=fltarr(npeaks)
    simage=dsmooth(image, 1.0)
    for i=0L, npeaks-1L do begin
        dcen3x3, simage[xcenold[i]-1:xcenold[i]+1, $
                        ycenold[i]-1:ycenold[i]+1], xr, yr
        if(xr ge -0.5 and xr lt 2.5 and $
           yr ge -0.5 and yr lt 2.5) then begin
            xcen[i]=xcenold[i]-1.+xr
            ycen[i]=ycenold[i]-1.+yr
        endif else begin
            xcen[i]=xcenold[i]
            ycen[i]=ycenold[i]
        endelse
    endfor
endif

end
;------------------------------------------------------------------------------
