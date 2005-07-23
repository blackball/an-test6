;+
; NAME:
;   hizzle
; PURPOSE:
;   Combine multiple HST/ACS images into a single mosaic.
; CALLING SEQUENCE:
;   image= hizzle(astr,filelist,outfile,naxis=[naxis1,naxis2])
; COMMENTS:
;   - Philosophically, this differs from STScI "drizzle" in that it
;     treats the input images as constraints on an intensity map,
;     *not* "flux catchers".
;   - Based on Hogg's ACS WCS routines and Blanton's image combination
;     routines.
;   - Takes as input the astrometric header for the image you *want*
;     to make, and a list of HST/ACS "_FLT.fits" files; returns a
;     mosaic with the same calibration properties as those produced by
;     STScI "drizzle".
;   - Although this code treats the input images as constraints on the
;     intensity, the output has units of flux per pixel, for ease of
;     use by conventional astronomical tools.
; INPUTS:
;   astr      - vanilla astrometric WCS structure -- or FITS header
;               containting such -- for the output image
;   filelist  - list of HST/ACS "_FLT.fits" files; ought to be all in
;               the same bandpass
;   outfile   - name of output FITS file to make; if not set, no
;               output is made
; OPTIONAL INPUTS:
;   naxis     - [2] array of naxis1,naxis2; not necessary if they are
;               not elements (or header cards) already existing in
;               astr; astr over-rides if they conflict
; OUTPUTS:
;   image     - the image that was saved in the FITS file
; BUGS:
;   - Not yet written.
;   - Must figure out how to create/read/calculate weights.
;   - Must figure out stuff about pixel rejection.
;   - Finds useful pixels *stoopidly*; it *will* run out of memory.
; REVISION HISTORY:
;   2005-07-21  started - Hogg (NYU)
;-
function hizzle, orig_astr,filelist,outfile,naxis=naxis

; deal with ridiculous NAXIS issues
if (type(orig_astr,/name) EQ 'STRING') then begin
    extast, orig_astr,astr
    naxis1= sxpar(orig_astr,'NAXIS1')
    naxis2= sxpar(orig_astr,'NAXIS2')
endif else begin
    astr= orig_astr
    if (tag_exists(orig_astr,'naxis1') and $
        tag_exists(orig_astr,'naxis2')) then begin
        naxis1= orig_astr.naxis1
        naxis2= orig_astr.naxis2
    endif
endelse
if (not keyword_set(naxis1)) then begin
    if (not keyword_set(naxis)) then begin
        splog, 'ERROR: NAXIS values required in either ASTR input or NAXIS input'
        splog, 'ERROR: see code header for more information'
        return, 0
    endif
    naxis1= naxis[0]
    naxis2= naxis[1]
endif

; loop over input images, looking for pixels that might be useful
nfile= n_elements(filelist)
for ii=0L,nfile-1L do for exten=0,1 do begin
    splog, 'reading '+filelist[ii]+' exten',exten
    inimage= mrdfits(filelist[ii],inhdr)
    foo= size(inimage,/dimens)
    nx= foo[0]
    ny= foo[1]
    xx= dindgen(nx)#replicate(1D0,ny) ; check this
    yy= replicate(1D0,ny)#dindgen(ny) ; check this
    hogg_acs_xyad, inhdr,temporary(xx),temporary(yy),aa,dd
    ad2xy, temporary(aa),temporary(dd),astr,xx,yy
    inside= where((xx GT 0) and (xx LT naxis1) and $
                  (yy GT 0) and (yy LT naxis2),ninside)
    if (ninside LT 1) then begin
        splog, 'no pixels to use in '+filelist[ii]+' exten',exten
    endif else begin
        if keyword_set(inputx) then $
          inputx= [inputx,xx[inside]] else $
          inputx= xx[inside]
        if keyword_set(inputy) then $
          inputy= [inputy,yy[inside]] else $
          inputy= yy[inside]
        if keyword_set(inputflux) then $
          inputflux= [inputflux,inimage[inside]] else $
          inputflux= inimage[inside]
    endelse
endfor

; call the image combination code
whatever, inputx,inputy,inimage,inweight,image

return, image
end

