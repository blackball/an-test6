pro sdss_usno, flist,maglim=maglim,jpg=jpg,stats=stats,quads=quads

maxx= 2048.0
maxy= 1489.0
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

obj= sdss_readobj(flist.run,flist.camcol,flist.field,rerun=flist.rerun)
indx= where(obj.petroflux[flist.filter] GT (10.0^(0.4*(22.5-maglim))))
obj= obj[indx]
xx= obj.colc[flist.filter]
yy= obj.rowc[flist.filter]
oplot, xx,yy,psym=6

gsa = sdss_astrom(flist.run,flist.camcol,flist.field,rerun=flist.rerun, $
                  filter=flist.filter)
astrom_xyad, gsa,maxx/2.0,maxy/2.0,ra=racen,dec=deccen
rad= 0.4*max([maxx,maxy])/3600.0
usno= usno_read(racen,deccen,rad,catname='USNO-B1.0')
astrom_adxy, gsa,usno.ra,usno.dec,xpix=usnox,ypix=usnoy
indx= where((usnox GT 0.0) AND $
            (usnox LT maxx) AND $
            (usnoy GT 0.0) AND $
            (usnoy LT maxy),nindx)
if (nindx GT 0) then begin
    usno= usno[indx]
    usnox= usnox[indx]
    usnoy= usnoy[indx]
endif

if keyword_set(quads) then begin
    bigusno= usno_read(racen,deccen,3.0*rad,catname='USNO-B1.0')
    astrom_adxy, gsa,bigusno.ra,bigusno.dec,xpix=bigusnox,ypix=bigusnoy
    totalncd= 0L
    totalncdin= 0L
    for qq=0,100 do begin
        ranx= randomu(seed)*maxx
        rany= randomu(seed)*maxy
        mind2= min((bigusnox-ranx)^2+(bigusnoy-rany)^2,Aindx)
        scale= 250.0            ; pix
        Adist2= (bigusnox[Aindx]-bigusnox)^2+(bigusnoy[Aindx]-bigusnoy)^2
        inscale= where(Adist2 LT (scale^2))
        maxd2= max(Adist2[inscale],Bindx)
        Bindx= inscale[Bindx]
        Bdist2= (bigusnox[Bindx]-bigusnox)^2+(bigusnoy[Bindx]-bigusnoy)^2
        tiny= 1d-6
        incircle= where(((Adist2+Bdist2) LT (scale^2)) AND $
                        (Adist2 GT tiny) AND $
                        (Bdist2 GT tiny),nincircle)
        ncd= nincircle*(nincircle-1)/2
        print, 'choices for C and D: ',ncd
        if (ncd GT 0) then begin
            inimage= where((bigusnox[incircle] GT 0.0) AND $
                           (bigusnox[incircle] LT maxx) AND $
                           (bigusnoy[incircle] GT 0.0) AND $
                           (bigusnoy[incircle] LT maxy),ninimage)
            ncdin= ninimage*(ninimage-1)/2
            print, 'number inside the image: ',ncdin
        endif else begin
            ncdin= 0
        endelse
        totalncd= totalncd+ncd
        totalncdin= totalncdin+ncdin
    endfor
    print, 'fraction of all quads that make it: ', $
      float(totalncdin)/float(totalncd)

endif

if keyword_set(stats) then begin
    set_plot, 'PS'
    xsize= 7.5
    ysize= xsize
    psfilename=prefix+'-stats.ps'
    device, file=psfilename,/inches,xsize=xsize,ysize=ysize, $
      xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color
    hogg_plot_defaults
    spherematch, obj.ra,obj.dec,usno.ra,usno.dec,2.0/3600.0, $
      match1,match2,distance12
    help, match1,match2,obj,usno
    deltax= xx[match1]-usnox[match2]
    deltay= yy[match1]-usnoy[match2]
    plot, deltax,deltay,psym=6, $
      xrange=[-10,10],yrange=[-10,10]
    nusno= n_elements(usno)
    deltax= usnox#replicate(1,nusno)-replicate(1,nusno)#usnox
    deltay= usnoy#replicate(1,nusno)-replicate(1,nusno)#usnoy
    dist= sqrt(deltax^2+deltay^2)
    tiny= 1d-6
    dist= dist[where(dist GT tiny)]
    hogg_plothist, dist,/totalweight
    device,/close
endif

if keyword_set(jpg) then begin
    frame= smosaic_cacheimg(flist)
    frame= frame-median(frame)
    foo= size(frame,/dimens)
    naxis1= foo[0]
    naxis2= foo[1]
    set_plot, 'PS'
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
endif

return
end
