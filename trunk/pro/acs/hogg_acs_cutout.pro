;+
; NAME:
;   hogg_acs_cutout
; PURPOSE:
;   Return, for an ACS "_FLT" image, a cutout on an new (input) WCS
;     grid.
; CALLING SEQUENCE:
;   cutout= hogg_acs_cutout('
; INPUTS:
;   fltname  - name of "_FLT" file
;   exten    - image extension; should be 1,4,7, I *think*
;   astr     - astrometry structure for cutout
;   naxis    - [naxis1,naxis2] size of cutout
; OPTIONAL INPUTS:
;   xx,yy    - internal inputs that can be sent in to speed up the
;              code on repeat calls with the same "astr" and "naxis"
;   aa,dd    - ditto
; OUTPUT:
;   cutout   - output cutout image
; OPTIONAL OUTPUTS:
;   error    - output error values for the image
;   hdr      - primary header
;   exhdr    - header for the EXTEN extension
; BUGS:
;   - Error output not yet tested.
;   - Super-dumb implementation; many things could be done to save
;     time.
;   - Uses *nearest neighbor* interpolation; use only for input astr
;     with tiny pixel scale (eg 0.025 or smaller).
;   - Depends on photoop -- it wouldn't if I made an equivalent of
;     smosaic_hdr in idlutils (there should be one there anyway).
; COMMENTS:
; DEPENDENCIES:
;   idlutils
;   photoop (ugh)
; REVISION HISTORY:
;   2005-05-25  started - Hogg (NYU)
;   2005-06-01  made ready for public abuse - Hogg
;-
function hogg_acs_cutout, fltname,exten,astr,naxis,error=error, $
                          hdr=hdr,exhdr=exhdr, $
                          xx=xx,yy=yy,aa=aa,dd=dd
if (not keyword_set(xx)) then xx= dindgen(naxis[0])#replicate(1,naxis[1])
if (not keyword_set(yy)) then yy= replicate(1,naxis[0])#dindgen(naxis[1])
if (not (keyword_set(aa) and keyword_set(dd))) then $
  xy2ad, xx,yy,astr,aa,dd
hdr= headfits(fltname)
exhdr= headfits(fltname,exten=exten)
hogg_acs_adxy, exhdr,aa,dd,xflt,yflt
xflt= round(xflt)               ; WARNING: nearest neighbor!
yflt= round(yflt)
naxis1= sxpar(exhdr,'NAXIS1')
naxis2= sxpar(exhdr,'NAXIS2')
inimage= where((xflt GE 0) AND $
               (xflt LT (naxis1-1)) AND $
               (yflt GE 0) AND $
               (yflt LT (naxis2-1)),nin)
splog, nin,' pixels in exten',exten,' of '+fltname
if (nin GT 0) then begin
    inimage= mrdfits(fltname,exten)
    image= fltarr(naxis[0],naxis[1])
    image[round(xx[inimage]),round(yy[inimage])]= $
      image[xflt[inimage],yflt[inimage]]
    if arg_present(error) then begin
        inerror= mrdfits(fltname,exten+1)
        error= fltarr(naxis[0],naxis[1])
        error[round(xx[inimage]),round(yy[inimage])]= $
          error[xflt[inimage],yflt[inimage]]
    endif
endif else begin
    splog, 'WARNING: no overlapping pixels, returning -1'
    return, -1
endelse
return, image
end
