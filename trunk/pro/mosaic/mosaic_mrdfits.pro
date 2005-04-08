function mosaic_mrdfits,filename,hdu,header

return,mrdfits(filename,hdu)+32768l
end
