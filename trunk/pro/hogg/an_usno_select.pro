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
;   filename - output filename (default sensible)
; OUTPUTS:
;   nstars   - number of stars *actually* produced
; COMMENTS:
;   - Ranks stars by brightness in a particular band.
;   - Returns a number of stars within a factor of 4 of the number you
;     ask for.
; BUGS:
;   - code not tested
;   - No cuts on PM error or position error because usno_read does
;     not return these quantities.  D'oh!
; REVISION HISTORY:
;   2005-10-02  started - Hogg (NYU)
;-
pro an_usno_select, nstars=nstars,band=band,minmag=minmag,filename=filename
if (NOT keyword_set(nstars)) then nstars= 12L*4L^11L
if (NOT keyword_set(band)) then band= 3 ; R band
if (NOT keyword_set(minmag)) then minmag= 14.0 ; min mag
; if (NOT keyword_set(maxerr)) then maxerr= 700.0 ; max position error
; if (NOT keyword_set(maxpm)) then maxpm= 70.0 ; max proper motion
if (NOT keyword_set(filename)) then filename= 'an_usno_' $
  +strtrim(string(band),2)+'.bin'
nside= 2L^6L
healgen_lb,nside,rap,decp
npix= n_elements(rap)
thetamax= 2.0D0*sqrt(4D0*1.8D2^2/!DPI/double(npix))
nperpix= round(float(nstars)/float(npix))
nstars= long(npix)*long(nperpix)
ra= dblarr(nstars)-99.0
dec= dblarr(nstars)-99.0
mag= fltarr(nstars)+9999.0
radperdeg= !DPI/1.8D2
for ii=0L,npix-1L do begin
    usno= usno_read(rap[ii],decp[ii],thetamax)
    if (n_tags(usno) GT 1) then begin
        if (band EQ 2) then usnomag= usno.bmag
        if (band EQ 3) then usnomag= usno.rmag
        ang2pix_ring, nside,(9D1-usno.dec)*radperdeg,usno.ra*radperdeg,ind
        splog, ii,median(ind)
        inside= where((ind EQ ii) AND $
                      (usnomag GT minmag),ninside)
;                   (usno.sde LT maxerr) AND $
;                   (usno.sra LT maxerr) AND $
;                   (usno.mura LT maxpm) AND $
;                   (usno.mudec LT maxpm) AND $
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
            mag[jj:kk]= usnomag[sindx]
        endif
    endif
endfor
sindx= sort(mag)
ra= (temporary(ra))[sindx]
dec= (temporary(dec))[sindx]
mag= (temporary(mag))[sindx]
openw, wlun,filename,/get_lun,/stdio
magic= uint('FF00'X)
nstars= long(n_elements(ra))
writeu, wlun,magic,nstars,fix(3)
writeu, wlun,double(0),double(2)*!DPI,double(-0.5)*!DPI,double(0.5)*!DPI
angles_to_xyz, 1D0,temporary(ra),9D1-temporary(dec),xx,yy,zz
for ii=0L,nstars-1L do writeu, wlun,xx[ii],yy[ii],zz[ii]
close, wlun
free_lun, wlun
return
end
