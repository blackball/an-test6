;+
; PURPOSE:
;   convert MM's fits file to a binary file STR can read
; BUGS:
;   - not written
;   - no header
;-
pro hogg_fits2str, infile,outfile
usno= mrdfits(infile,1)
openw, wlun,outfile,/get_lun
magic= uint('FF00'X)
help, magic
writeu, wlun,magic
nstars= long(n_elements(usno))
writeu, wlun,nstars
writeu, wlun,0D0,3.6D2,-9D1,9D1
pro angles_to_xyz,1D0,usno.ra,9D1-usno.dec,xx,yy,zz
for ii=0L,nstars-1L do writeu, wlun,xx[ii],yy[ii],zz[ii]
close, wlun
free_lun, wlun
return
end
