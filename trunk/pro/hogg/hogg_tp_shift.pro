;+
; NAME:
;   hogg_tp_shift
; PURPOSE:
;   Shift tangent point on the sphere (CRVAL, in RA, Dec units),
;   adjusting simultaneously the CD matrix (to deal with coordinate
;   issues near the celestial poles).
; INPUTS:
;   astr   - astrometry structure
;   crval  - new crval (tangent point on the sphere) to insert
; BUGS:
;   - no comment header
;-
function hogg_tp_shift, astr,crval
ra_old=  astr.crval[0]
ra_new=  crval[0]
dec_new= crval[1]
deltaorient= (ra_new-ra_old)*!DPI/180D0*sin(dec_new*!DPI/180D0)
newastr= astr
newastr.crval= crval
newastr.cd= bigast.cd#[[ cos(deltaorient), sin(deltaorient)], $
                      [-sin(deltaorient), cos(deltaorient)]]
return, newastr
end
