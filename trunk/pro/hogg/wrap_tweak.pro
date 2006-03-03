;+
; NAME:
;   wrap_tweak
; PURPOSE:
;   Run WCS tweak on a generic image.
; INPUTS:
;   imagefile    - FITS file name of image with roughly correct WCS
;   xyfile       - ASCII file name with X,Y,FLUX
; OPTIONAL INPUTS:
;   exten        - which extension of FITS file to look at; default 0
;   siporder     - order for SIP distortions; default 1 (ie, no
;                  distortions)
;   qa           - name of PS file for QA plots
; BUGS:
;   - Not tested.
;   - Dependencies.
; 2006-Mar-02  Finkbeiner
;-
pro wrap_tweak, imagefile, xyfile, exten=exten, $
                siporder=siporder,qa=qa

hdr=headfits(imagefile,exten=exten)
astr=an_tweak(imagefile,exten=exten,xyfile=xyfile,siporder=siporder,qa=qa)

if not keyword_set(astr) then message,'an_tweak FAILED'

; update header
putast,hdr,astr

nline=n_elements(hdr)
for i=0,nline-1 do begin 
    print,hdr[i]
endfor
    
return
end
