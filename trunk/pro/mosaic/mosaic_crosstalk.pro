;+
; NAME:
;   mosaic_crosstalk
; PURPOSE:
;   Estimate all 64-8 mosaic chip crosstalk terms
; INPUTS:
;   filename   - name of a raw Mosaic filename to read/test
; BUGS:
;   - sky grid size hard-coded.
; REVISION HISTORY:
;   2005-05-05  started - Hogg
;-
function mosaic_crosstalk_one, vec1,vec2

; make masks of bright pixels
quantile= weighted_quantile(vec2,quant=[0.05,0.95])
mask2= ((vec2 GE quantile[0]) AND (vec2 LE quantile[1]))

; find amplitude by least-squares?
aa= [[double(vec1*mask2)],[double(mask2)]]
aataa= transpose(aa)#aa
aataainvaa= invert(aataa,/double)
aatyy= aa##[[double(vec2)]]
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
        image1= (mosaic_mrdfits(filename,hdu1,hdr1))[xmin:xmax,ymin:ymax]
        mosaic_data_section, filename,hdu2,xmin,xmax,ymin,ymax,hdr=hdr
        image2= (mosaic_mrdfits(filename,hdu2,hdr2))[xmin:xmax,ymin:ymax]
        if (((sxpar(hdr1,'ATM1_1')*sxpar(hdr2,'ATM1_1')) EQ (-1)) AND $
            ((sxpar(hdr1,'ATM2_2')*sxpar(hdr2,'ATM2_2')) EQ (-1))) then $
          image2= rotate(image2,2)

; estimate and subtract sky
        bw_est_sky, image1,sky1
        image1= image1-temporary(sky1)
        bw_est_sky, image2,sky2
        image2= image2-temporary(sky2)

; reform
        npix= n_elements(image1)
        image1= reform(image1,npix)
        image2= reform(image2,npix)

; compute and store
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
