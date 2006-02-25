;+
; NAME:
;   an_tweak
; PURPOSE:
;   Run WCS tweak on a generic image.
; INPUTS:
;   imagefile    - FITS file name of image with roughly correct WCS
; OPTIONAL INPUTS:
;   exten        - which extension of FITS file to look at; default 0
;   originalastr - the original astrometry header info; over-rides
;                  header to FITS file
;   xylist       - X,Y list of stars in image
;   siporder     - order for SIP distortions; default 1 (ie, no
;                  distortions)
;   qa           - name of PS file for QA plots
; BUGS:
;   - Not tested.
;   - Dependencies.
;   - xylist "optional" input is currently REQUIRED.
;   - jitter hard-wired.
; REVISION HISTORY:
;-
function an_tweak, imagefile,exten=exten, $
                   originalastr=originalastr,xylist=xylist, $
                   siporder=siporder,qa=qa
if (not keyword_set(siporder)) then siporder= 1

; get "original" WCS for image
if (NOT keyword_set(originalastr)) then begin
    hdr= headfits(imagefile,exten=exten)
    extast, hdr,originalastr
endif
astr= originalastr
splog, 'got original ASTR structure'

; get x,y list for image
;if (NOT keyword_set(xylist)) then begin
;    xylist= an_xylist(imagefile,exten=exten)
;endif
splog, 'got XYLIST of size',n_elements(xylist.x)

; get approximate sky location and size
xy2ad, xylist.x,xylist.y,astr,aa,dd
hogg_mean_ad, aa,dd,meanaa,meandd
maxangle= 2.0*max(djs_diff_angle(aa,dd,meanaa,meandd))
splog, 'will get standards in',meanaa,meandd,maxangle

; read in "standards" from USNO, SDSS, or 2MASS
standards= an_sdss_read(meanaa,meandd,maxangle)
jitter= 1.0                     ; arcsec
if (n_tags(standards) LT 2) then begin
    standards= an_usno_read(meanaa,meandd,maxangle)
    jitter= 1.0                 ; arcsec
endif
splog, 'got standards list of size',n_elements(standards.ra)

; QA plot
if keyword_set(qa) then begin
    set_plot, 'ps'
    xsize= 7.5 & ysize= 7.5
    set_plot, "PS"
    device, file=qa,/inches,xsize=xsize,ysize=ysize, $
      xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color, bits=8
    hogg_plot_defaults, axis_char_scale=axis_char_scale
    !P.TITLE= 'original WCS'
    ad2xy, standards.ra,standards.dec,astr,sx,sy
    plot, sx,sy,psym=6
    oplot, xylist.x,xylist.y,psym=1
endif

; perform Fink-Hogg shift, preserving SIP terms
shiftastr= an_wcs_shift(astr,xylist,standards)
astr= shiftastr
splog, 'shifted ASTR'

; QA plot
if keyword_set(qa) then begin
    !P.TITLE= 'shifted WCS'
    ad2xy, standards.ra,standards.dec,astr,sx,sy
    plot, sx,sy,psym=6
    oplot, xylist.x,xylist.y,psym=1
endif

; perform linear tweak, preserving SIP terms
nsigma= 5.0
for log2jitter=3,0,-1 do begin
    thisjitter= jitter*2.0^log2jitter
    lineartweakastr= hogg_wcs_tweak(astr,xylist.x,xylist.y, $
                                    siporder=1,jitter=thisjitter, $
                                    nsigma=nsigma,usno=standards,chisq=chisq)
    astr= lineartweakastr
endfor
splog, 'tweaked ASTR to linear order'

; QA plot
if keyword_set(qa) then begin
    !P.TITLE= 'linear tweaked WCS'
    ad2xy, standards.ra,standards.dec,astr,sx,sy
    plot, sx,sy,psym=6
    oplot, xylist.x,xylist.y,psym=1
endif

; perform non-linear tweak, if requested
if (siporder GT 1) then begin
    for ii=0,3 do begin
        tweakastr= hogg_wcs_tweak(astr,xylist.x,xylist.y, $
                                  siporder=siporder,jitter=jitter, $
                                  nsigma=nsigma,usno=standards,chisq=chisq)
        astr= tweakastr
    endfor
    splog, 'tweaked ASTR to order',siporder

; QA plot
    if keyword_set(qa) then begin
        !P.TITLE= 'non-linear tweaked WCS'
        ad2xy, standards.ra,standards.dec,astr,sx,sy
        plot, sx,sy,psym=6
        oplot, xylist.x,xylist.y,psym=1
    endif
endif else begin
    tweakastr= lineartweakastr
endelse

if keyword_set(qa) then device,/close
return, tweakastr
end
