;+
; NAME:
;   an_tweak
; PURPOSE:
;   Run WCS tweak on a generic image.
; BUGS:
;   - Not written.
;   - Dependencies.
;-
function an_tweak, imagefile,exten=exten, $
                   originalastr=originalastr,xylist=xylist, $
                   siporder=siporder

; get "original" WCS for image
if (NOT keyword_set(originalastr)) then begin
    hdr= headfits(imagefile,exten=exten)
    extast, hdr,originalastr
endif

; get x,y list for image
if (NOT keyword_set(xylist)) then begin
    xylist= an_xylist(imagefile,exten=exten)
endif

; read in "standards" from USNO, SDSS, or 2MASS
standards= an_sdss_read()
jitter= 0.1                     ; arcsec
if (n_tags(standards) LT 2) then begin
    standards= an_usno_read()
    jitter= 1.0                 ; arcsec
endif

; perform Fink-Hogg shift, preserving SIP terms
shiftastr= hogg_wcs_shift(originalastr,xylist,standards)

; perform linear tweak, preserving SIP terms
nsigma= 5.0
lineartweakastr= hogg_wcs_tweak(astr,xx,yy,siporder=1,jitter=8.0*jitter, $
                                nsigma=nsigma,usno=standards,chisq=chisq)
lineartweakastr= hogg_wcs_tweak(astr,xx,yy,siporder=1,jitter=4.0*jitter, $
                                nsigma=nsigma,usno=standards,chisq=chisq)
lineartweakastr= hogg_wcs_tweak(astr,xx,yy,siporder=1,jitter=2.0*jitter, $
                                nsigma=nsigma,usno=standards,chisq=chisq)
lineartweakastr= hogg_wcs_tweak(astr,xx,yy,siporder=1,jitter=jitter, $
                                nsigma=nsigma,usno=standards,chisq=chisq)

; perform non-linear tweak, if requested
if keyword_set(siporder) then begin
; ITERATE THIS
    tweakastr= hogg_wcs_tweak(astr,xx,yy,siporder=siporder,jitter=jitter, $
                              nsigma=nsigma,usno=standards,chisq=chisq)
endif else begin
    tweakastr= lineartweakastr
endelse

return, tweakastr
end
