pro testy
prefix= 'test'
racen= 15D0*(11.+31./60.+51.6/3600.)
deccen= -1.0*(12D0+31./60.+57.0/3600.)
dra= 0.30/60.0
pixscale= 0.03/3600.0
bigast= smosaic_hdr(racen,deccen,dra,dra,pixscale=pixscale)
xx= dindgen(bigast.naxis1)#replicate(1,bigast.naxis2)
yy= replicate(1,bigast.naxis1)#dindgen(bigast.naxis2)
xy2ad, xx,yy,bigast,aa,dd
filelist= findfile('~/lens/data/hogghogg77799/*_flt.fit*')
for ii=0,n_elements(filelist)-1 do begin
    filename= filelist[ii]
    hdr0= headfits(filename)
    thisfilter= hogg_acs_filter(hdr0)
    if (thisfilter NE '0') then for exten=1,sxpar(hdr0,'NEXTEND'),3 do begin
        hdr1= headfits(filename,exten=exten)
        hogg_acs_adxy, hdr1,aa,dd,xflt,yflt
        xflt= round(xflt)       ; WARNING: nearest neighbor!
        yflt= round(yflt)
        naxis1= sxpar(hdr1,'NAXIS1')
        naxis2= sxpar(hdr1,'NAXIS2')
        inimage= where((xflt GE 0) AND $
                       (xflt LT (naxis1-1)) AND $
                       (yflt GE 0) AND $
                       (yflt LT (naxis2-1)),nin)
        splog, nin,' pixels in exten',exten,' of '+filename+' '+thisfilter
        if (nin GT 0) then begin
            image= mrdfits(filename,exten)
            thisoutimage= fltarr(bigast.naxis1,bigast.naxis2)
            thisoutimage[round(xx[inimage]),round(yy[inimage])]= $
              image[xflt[inimage],yflt[inimage]]
            if (NOT keyword_set(outimage)) then outimage= thisoutimage $
            else outimage= [[[outimage]],[[thisoutimage]]]
            help, outimage
            if (NOT keyword_set(filter)) then filter= thisfilter $
            else filter= [filter,thisfilter]
            help, filter
        endif
    endfor
endfor

sindx= sort(filter)
outimage= outimage[*,*,sindx]
filter= filter[sindx]
bstart= uniq(filter)
nblock= n_elements(bstart)
for bb=0,nblock-1 do begin
    bimage= median(outimage[*,*,where(filter EQ filter[bstart[bb]])], $
                   dimension=3)
    outfilename= prefix+'_'+filter[bstart[bb]]+'.fits'
    mwrfits, bimage,outfilename,/create
endfor

bim= mrdfits(prefix+'_F555W.fits')
bim= bim-median(bim)
rim= mrdfits(prefix+'_F814W.fits')
rim= rim-median(rim)
nw_rgb_make,rim,rim+bim,bim,name=prefix+'.jpg', $
  scales=[0.02,0.01,0.02],nonlinearity=3,rebinfactor=1

return
end
