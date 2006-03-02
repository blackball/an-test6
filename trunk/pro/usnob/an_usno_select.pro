;+
; NAME:
;   an_usno_select
; PURPOSE:
;   Select 12 *uniform* catalogs from USNO, one per (nside=1) HEALPix
;   pixel using healpix on a fine scale for uniformity, for the
;   astrometry.net project.
; INPUTS:
; OPTIONAL INPUTS:
;   nstars   - number of stars to return per healpix catalog (slightly
;              more than 1/12 of the sky (default 12L*4L^9L)
;   band     - band to use for brightness ranking; 2 for B, 3 for R
;              (default 3)
;   minmag   - minimum (brightest) magnitude to consider (default
;              14.0 mag)
;   scale    - scale on which to ensure uniformity of the catalog
;              (default 4.0 deg)
; KEYWORDS:
;   sdss     - over-ride all inputs to SDSS-optimized values
;   galex    - over-ride all inputs to GALEX-optimized values
;   continue - pick up where you left off using save file - USE WITH
;              EXTREME CAUTION
;   mwrfits  - write fits files for each of the small-scale healpix
;              pixels - USE WITH EXTREME CAUTION
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
;   - 12 catalogs don't overlap AT ALL.
;   - Keyword "continue" not properly tested.
;   - No cuts on PM error or position error because usno_read does
;     not return these quantities.  D'oh!
; REVISION HISTORY:
;   2005-10-02  started - Hogg (NYU)
;-
pro an_usno_select, nstars=nstars,band=band,minmag=minmag, $
                    sdss=sdss,galex=galex,continue=continue,mwrfits=mwrfits
radperdeg= !DPI/1.8D2

; set defaults
if (NOT keyword_set(nstars)) then nstars= 12L*4L^9L
if (NOT keyword_set(band)) then band= 3 ; R band
if (NOT keyword_set(minmag)) then minmag= 14.0 ; mag
if (NOT keyword_set(scale)) then scale= 4.0 ; deg
; if (NOT keyword_set(maxerr)) then maxerr= 700.0 ; max position error
; if (NOT keyword_set(maxpm)) then maxpm= 70.0 ; max proper motion
if (keyword_set(sdss)) then begin
    nstars= 12L*4L^11L
    band= 3                     ; R band
    minmag= 14.0                ; min mag
    prefix= 'an_usno_sdss'
endif
if (keyword_set(galex)) then begin
    nstars= 12L*4L^8L
    band= 2                     ; B band
    minmag= 2.0                 ; min mag
    prefix= 'an_usno_galex'
endif
nside= 2L^(6L-round(alog(scale)/alog(2.0)))
if (NOT keyword_set(prefix)) then prefix= 'an_usno_' $
  +strtrim(string(band),2)+'_'+strtrim(string(nside),2)

; set up healpix grids
bignside= 1
healgen_lb,bignside,bigrap,bigdecp
healgen_lb,nside,finerap,finedecp
help, nstars,band,minmag,scale,nside,prefix,bigrap,finerap

; loop over big healpix pixels
for bigpix= 0L,n_elements(bigrap)-1L do begin
    bigpixstr= string(bigpix,format='(I2.2)')
    objsfile= prefix+'_'+bigpixstr+'.objs'
    savefile= prefix+'_'+bigpixstr+'.sav'

; find fine pixels in the big pixel
    ang2pix_ring, bignside,(9D1-finedecp)*radperdeg,finerap*radperdeg,ind
    goodpix= where(ind EQ bigpix)
    rap= finerap[goodpix]
    decp= finedecp[goodpix]
    npix= n_elements(rap)
    nperpix= round(float(nstars)/float(npix))
    nstars= long(npix)*long(nperpix)

; allocate arrays
    ra= dblarr(nstars)-99.0
    dec= dblarr(nstars)-99.0
    ranking_value= fltarr(nstars)-99.0
    help, objsfile,npix,nperpix,nstars

; loop over small healpix pixels
    for ii=0L,npix-1L do begin
        if keyword_set(continue) then begin
            restore, savefile
            continue= 0
        endif
        usno= an_usno_read(rap[ii],decp[ii],scale)
        if (n_tags(usno) GT 1) then begin
            usnomag= usno.mag[band]
            ang2pix_ring, nside,(9D1-usno.dec)*radperdeg,usno.ra*radperdeg,ind

            if keyword_set(mwrfits) then begin
                prefix= 'an_usno_'
                bigstr= string(bigpix,format="(I2.2)")
                finestr= string(goodpix[ii],format="(I5.5)")
                fitsname= prefix+bigstr+'_'+finestr+'.fits'
                mwrfits, usno[where(ind EQ goodpix[ii])],fitsname
            endif

            inside= where((ind EQ goodpix[ii]) AND $
                          (usnomag GT minmag),ninside)
; ;                      (usno.sde LT maxerr) AND $
; ;                      (usno.sra LT maxerr) AND $
; ;                      (usno.mura LT maxpm) AND $
; ;                      (usno.mudec LT maxpm) AND $
;             splog, 'found',ninside,' good USNO-B1.0 stars in pixel',goodpix[ii]
            if (ninside GT 1) then begin
                sindx= inside[sort(usnomag[inside])]
                jj= ii*nperpix
                djj= (nperpix < ninside)-1L
                splog, 'keeping',djj+1L,' USNO-B1.0 stars in pixel', $
                  goodpix[ii],' (',ii+1L,' of',npix,' )'
                sindx= sindx[0:djj]
                kk= jj+djj
                ra[jj:kk]= usno[sindx].ra
                dec[jj:kk]= usno[sindx].dec
                ranking_value[jj:kk]= findgen(djj+1)+0.001*usnomag[sindx]
            endif
        endif
        if ((ii MOD 4096) EQ 0) then save, file=savefile
    endfor
    save, file=savefile

; clean up catalog and sort by rank
    good= where(ranking_value GE 0.0,nstars)
    ra= (temporary(ra))[good]
    dec= (temporary(dec))[good]
    ranking_value= (temporary(ranking_value))[good]
    good= 0                     ; free memory
    sindx= sort(ranking_value)
    ra= (temporary(ra))[sindx]
    dec= (temporary(dec))[sindx]
    ranking_value= (temporary(ranking_value))[sindx]
    sindx= 0                    ; free memory
    save, file=savefile
    ranking_value= 0            ; free memory

; write file
    openw, wlun,objsfile,/get_lun,/stdio
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
endfor
return
end
