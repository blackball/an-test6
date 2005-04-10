;+
; NAME:
;   mosaic_mosaic_grid
; PURPOSE:
;   Make grid of mosaics of KPNO Mosaic imaging
; INPUTS:
;   filelist      - list of files to use
;   prefix        - prefix to use for output file names
;   racen,deccen  - center of image (J2000 deg)
;   dra,ddec      - size of image (deg)
;   nra,ndec      - numbers of image to make in the RA and Dec
;                   directions; must be odd
; BUGS:
; REVISION HISTORY:
;   - 2005-04-10  started under duress - Hogg
;-
pro mosaic_mosaic_grid, filelist,prefix,racen,deccen,dra,ddec,nra,ndec

pixscale=.26/3600.0
center= smosaic_hdr(racen,deccen,dra,ddec,pixscale=pixscale)

for ii=-(nra-1)/2,(nra-1)/2 do begin
    iistr= strtrim(string(ii),2)
    if (ii GE 0) then iistr= '+'+iistr
    for jj=-(ndec-1)/2,(ndec-1)/2 do begin
        jjstr= strtrim(string(jj),2)
        if (jj GE 0) then iistr= '+'+jjstr

        filename= prefix+iistr+jjstr+'.fits'
        bigast= center
        bigast.crpix= center.crpix-[double(ii)*double(center.naxis1), $
                                    double(jj)*double(center.naxis2)]
        mosaic_mosaic, filelist,filename,bigast=bigast
    endfor
endfor
return
end

