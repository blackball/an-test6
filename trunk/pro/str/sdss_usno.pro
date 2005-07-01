pro sdss_usno, flist, maglim=maglim

if NOT keyword_set(maglim) then maglim= 19.5
if NOT keyword_set(flist) then begin
    flist= {flist, $
            run    : 745L, $
            rerun  : 137L, $
            camcol : 3L,   $
            field  : 101L, $
            filter : 2L    $
           }
endif
prefix= 'str-'+strtrim(string(flist.run),2) $
  +'-'+strtrim(string(flist.rerun),2) $
  +'-'+strtrim(string(flist.camcol),2) $
  +'-'+strtrim(string(flist.field),2) $
  +'-'+strtrim(string(flist.filter),2)

frame= smosaic_cacheimg(flist)
frame= frame-median(frame)
foo= size(frame,/dimens)
naxis1= foo[0]
naxis2= foo[1]

; atv, frame
obj= sdss_readobj(flist.run,flist.camcol,flist.field,rerun=flist.rerun)
indx= where(obj.petroflux[flist.filter] GT (10.0^(0.4*(22.5-maglim))))
obj= obj[indx]
xx= obj.colc[flist.filter]
yy= obj.rowc[flist.filter]
oplot, xx,yy,psym=6
print, minmax(xx),minmax(yy)

racen= mean(obj.ra)
deccen= mean(obj.dec)
rad= 1.0/6.0
usno= usno_read(racen,deccen,rad,catname='USNO-B1.0')
gsa = sdss_astrom(flist.run,flist.camcol,flist.field,rerun=flist.rerun, $
                  filter=flist.filter)
astrom_adxy, gsa,usno.ra,usno.dec,xpix=usnox,ypix=usnoy

set_plot, "PS"
xsize= 7.5
ysize= xsize*float(naxis2)/float(naxis1)
psfilename=prefix+'-overlay.ps'
device, file=psfilename,/inches,xsize=xsize,ysize=ysize, $
  xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color
hogg_plot_defaults
!X.MARGIN= [0,0]
!X.OMARGIN= [0,0]
!Y.MARGIN= [0,0]
!Y.OMARGIN= [0,0]
nw_overlay_range, naxis1,naxis2,xrange,yrange
plot, [0],[0],/nodata, $
  xstyle=5,xrange=xrange, $
  ystyle=5,yrange=yrange
oplot, xx,yy,psym=6
hogg_usersym, 20,thick=!P.THICK
oplot, usnox,usnoy,psym=8
device,/close
overlay= 1-hogg_image_overlay(psfilename,naxis1,naxis2)
overlaycolor= [0,1,0]
for bb=0,2 do overlay[*,*,bb]= overlaycolor[bb]*overlay[*,*,bb]

nw_rgb_make, frame,frame,frame,name=prefix+'.jpg', $
  scales= [6.5,6.5,6.5],nonlinearity=3,quality=95, $
  overlay=overlay

return
end
