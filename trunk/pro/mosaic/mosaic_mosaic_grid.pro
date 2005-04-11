;+
; NAME:
;   mosaic_mosaic_grid
; PURPOSE:
;   Make grid of mosaics of KPNO Mosaic imaging
; INPUTS:
;   filelist      - list of files to use
;   prefix        - prefix to use for output file names
;   racen,deccen  - center (J2000 deg) of central image
;   dra,ddec      - size (deg) of each individual image in the grid
;   nx,ny         - numbers of image to make in the RA and Dec
;                   directions; must be odd
; BUGS:
; REVISION HISTORY:
;   - 2005-04-10  started under duress - Hogg
;-
pro mosaic_mosaic_grid, filelist,prefix,racen,deccen,dra,ddec,nx,ny

if (NOT keyword_set(prefix)) then prefix= 'UMa_dwarf_g_'
if (NOT keyword_set(racen)) then racen= 158.72
if (NOT keyword_set(deccen)) then deccen= 51.92
if (NOT keyword_set(dra)) then dra= 6.0/60.0
if (NOT keyword_set(ddec)) then ddec= dra
if (NOT keyword_set(nra)) then nra= 7
if (NOT keyword_set(ndec)) then ndec= nra

pixscale=.26/3600.0
center= smosaic_hdr(racen,deccen,dra,ddec,pixscale=pixscale)

for ii=-(nx-1)/2,(nx-1)/2 do begin
    iistr= strtrim(string(ii),2)
    if (ii GE 0) then iistr= '+'+iistr
    for jj=-(ny-1)/2,(ny-1)/2 do begin
        jjstr= strtrim(string(jj),2)
        if (jj GE 0) then iistr= '+'+jjstr

        filename= prefix+iistr+jjstr+'.fits'
        splog, 'starting work on '+filename
        bigast= center
        bigast.crpix= center.crpix-[double(ii)*double(center.naxis1), $
                                    double(jj)*double(center.naxis2)]
        mosaic_mosaic, filelist,filename,bigast=bigast
    endfor
endfor
return
end

