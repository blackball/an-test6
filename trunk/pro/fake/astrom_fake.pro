;+
; NAME:
;   astrom_fake
; PURPOSE:
;   Make fake astrometric catalog and image lists.
; BUGS:
;   Code header not written.
; REVISION HISTORY:
;   2004-02-25  started - Hogg (NYU)
;-
pro astrom_fake
seed= -2L
nstars= 1000000L
filename= 'catalog.txt'
astrom_fake_catalog, seed,nstars,filename,id=id,xx=xx,yy=yy,zz=zz
ntry=1000
for try=1L,ntry do begin
    trystr= string(try,format='(I9.9)')
    filename1= trystr+'.txt'
    radius= 1.0*randomu(seed)+0.2
    astrom_fake_image_list, seed,id,xx,yy,zz,radius,filename1
endfor
cmd= 'tar cvf for_chris_n_sam.tar *.txt'
spawn, cmd
cmd= 'gzip -fv --best for_chris_n_sam.tar'
spawn, cmd
return
end
