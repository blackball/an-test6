;+
; NAME:
;   an_gsss_to_sip
; PURPOSE:
;   Convert a GSSS astrometry structure to a tangent-plane SIP
;   astrometry structure by least-squares on a set of control points.
; INPUTS:
;   hdr            - FITS header containing input astrom structure
; OPTIONAL INPUTS:
;   siporder       - order for SIP polynomial terms; default 1
; OUTPUTS:
;   hdr            - "fixed" FITS header
; BUGS:
;   - Never run on anything.
; REVISION HISTORY:
;   - 2006-06-13  started - Hogg (NYU)
;-
function an_gsss_to_sip, hdr,siporder=siporder
if (NOT keyword_set(siporder)) then siporder= 1
siporder= siporder>1

; make first-order header
newhdr= hdr
gsss_stdast, newhdr
splog, 'made basic astrometry structure with GSSS_STDAST';

; do higher-order stuff
if (siporder GT 1) then begin
    extast, newhdr,astr
    gsssextast, hdr,gsa
    nxpts= siporder+2
    nypts= siporder+2
    xx= ((double(naxis1)-1D0)*dindgen(nxpts)/double(nxpts-1)) $
      #replicate(1D0,nypts)
    yy= ((double(naxis2)-1D0)*dindgen(nypts)/double(nypts-1)) $
      ##replicate(1D0,nxpts)
    gsssxyad, xx,yy,aa,dd
    points= replicate({points, ra: aa[0], dec: dd[0]},nxpts*nypts)
    points.ra= aa
    points.dec= dd
    for ii=0,3 do begin
        tweakastr= hogg_wcs_tweak(astr,xx,yy, $
                                  siporder=siporder,jitter=jitter, $
                                  nsigma=nsigma,usno=points,chisq=chisq)
        astr= tweakastr
    endfor
    splog, 'tweaked astrometry to order',siporder
endif

; return
return, newhdr
end
