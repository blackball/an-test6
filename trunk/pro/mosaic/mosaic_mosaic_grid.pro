;+
; NAME:
;   mosaic_mosaic_grid
; PURPOSE:
;   Make NX x NY grid of mosaics of KPNO Mosaic imaging
; INPUTS:
;   filelist      - list of files to use
;   prefix        - prefix to use for output file names
;   racen,deccen  - center (J2000 deg) of central image
;   dra,ddec      - size (deg) of each individual image in the grid
;   nx,ny         - numbers of image to make in the RA and Dec
;                   directions; must be odd
; OPTIONAL INPUTS:
;   bitmaskname
; BUGS:
; REVISION HISTORY:
;   2005-04-10  started under duress - Hogg
;-
pro mosaic_mosaic_grid, filelist,prefix,racen,deccen,dra,ddec,nx,ny, $
                        bitmaskname=bitmaskname

pixscale=.40/3600.0
center= hogg_make_astr(racen,deccen,dra,ddec,pixscale=pixscale,npixround=8)

for ii=-(nx-1)/2,(nx-1)/2 do begin
    iistr= strtrim(string(ii),2)
    if (ii GE 0) then iistr= '+'+iistr
    for jj=-(ny-1)/2,(ny-1)/2 do begin
        jjstr= strtrim(string(jj),2)
        if (jj GE 0) then jjstr= '+'+jjstr

        filename= prefix+'_'+iistr+jjstr+'.fits'
        splog, 'starting work on '+filename
        bigast= center
        bigast.crpix= center.crpix+[double(ii)*double(center.naxis[0]), $
                                    double(jj)*double(center.naxis[1])]
        mosaic_mosaic, filelist,filename,bigast=bigast, $
          bitmaskname=bitmaskname
    endfor
endfor
return
end

