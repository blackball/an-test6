;+
; NAME:
;   hogg_acs_xyad
; PURPOSE:
;   Convert x,y to RA,Dec using ACS distortions plus WCS info, as is
;   stored in "FLT" files.
; INPUTS:
;   hdr        - ACS FLT image header as read by headfits() or equiv
;   x,y        - input x,y position vectors
; OUTPUTS:
;   a,d        - output RA,Dec positions, after distortions and WCS
;                have been applied.
; COMMENTS:
;   - calls hogg_acs_distort
; BUGS:
; REVISION HISTORY:
;   2005-05-24  started - Hogg (NYU)
;-
pro hogg_acs_xyad, hdr,x,y,a,d
hogg_acs_distort, hdr,x,y,sqx,sqy
adxy, hdr,sqx,sqy,a,d
return
end
