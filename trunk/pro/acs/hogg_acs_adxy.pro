;+
; NAME:
;   hogg_acs_adxy
; PURPOSE:
;   Convert RA,Dec to xy using ACS distortions plus WCS info, as is
;   stored in "FLT" files.
; INPUTS:
;   hdr        - ACS FLT image header as read by headfits() or equiv
;   a,d        - input RA,Dec positions
; OUTPUTS:
;   x,y        - output x,y position vectors, after WCS and distortions
;                have been applied.
; COMMENTS:
;   - calls hogg_acs_distort iteratively!
; BUGS:
;   - Is slow.
; REVISION HISTORY:
;   2005-05-24  started - Hogg (NYU)
;-
pro hogg_acs_adxy, hdr,a,d,x,y
tiny= 1d-8 ; pixels
adxy, hdr,a,d,sqx,sqy
x= sqx
y= sqy
repeat begin
    hogg_acs_distort, hdr,x,y,newsqx,newsqy
    x= x-(newsqx-sqx)
    y= y-(newsqy-sqy)
    foo= where(finite(x) EQ 0,ninfinite)
    if (ninfinite GT 0) then begin
        x= -1
        y= -1
        newsqx= sqx
        newsqy= sqy
endrep until (max([newsqx-sqx,newsqy-sqy]) LT tiny)
return
end
