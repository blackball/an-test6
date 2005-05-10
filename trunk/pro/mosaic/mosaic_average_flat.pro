pro mosaic_average_flat,filelist,avzero,avdark,flatname

nfiles=n_elements(filelist)
   
hdr0=headfits(filelist[0])
naxis=sxpar(hdr0,'NAXIS')
if (naxis eq 2) then begin
    firsthdu=0
endif else begin
    mwrfits,0,flatname,/create
    firsthdu=1
endelse

lasthdu=sxpar(hdr0,'NEXTEND')

for hdu=firsthdu,lasthdu do begin
    splog, 'working on hdu',hdu

    zero=mrdfits(avzero,hdu)
    dark=mrdfits(avdark,hdu)

    hdr=headfits(filelist[0],exten=hdu)
    mosaic_data_section, filelist[0],hdu,xmin,xmax,ymin,ymax,hdr=hdr
    xsize= xmax-xmin+1
    ysize= ymax-ymin+1
    flat=intarr(xsize,ysize,nfiles)

    for i=0,nfiles-1 do begin
        splog, 'reading file', filelist[i]
        hdr=headfits(filelist[i])
        darktime=sxpar(hdr,'DARKTIME')
        splog, 'darktime',darktime
        exptime=sxpar(hdr,'EXPTIME')
        splog, 'exptime',exptime

        flat[*,*,i]=(mosaic_mrdfits(filelist[i],hdu,crosstalk=crosstalk) $
                     -zero-dark*darktime)/exptime
    endfor

    avsigclip= float(djs_avsigclip(temporary(flat),sigre=3,maxiter=10))
    if (not keyword_set(norm)) then norm= float(median(avsigclip))
    mwrfits,avsigclip/norm,flatname
endfor
return
end
