;+
; BUGS:
;  - No comment header.
;  - Everything hard-coded.
;-
pro mosaic_bitmask,avzero,avdark,gflat,bitmaskname
mwrfits,0,bitmaskname,/create
for hdu=1,8 do begin
zero=mrdfits(avzero,hdu)
medzero=median(zero)
bitmask=((zero gt (1.2*medzero)) or (zero lt (.8*medzero)))
dark=mrdfits(avdark,hdu)
bitmask=bitmask+2*((dark gt .01) or (zero lt -.01))
flat=mrdfits(gflat,hdu)
medflat=median(flat,9)
flatmask=((flat gt 3.) or (flat lt .1) or $
          (flat gt (1.2*medflat)) or (flat lt (.8*medflat) ))
flatmask=(smooth(float(flatmask),3) gt 0.)
bitmask=bitmask+4*flatmask
mwrfits,byte(bitmask),bitmaskname
endfor
return
end
