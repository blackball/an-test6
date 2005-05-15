;+
; NAME:
;   mosaic_crosstalk
; PURPOSE:
;   Estimate all mosaic chip crosstalk terms
; INPUTS:
;   filename   - name of a raw Mosaic filename to read/test
; KEYWORDS:
;   redo       - assume that this is a flattened file, not a raw file,
;                and this is a re-do of the calculation.
; BUGS:
;   - image size hard-coded
;   - sub-section size hard-coded
;   - only does neighboring HDUs to save time (since we know that
;     there is only cross-talk on neighbors)
; REVISION HISTORY:
;   2005-05-05  started - Hogg
;   2005-05-15  only do neighboring HDUs - Hogg
;-
function mosaic_crosstalk_one, image1,image2,x1=x1,y1=y1

; find best subsection
nx= 256L
ny= 512L
if (n_elements(x1) EQ 0) then begin
    bestsig= 0.
    for x1=nx/2,2048-3*nx/2,nx/2 do for y1=ny/2,4096-3*ny/2,ny/2 do begin
        sig= djsig(image1[x1:x1+nx-1,y1:y1+ny-1],sigrej=100.0,maxiter=1)
        if (sig GT bestsig) then begin
            bestsig= sig
            bestx1= x1
            besty1= y1
        endif
    endfor
    x1= bestx1
    y1= besty1
endif
splog, 'best section starts at',x1,y1

; reform
npix= nx*ny
vec1= reform(image1[x1:x1+nx-1,y1:y1+ny-1],npix)
vec2= reform(image2[x1:x1+nx-1,y1:y1+ny-1],npix)
xxx= reform(dindgen(nx)#(dblarr(ny)+1),npix)-(double(nx-1)/2)
yyy= reform((dblarr(nx)+1)#dindgen(ny),npix)-(double(ny-1)/2)

; make masks of extreme pixels -- NB that we DON'T want extreme pixels
;                                 in vec2, and we DO want extreme
;                                 pixels in vec1
quant1= weighted_quantile(vec1,quant=[0.05,0.95])
quant2= weighted_quantile(vec2,quant=[0.01,0.99])
mask= (((vec1 LE quant1[0]) OR (vec1 GE quant1[1])) AND $
       (vec2 GE quant2[0]) AND (vec2 LE quant2[1]))

; find amplitude by least-squares?
aa= [[double(vec1*mask)],[double(mask)],[xxx*mask],[yyy*mask]]
aataa= transpose(aa)#aa
aataainvaa= invert(aataa,/double)
aatyy= aa##[[double(vec2)]]
xx= aataainvaa##aatyy

return, xx[0]
end

pro mosaic_crosstalk, filename,redo=redo
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
    if (n_elements(im1x) GT 0) then foo= temporary(im1x)
    if (n_elements(im1y) GT 0) then foo= temporary(im1y)

; read in hdu1
    if keyword_set(redo) then $
      image1= mrdfits(filename,hdu1,hdr1) $
    else $
      image1= mosaic_mrdfits(filename,hdu1,hdr1, $
                             crosstalk=dblarr(8,8))

; read in hdu2
    hdu2= hdu1+1
    if keyword_set(redo) then $
      image2= mrdfits(filename,hdu2,hdr2) $
    else $
      image2= mosaic_mrdfits(filename,hdu2,hdr2, $
                             crosstalk=dblarr(8,8))
    if (((sxpar(hdr1,'ATM1_1')*sxpar(hdr2,'ATM1_1')) EQ (-1)) AND $
        ((sxpar(hdr1,'ATM2_2')*sxpar(hdr2,'ATM2_2')) EQ (-1))) then $
      image2= rotate(image2,2)

; compute and store
    crosstalk[hdu1-1,hdu2-1]= mosaic_crosstalk_one(image1,image2, $
                                                   x1=im1x,y1=im1y)
    splog, 'contribution of',hdu1,' to',hdu2,' is',crosstalk[hdu1-1,hdu2-1]
    crosstalk[hdu2-1,hdu1-1]= mosaic_crosstalk_one(image2,image1)
    splog, 'contribution of',hdu2,' to',hdu1,' is',crosstalk[hdu2-1,hdu1-1]
endfor

; write output
splog, 'writing '+crosstalkname
mwrfits, crosstalk,crosstalkname,/create
return
end
