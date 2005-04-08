function mosaic_mrdfits,filename,hdu,header
hdr=headfits(filename,exten=hdu)
bitpix=sxpar(hdr,'BITPIX')
if bitpix ne 16 then begin
print,'this is not a raw image!'
return,-1
endif
return,mrdfits(filename,hdu)+32768l
end
