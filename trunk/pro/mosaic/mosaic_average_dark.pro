pro mosaic_average_dark,filelist,avzero,darkname

nfiles=n_elements(filelist)

hdr0=headfits(filelist[0])
naxis=sxpar(hdr0,'NAXIS')
if (naxis eq 2) then begin
    firsthdu=0
endif else begin
    mwrfits,0,darkname,/create
    firsthdu=1
endelse

lasthdu=sxpar(hdr0,'NEXTEND')
for hdu=firsthdu,lasthdu do begin
    splog, 'working on hdu',hdu
    splog, 'zero file is ',avzero
    zero=mrdfits(avzero,hdu)

    hdr=headfits(filelist[0],exten=hdu)
    mosaic_data_section, filelist[0],hdu,xmin,xmax,ymin,ymax,hdr=hdr
    xsize= xmax-xmin+1
    ysize= ymax-ymin+1
    dark=intarr(xsize,ysize,nfiles)

    for i=0,nfiles-1 do begin
        splog, 'reading file', filelist[i]
        hdr=headfits(filelist[i])
        darktime=sxpar(hdr,'DARKTIME')
        splog, 'darktime',darktime
        dark[*,*,i]=(mosaic_mrdfits(filelist[i],hdu,crosstalk=crosstalk) $
                     -zero)/darktime
    endfor
    avsigclip= float(djs_avsigclip(temporary(dark),sigre=3,maxiter=10))
    mwrfits,avsigclip,darkname
endfor
return
end
