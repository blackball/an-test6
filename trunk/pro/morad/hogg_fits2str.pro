;+
; PURPOSE:
;   convert USNO-B1.0 fits files to a binary file STR can read
; BUGS:
;   - not written
;   - no header
;   - path hard-coded!
;-
pro hogg_fits2str, outfile
if (not keyword_set(blimit)) then blimit= [1.0,14.5] ; mag
if (not keyword_set(maxerr)) then maxerr=700. ; max position error
if (not keyword_set(maxpm)) then maxpm=70. ; max proper motion
if (not keyword_set(outfile)) then outfile= 'for_str.bin'
file= findfile('/global/pogson1/usnob_fits/*/b*.fit*',count=nfile)
for ii=0L,nfile-1L do begin
    thisusno= mrdfits(file[ii],1)
    good= where((thisusno.mag[2] GT blimit[0]) AND $
                (thisusno.mag[2] LT blimit[1]) AND $
                (thisusno.sde LT maxerr) AND $
                (thisusno.sra LT maxerr) AND $
                (thisusno.mura LT maxpm) AND $
                (thisusno.mudec LT maxpm),ngood)
    if (ngood GT 0) then begin
        thisusno= (temporary(thisusno))[good]
        if keyword_set(usno) then usno= [temporary(usno),thisusno] $
        else usno= thisusno
    endif
    help, file[ii],usno
endfor
sindx= sort(usno.mag[2])
usno= usno[sindx]
openw, wlun,outfile,/get_lun
magic= uint('FF00'X)
help, magic
writeu, wlun,magic
nstars= long(n_elements(usno))
writeu, wlun,nstars
writeu, wlun,0D0,3.6D2,-9D1,9D1
angles_to_xyz, 1D0,usno.ra,9D1-usno.dec,xx,yy,zz
for ii=0L,nstars-1L do writeu, wlun,xx[ii],yy[ii],zz[ii]
close, wlun
free_lun, wlun
return
end
