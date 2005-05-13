;+
; COMMENTS:
;   Pass "crosstalk=dblarr(8,8)" to force no crosstalk correction.
; BUGS:
;   - no correct code header
;   - reads crosstalk file but doesn't use it
;-
function mosaic_mrdfits,filename,hdu,hdr,crosstalk=crosstalk
if (NOT keyword_set(crosstalk)) then begin
    crosstalkname= './mosaic_crosstalk.fits'
    if file_test(crosstalkname) then begin
        crosstalk= mrdfits('./mosaic_crosstalk.fits')
    endif else begin
        splog, 'ERROR: need file '+crosstalkname
        return, -1
    endelse
endif
hdr= headfits(filename,exten=hdu)
bitpix= sxpar(hdr,'BITPIX')
if bitpix ne 16 then begin
    splog, 'ERROR: this is not a raw image! returning'
    return, -1
endif
splog, 'reading HDU',hdu,' of '+filename
image= float(mrdfits(filename,hdu,hdr,/silent))+32768.0
mosaic_data_section, filename,hdu,xmin,xmax,ymin,ymax,hdr=hdr
crosstalkers= where(crosstalk[*,hdu-1] GT 1d-4,ncross)
for ii=0,ncross-1 do begin
    coeff= float(crosstalk[crosstalkers[ii],hdu-1])
    splog, 'removing HDU',crosstalkers[ii]+1,' crosstalk with coeff',coeff
    image= image-coeff $
      *(float(mrdfits(filename,(crosstalkers[ii]+1),/silent))+32768.0)
endfor
return, image[xmin:xmax,ymin:ymax]
end
