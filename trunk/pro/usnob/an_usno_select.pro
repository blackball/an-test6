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
;   - code not written
;   - code not tested
; REVISION HISTORY:
;   2005-10-02  started - Hogg (NYU)
;-
pro an_usno_select, nstars=nstars,band=band,minmag=minmag,filename=filename
if (NOT keyword_set(nstars)) then nstars= 12L*4L^7L ; 11L
if (NOT keyword_set(band)) then band= 3 ; R band
if (NOT keyword_set(minmag)) then minmag= 14.0 ; min mag
if (NOT keyword_set(maxerr)) then maxerr= 700.0 ; max position error
if (NOT keyword_set(maxpm)) then maxpm= 70.0 ; max proper motion
if (NOT keyword_set(filename)) then filename= 'an_usno_' $
  +strtrim(string(band),2)+'.bin'
nside= 2L^6L
healgen_lb,nside,rap,decp
npix= n_elements(rap)
thetamax= 2D0*sqrt(4D0*1.8D2^2/!DPI/double(npix))
nperpix= round(float(nstars)/float(npix))
nstars= long(npix)*long(nperpix)
ra= dblarr(nstars)-99.0
dec= dblarr(nstars)-99.0
mag= fltarr(nstars)+9999.0
for ii=0L,npix do begin
    usno= usno_read(rap[ii],decp[ii],radius=thetamax)
    ang2pix, nside,usno.ra,usno.dec,ind
    inside= where((ind EQ ii) AND $
                  (usno.sde LT maxerr) AND $
                  (usno.sra LT maxerr) AND $
                  (usno.mura LT maxpm) AND $
                  (usno.mudec LT maxpm) AND $
                  (usno.mag[band] GT minmag),ninside)
    splog, 'found',ninside,' good USNO-B1.0 stars in pixel',ii
    if (ninside GT 1) then begin
        sindx= inside[sort(usno[inside].mag[band])]
        jj= ii*nperpix
        djj= (nperpix < ninside)-1L
        sindx= sindx[0:djj]
        kk= jj+djj
        ra[jj:kk]= usno[sindx].ra
        dec[jj:kk]= usno[sindx].dec
        mag[jj:kk]= usno[sindx].mag[band]
        splog, 'adding stars with mags',usno[sindx].mag[band]
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
