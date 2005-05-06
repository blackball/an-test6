;+
; NAME:
;   mosaic_crosstalk
; PURPOSE:
;   Estimate all 64-8 mosaic chip crosstalk terms
; INPUTS:
;   filename   - name of a raw Mosaic filename to read/test
; REVISION HISTORY:
;   2005-05-05  started - Hogg
;-
function mosaic_crosstalk_one, image1,image2

; make masks of bright pixels
quantile= weighted_quantile(image2,quant=0.75)
mask2= (image2 LE quantile)

; find amplitude by least-squares?
mean2= total(image2*mask2,/double)/total(mask2,/double)
aa= [[double(image1*mask2)],[double(mean2*mask2)]]
aataa= transpose(aa)#aa
aataainvaa= invert(aataa,/double)
aatyy= aa##[[double(image2)]]
xx= aataainvaa##aatyy

return, xx[0]
end

pro mosaic_crosstalk, filename
crosstalk= dblarr(8,8)

; deal with file names
tmp= strsplit(filename,'/',/extract)
prefix= tmp[n_elements(tmp)-1]
prefix= strmid(prefix,0,strpos(prefix,'.fit'))
crosstalkname= prefix+'_crosstalk.fits'

; no clobber
if file_test(crosstalkname) then begin
    splog, 'file '+crosstalkname+' already exists; returning'
    return
endif

splog, 'working on '+filename

; loop over all eligible pairs
for hdu1=1,8 do begin
    for hdu2=hdu1+1,8 do begin

; read in all hdu1 and hdu2
        mosaic_data_section, filename,hdu1,xmin,xmax,ymin,ymax,hdr=hdr
        image1= (mosaic_mrdfits(filename,hdu1))[xmin:xmax,ymin:ymax]
        npix= n_elements(image1)
        image1= reform(image1,npix)
        mosaic_data_section, filename,hdu2,xmin,xmax,ymin,ymax,hdr=hdr
        image2= (mosaic_mrdfits(filename,hdu2))[xmin:xmax,ymin:ymax]
        image2= reform(image2,npix)

        crosstalk[hdu1-1,hdu2-1]= mosaic_crosstalk_one(image1,image2)
        crosstalk[hdu2-1,hdu1-1]= mosaic_crosstalk_one(image2,image1)
        splog, 'contribution of',hdu1,' to',hdu2,' is',crosstalk[hdu1-1,hdu2-1]
        splog, 'contribution of',hdu2,' to',hdu1,' is',crosstalk[hdu2-1,hdu1-1]
    endfor
endfor

; write output
splog, 'writing '+crosstalkname
mwrfits, crosstalk,crosstalkname,/create
return
end
