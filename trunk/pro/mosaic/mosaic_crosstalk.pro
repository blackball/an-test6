;+
; NAME:
;   mosaic_crosstalk
; PURPOSE:
;   Estimate all 64-8 mosaic chip crosstalk terms
; INPUTS:
;   filename   - name of a raw Mosaic filename to read/test
;-
pro mosaic_crosstalk, filename
crosstalk= dblarr(8,8)
tmp= strsplit(filename,'/',/extract)
prefix= tmp[n_elements(tmp)-1]
prefix= strmid(prefix,0,strpos(prefix,'.fit'))
splog, 'working on '+filename

; loop over all eligible pairs
for hdu1=1,8 do begin
    for hdu2=1,8 do if (hdu1 NE hdu2) then begin

; read in all hdu1 and hdu2
        mosaic_data_section, filename,hdu1,xmin,xmax,ymin,ymax,hdr=hdr
        image1= (mosaic_mrdfits(filename,hdu1))[xmin:xmax,ymin:ymax]
        npix= n_elements(image1)
        image1= reform(image1,npix)
        mosaic_data_section, filename,hdu2,xmin,xmax,ymin,ymax,hdr=hdr
        image2= (mosaic_mrdfits(filename,hdu2))[xmin:xmax,ymin:ymax]
        image2= reform(image2,npix)

; make masks of bright pixels
        mask1= 0*byte(image1)+1
        indx= where(image1 GT (weighted_quantile(image1,quant=0.75)))
        mask1[indx]= 0
        mask2= 0*byte(image2)+1
        indx= where(image2 GT (weighted_quantile(image2,quant=0.75)))
        mask2[indx]= 0

; find amplitude by least-squares?
        aa= [[double(image1*mask1)],[double(mask1)]]
        aataa= transpose(aa)#aa
        aataainvaa= invert(aataa,/double)
        aatyy= aa##[[double(image2)]]
        xx= aataainvaa##aatyy
        splog, 'crosstalk between',hdu1,' and',hdu2,' is',xx[0]

        crosstalk[hdu1-1,hdu2-1]= xx[0]
    endif
endfor

; write output
crosstalkname= prefix+'_crosstalk.fits'
splog, 'writing '+crosstalkname
mwrfits, crosstalk,crosstalkname,/create
return
end
