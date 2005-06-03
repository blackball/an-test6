pro testy
prefix= 'test'
racen= 15D0*(11.+31./60.+51.6/3600.)
deccen= -1.0*(12D0+31./60.+57.0/3600.)
dra= 0.30/60.0
pixscale= 0.025/3600.0
astr= smosaic_hdr(racen,deccen,dra,dra,pixscale=pixscale)

stop

filelist= findfile('./*_flt.fit*')
for ii=0,n_elements(filelist)-1 do begin
    filename= filelist[ii]
    hdr0= headfits(filename)
    thisfilter= hogg_acs_filter(hdr0)
    if (thisfilter EQ '0') then begin
        splog, filename+' is not an ACS image, apparently; skipping'
    endif else for exten=1,sxpar(hdr0,'NEXTEND'),3 do begin
        thisoutimage= hogg_acs_cutout(filename,exten,astr, $
                                      [astr.naxis1,astr.naxis2], $
                                      xx=xx,yy=yy,aa=aa,dd=dd)
        if (n_elements(thisoutimage) EQ n_elements(xx)) then begin
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
  scales=[0.02,0.01,0.02],nonlinearity=3,rebinfactor=2

return
end
