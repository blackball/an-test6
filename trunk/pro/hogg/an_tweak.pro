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
astr= originalastr

; get x,y list for image
if (NOT keyword_set(xylist)) then begin
    xylist= an_xylist(imagefile,exten=exten)
endif

; get approximate sky location and size
xy2ad xylist.x,xylist.y,astr,aa,dd
hogg_mean_ad, aa,dd,meanaa,meandd
maxangle= 2.0*max(djs_diff_angle(aa,dd,meanaa,meandd))

; read in "standards" from USNO, SDSS, or 2MASS
standards= an_sdss_read(meanaa,meandd,maxangle)
jitter= 0.2                     ; arcsec
if (n_tags(standards) LT 2) then begin
    standards= an_usno_read(meanaa,meandd,maxangle)
    jitter= 1.0                 ; arcsec
endif

; perform Fink-Hogg shift, preserving SIP terms
shiftastr= an_wcs_shift(astr,xylist,standards)
astr= shiftastr

; perform linear tweak, preserving SIP terms
nsigma= 5.0
for log2jitter=3,0,-1 do begin
    thisjitter= jitter*2.0^log2jitter
    lineartweakastr= hogg_wcs_tweak(astr,xx,yy,siporder=1,jitter=thisjitter, $
                                    nsigma=nsigma,usno=standards,chisq=chisq)
    astr= lineartweakastr
endfor

; perform non-linear tweak, if requested
if (keyword_set(siporder) AND (siporder GT 1)) then begin
    for ii=0,3 do begin
        tweakastr= hogg_wcs_tweak(astr,xx,yy,siporder=siporder,jitter=jitter, $
                                  nsigma=nsigma,usno=standards,chisq=chisq)
        astr= tweakastr
    endfor
endif else begin
    tweakastr= lineartweakastr
endelse

return, tweakastr
end
