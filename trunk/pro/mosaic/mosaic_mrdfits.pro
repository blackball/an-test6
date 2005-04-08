function mosaic_mrdfits,filename,hdu,hdr
hdr=headfits(filename,exten=hdu)
bitpix=sxpar(hdr,'BITPIX')
if bitpix ne 16 then begin
splog, 'ERROR: this is not a raw image! returning'
return, -1
endif
return, mrdfits(filename,hdu)+32768l
end
