;+
; NAME:
;   mosaic_mosaic_grid_combine
; PURPOSE:
;   Combine into one mosaic a NX x NY grid of mosaics from mosaic_mosaic_grid
; INPUTS:
;   prefix        - prefix to use for output file names
;   nx,ny         - numbers of images in the RA and Dec directions;
;                   must be odd
; BUGS:
; REVISION HISTORY:
;   2005-04-10  started under duress - Hogg
;-
pro mosaic_mosaic_grid_combine, prefix,nx,ny

for ii=-(nx-1)/2,(nx-1)/2 do begin
    iistr= strtrim(string(ii),2)
    if (ii GE 0) then iistr= '+'+iistr
    for jj=-(ny-1)/2,(ny-1)/2 do begin
        jjstr= strtrim(string(jj),2)
        if (jj GE 0) then jjstr= '+'+jjstr

        filename= prefix+'_'+iistr+jjstr+'.fits'
        if (NOT keyword_set(column)) then column= mrdfits(filename) $
        else column= [[mrdfits(filename)],[column]]
        help, column
    endfor

    if (NOT keyword_set(canvas)) then canvas= temporary(column) $
    else canvas= [temporary(column),canvas]
    help, canvas
endfor
mwrfits, canvas,prefix+'.fits',/create
end
