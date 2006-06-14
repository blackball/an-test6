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
;                  header to FITS file, must not be GSSS format
;   xylist       - X,Y list of stars in image
;   xyfile       - OR you can pass a ASCII file name with X,Y,FLUX
;                   xyfile overrides xylist. 
;   siporder     - order for SIP distortions; default 1 (ie, no
;                  distortions)
;   qa           - name of PS file for QA plots
; BUGS:
;   - Not rigorously tested.
;   - Dependencies.
;   - xylist "optional" input is currently REQUIRED unless you pass xyfile
;   - jitter hard-wired.
; REVISION HISTORY:
;-
function an_tweak, imagefile,exten=exten, $
                   originalastr=originalastr,xylist=xylist, xyfile=xyfile, $
                   siporder=siporder,qa=qa
if (not keyword_set(siporder)) then siporder= 1
if (not keyword_set(nsigma)) then nsigma= 5.0

; get "original" WCS for image
if (NOT keyword_set(originalastr)) then begin
    hdr= headfits(imagefile,exten=exten)

; deal with the possibility that this has a GSSS header
    gsss = sxpar(hdr,'PPO1',count=N_ppo1)
    if N_ppo1 EQ 1 then begin 
        splog, 'converting GSSS structure into approximate ASTR structure'
        gsss_stdast, hdr
    endif

; extract astr and check
    extast, hdr,originalastr
    if not keyword_set(originalastr) then begin 
        print,'ERROR: Image file ',imagefile
        print,'       does not appear to have an astrometric header'
        return,0
    endif
endif
astr= originalastr
splog, 'got original ASTR structure'

; read in xyfile if keyword is set -- this overrides xylist
if keyword_set(xyfile) then begin 
    readcol, xyfile, x_in, y_in, flux_in    ; read file
    xylist={x:x_in, y:y_in, flux:flux_in}   ; pack in a structure
endif 

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
jitter= 0.2                     ; arcsec
if (n_tags(standards) LT 2) then begin
    standards= an_usno_read(meanaa,meandd,maxangle)
    jitter= 1.0                 ; arcsec
endif
splog, 'got standards list of size',n_elements(standards.ra)

; QA plot
if keyword_set(qa) then begin
    xsize= 7.5 & ysize= 7.5
    set_plot, 'PS'
    device, file=qa,/inches,xsize=xsize,ysize=ysize, $
      xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color, bits=8
    hogg_plot_defaults, axis_char_scale=axis_char_scale
    !P.TITLE= 'original WCS'
    ad2xy, standards.ra,standards.dec,astr,sx,sy
    plot, sx,sy,psym=6
    oplot, xylist.x,xylist.y,psym=1
endif

; perform Fink-Hogg shift, preserving SIP terms
shiftastr= an_wcs_shift(astr,xylist,standards,dmax=2000.0,binsz=4.0)
astr= shiftastr
shiftastr= an_wcs_shift(astr,xylist,standards,dmax=8.0,binsz=0.5)
astr= shiftastr
splog, 'shifted ASTR'

; QA plot
if keyword_set(qa) then begin
    !P.TITLE= 'shifted WCS'
    ad2xy, standards.ra,standards.dec,astr,sx,sy
    plot, sx,sy,psym=6
    oplot, xylist.x,xylist.y,psym=1
endif

; cut down standards list to nearby stuff
xy2ad, xylist.x,xylist.y,astr,aa,dd
spherematch, aa,dd,standards.ra,standards.dec,(30.0*jitter)/3600.0, $
  match1,match2,distance12,maxmatch=0
match2= match2[sort(match2)]
match2= match2[uniq(match2)]
shorts= standards[match2]

; QA plot
if keyword_set(qa) then begin
    !P.TITLE= 'shifted WCS'
    ad2xy, shorts.ra,shorts.dec,astr,sx,sy
    plot, sx,sy,psym=6
    oplot, xylist.x,xylist.y,psym=1
endif

; perform linear tweak, preserving SIP terms
for ii=0,3 do begin
    lineartweakastr= hogg_wcs_tweak(astr,xylist.x,xylist.y, $
                                    siporder=1,jitter=jitter, $
                                    nsigma=nsigma,usno=shorts,chisq=chisq)
    astr= lineartweakastr

; QA plot
    if keyword_set(qa) then begin
        !P.TITLE= 'linear tweaked WCS'
        ad2xy, shorts.ra,shorts.dec,astr,sx,sy
        plot, sx,sy,psym=6
        oplot, xylist.x,xylist.y,psym=1
    endif
endfor
splog, 'tweaked ASTR to linear order'

; perform non-linear tweak, if requested
if (siporder GT 1) then begin
    for ii=0,3 do begin
        tweakastr= hogg_wcs_tweak(astr,xylist.x,xylist.y, $
                                  siporder=siporder,jitter=jitter, $
                                  nsigma=nsigma,usno=shorts,chisq=chisq)
        astr= tweakastr
    endfor
    splog, 'tweaked ASTR to order',siporder

; QA plot
    if keyword_set(qa) then begin
        !P.TITLE= 'non-linear tweaked WCS'
        ad2xy, shorts.ra,shorts.dec,astr,sx,sy
        plot, sx,sy,psym=6
        oplot, xylist.x,xylist.y,psym=1
    endif
endif else begin
    tweakastr= lineartweakastr
endelse

if keyword_set(qa) then device,/close
return, tweakastr
end
