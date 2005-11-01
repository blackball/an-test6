;+
; NAME:
;   astrom_fake_catalog
; PURPOSE:
;   Make fake astrometric catalog.
; BUGS:
;   Code header not written.
; REVISION HISTORY:
;   2004-02-25  started - Hogg (NYU)
;-
pro astrom_fake_catalog, seed,nstars,filename,id=id,xx=xx,yy=yy,zz=zz
id= lindgen(nstars)+1L
xx= randomu(seed,nstars,/double)-0.5
yy= randomu(seed,nstars,/double)-0.5
zz= randomu(seed,nstars,/double)-0.5
norm= sqrt(xx^2+yy^2+zz^2)
xx= xx/norm
yy= yy/norm
zz= zz/norm
splog, 'writing file '+filename
openw, wlun,filename,/get_lun
for ii=0L,nstars-1L do printf, wlun,id[ii],xx[ii],yy[ii],zz[ii], $
  format='(I9.0,3(F13.9))'
close, wlun
free_lun, wlun
return
end
