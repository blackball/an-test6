;+
; NAME:
;   an_usno_select
; PURPOSE:
;   Select a *uniform* catalog from USNO, all-sky, using healpix, for
;   the astrometry.net project.
; INPUTS:
; OPTIONAL INPUTS:
;   nstars   - number of stars to return all-sky (default 12*4^11)
;   band     - band to use for brightness ranking; 2 for B, 3 for R, 4
;              for I (default 3)
;   minmag   - minimum (brightest) magnitude to consider (default 14.0)
; KEYWORDS:
;   sdss     - over-ride all inputs to SDSS-optimized values
;   galex    - over-ride all inputs to GALEX-optimized values
;   ngc      - only do healpix pixels in the North Galactic Cap
; OUTPUTS:
;   nstars   - number of stars *actually* produced
; COMMENTS:
;   - Ranks stars by in-pixel brightness rank, with a sub-ranking by
;     absolute brightness; see in-code definition of variable
;     "ranking_value".
;   - Returns a number of stars close to what you ask for but not
;     exactly, because there are some gaps/crap in USNO-B1.0.
;   - The SDSS and GALEX keywords over-ride any manual inputs.
; BUGS:
;   - code not tested
;   - No cuts on PM error or position error because usno_read does
;     not return these quantities.  D'oh!
; REVISION HISTORY:
;   2005-10-02  started - Hogg (NYU)
;-
pro an_usno_select, nstars=nstars,band=band,minmag=minmag, $
                    sdss=sdss,galex=galex,ngc=ngc
if (NOT keyword_set(nstars)) then nstars= 12L*4L^11L
if (NOT keyword_set(band)) then band= 3 ; R band
if (NOT keyword_set(minmag)) then minmag= 14.0 ; min mag
; if (NOT keyword_set(maxerr)) then maxerr= 700.0 ; max position error
; if (NOT keyword_set(maxpm)) then maxpm= 70.0 ; max proper motion
if (keyword_set(sdss)) then begin
    nstars= 12L*4L^10L
    band= 3                     ; R band
    minmag= 14.0                ; min mag
    prefix= 'an_usno_sdss'
endif
if (keyword_set(galex)) then begin
    nstars= 12L*4L^9L
    band= 2                     ; B band
    minmag= 2.0                 ; min mag
    prefix= 'an_usno_galex'
endif
if (NOT keyword_set(prefix)) then prefix= 'an_usno_' $
  +strtrim(string(band),2)
nside= 2L^8L                    ; 2L^6L makes approx 1 deg^2 pixels
healgen_lb,nside,rap,decp
thetamax= 2.0D0*sqrt(4D0*1.8D2^2/!DPI/double(n_elements(rap)))
if keyword_set(ngc) then begin
    prefix= prefix+'_ngc'
    glactc, ngpra,ngpdec,2000,0,90,2,/degree
    goodpix= where(djs_diff_angle(rap,decp,ngpra,ngpdec) LT 60.0)
    rap= rap[goodpix]
    decp= decp[goodpix]
endif else begin
    goodpix= lindgen(n_elements(rap))
endelse
if (NOT keyword_set(filename)) then filename= prefix+'.objs'
if (NOT keyword_set(savefile)) then savefile= prefix+'.sav'
npix= n_elements(rap)
nperpix= round(float(nstars)/float(npix))
nstars= long(npix)*long(nperpix)
ra= dblarr(nstars)-99.0
dec= dblarr(nstars)-99.0
ranking_value= fltarr(nstars)-99.0
radperdeg= !DPI/1.8D2
for ii=0L,npix-1L do begin
    splog, 'working on pixel',ii,' of',npix,' at',rap[ii],decp[ii]
    usno= usno_read(rap[ii],decp[ii],thetamax)
    if (n_tags(usno) GT 1) then begin
        if (band EQ 2) then usnomag= usno.bmag
        if (band EQ 3) then usnomag= usno.rmag
        ang2pix_ring, nside,(9D1-usno.dec)*radperdeg,usno.ra*radperdeg,ind
        inside= where((ind EQ goodpix[ii]) AND $
                      (usnomag GT minmag),ninside)
; ;                      (usno.sde LT maxerr) AND $
; ;                      (usno.sra LT maxerr) AND $
; ;                      (usno.mura LT maxpm) AND $
; ;                      (usno.mudec LT maxpm) AND $
        splog, 'found',ninside,' good USNO-B1.0 stars in pixel',ii
        if (ninside GT 1) then begin
            sindx= inside[sort(usnomag[inside])]
            jj= ii*nperpix
            djj= (nperpix < ninside)-1L
            splog, 'keeping',djj+1L,' USNO-B1.0 stars in pixel',ii
            sindx= sindx[0:djj]
            kk= jj+djj
            ra[jj:kk]= usno[sindx].ra
            dec[jj:kk]= usno[sindx].dec
            ranking_value[jj:kk]= findgen(djj+1)+0.001*usnomag[sindx]
        endif
    endif
    if ((ii MOD 4096) EQ 0) then save, filename=savefile
endfor
save, filename=savefile
good= where(ranking_value GE 0.0,nstars)
ra= (temporary(ra))[good]
dec= (temporary(dec))[good]
ranking_value= (temporary(ranking_value))[good]
good= 0 ; free memory
sindx= sort(ranking_value)
ra= (temporary(ra))[sindx]
dec= (temporary(dec))[sindx]
ranking_value= (temporary(ranking_value))[sindx]
sindx= 0 ; free memory
save, filename=savefile
ranking_value= 0 ; free memory
openw, wlun,filename,/get_lun,/stdio
magic= uint('FF00'X)
nstars= long(n_elements(ra))
writeu, wlun,magic,nstars,fix(3)
writeu, wlun,double(0),double(2)*!DPI,double(-0.5)*!DPI,double(0.5)*!DPI
for ii=0L,nstars-1L do begin ; this loop is slow but memory-safe
    angles_to_xyz, 1D0,ra[ii],9D1-dec[ii],xx,yy,zz
    writeu, wlun,xx,yy,zz
endfor
close, wlun
free_lun, wlun
return
end
