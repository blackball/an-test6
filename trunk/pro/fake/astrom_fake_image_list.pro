
;+
; NAME:
;   astrom_fake_image_list
; PURPOSE:
;   Make list of stars in a fake image from the fake catalog.
; BUGS:
;   Code header not written.
; REVISION HISTORY:
;   2004-02-25  started - Hogg (NYU)
;-
pro astrom_fake_image_list, seed,id,xx,yy,zz,radius,filename
; choose field center
xcen= randomu(seed,/double)-0.5
ycen= randomu(seed,/double)-0.5
zcen= randomu(seed,/double)-0.5
norm= sqrt(xcen^2+ycen^2+zcen^2)
xcen= xcen/norm
ycen= ycen/norm
zcen= zcen/norm
; find stars within the angular radius
degperrad= 1.8D2/!DPI
dotprod= xx*xcen+yy*ycen+zz*zcen
mindotprod= cos(radius/degperrad)
inimage= where(dotprod GT mindotprod,ninimage)
if (ninimage LT 1) then begin
    print, 'ERROR: no catalog stars in image'
    openw, wlun, filename,/get_lun
    close, wlun
    free_lun, wlun
    return
endif
idd= id[inimage]
xxx= xx[inimage]
yyy= yy[inimage]
zzz= zz[inimage]
; create x-y coords for stars in image
xtan= xxx-dotprod*xcen
ytan= yyy-dotprod*ycen
ztan= zzz-dotprod*zcen
xtmp= randomu(seed,/double)-0.5
ytmp= randomu(seed,/double)-0.5
ztmp= randomu(seed,/double)-0.5
xaxis1= ycen*ztmp-zcen*ytmp
yaxis1= zcen*xtmp-xcen*ztmp
zaxis1= xcen*ytmp-ycen*xtmp
norm1= sqrt(xaxis1^2+yaxis1^2+zaxis1^2)
xaxis1= xaxis1/norm1
yaxis1= yaxis1/norm1
zaxis1= zaxis1/norm1
xaxis2= ycen*zaxis1-zcen*yaxis1
yaxis2= zcen*xaxis1-xcen*zaxis1
zaxis2= xcen*yaxis1-ycen*xaxis1
ximage= (xtan*xaxis1+ytan*yaxis1+ztan*zaxis1)/sin(radius/degperrad)
yimage= (xtan*xaxis2+ytan*yaxis2+ztan*zaxis2)/sin(radius/degperrad)
; add interloper stars
ninterloper= 0
ntarget= 100
if (ninimage LT ntarget) then begin
    nrandom= round(float(ntarget-ninimage)*4.0/!PI)
    ximage2= 2.0*randomu(seed,nrandom)-1.0
    yimage2= 2.0*randomu(seed,nrandom)-1.0
    interloper= where((ximage2^2+yimage2^2) LT 1.0,ninterloper)
    if (ninterloper GT 0) then begin
        idd= [idd,(lonarr(ninterloper)-1L)]
        ximage= [ximage,ximage2[interloper]]
        yimage= [yimage,yimage2[interloper]]
    endif
endif
; randomize order
ntotal= n_elements(idd)
sindx= shuffle_indx(ntotal)
idd= idd[sindx]
ximage= ximage[sindx]
yimage= yimage[sindx]
; add jitter
; write output
splog, 'writing',ninimage,' catalog stars and',ninterloper,' interloper stars to file '+filename
openw, wlun,filename,/get_lun
for ii=0L,ntotal-1L do printf, wlun,idd[ii],ximage[ii],yimage[ii], $
  format='(I9.0,2(F13.9))'
close, wlun
free_lun, wlun
return
end
