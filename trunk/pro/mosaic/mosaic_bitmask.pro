pro mosaic_bitmask,avzero,avdark,gflat,bitmaskname

mwrfits,0,bitmaskname,/create
for hdu=1,8 do begin

zero=mrdfits(avzero,hdu)
bitmask=((zero gt 1e4) or (zero lt 1e3))
dark=mrdfits(avdark,hdu)
bitmask=bitmask+2*((dark gt .01) or (zero lt -.01))
flat=mrdfits(gflat,hdu)
bitmask=bitmask+4*((flat gt 3.) or (flat lt .1))


endfor
return

end