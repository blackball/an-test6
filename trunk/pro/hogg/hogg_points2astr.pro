;+
; NAME:
;   hogg_points2astr
; PURPOSE:
;   use 2 image points to construct an approximate astrometric header
; INPUTS:
;   coords   - [5,2] array of U,V (image coords) and x,y,z (unit-vector
;              on the sphere coords) of 2 points.
;   parity   - 0 if parity is the usual astronomical parity, 1 if not
; OUTPUTS:
;   astr     - astrometric structure, which can be used by Goddard
;              routines
; BUGS:
;   - Fixes the tangent point automatically; ought to take an input
;     tangent point in U,V coordinates.  Unfortunately, this will make
;     the code substantially more complicated.
;   - The signs to apply to the sin() terms in the CD matrix are a
;     guess at this point.
; REVISION HISTORY:
;   2005-08-01  started - Hogg (NYU)
;-
function hogg_pta_norm, vec
return, sqrt(total(vec^2))
end

function hogg_pta_cross, vec1,vec2
return, [vec1[1]*vec2[2]-vec1[2]*vec2[1], $
         vec1[2]*vec2[0]-vec1[0]*vec2[2], $
         vec1[0]*vec2[1]-vec1[1]*vec2[0]]
end

function hogg_points2astr, coords,parity
help, coords

; split input into vectors, "im" for in image, "sp" for on sphere,
; and "tp" for on tangent plane
imE= double(reform(coords[0:1,0],2))
imF= double(reform(coords[0:1,1],2))
spE= double(reform(coords[2:4,0],3))
spE= spE/hogg_pta_norm(spE) ; not necessary
spF= double(reform(coords[2:4,1],3))
spF= spF/hogg_pta_norm(spF) ; not necessary

; compute pointing
imT= 0.5*(imE+imF)
foo= spE+spF
spT= foo/hogg_pta_norm(foo)
spZ= double([0,0,1])
if ((transpose(spZ)#spT) EQ 1.0) then begin
    imT= imE
    spT= spE
endif
xyz_to_angles, spT[0],spT[1],spT[2],rr,aa,tt
dd= 9D1-temporary(tt)
adT= [aa,dd]
splog, "pointing: image point",imT," points in direction",adT

; compute scale
tpE= spE/(transpose(spT)#spE)[0]
tpF= spF/(transpose(spT)#spF)[0]
scale= hogg_pta_norm(tpF-tpE)/hogg_pta_norm(imF-imE)
scale= scale*1.8D2/!DPI
splog, "scale:",scale*3.6D3

; compute rotation
foo= spZ-(transpose(spT)#spZ)[0]*spT
eta= foo/hogg_pta_norm(foo)
xi= hogg_pta_cross(spT,eta)
; splog, "testing:",hogg_pta_norm(xi),hogg_pta_norm(eta) $
;   ,(transpose(xi)#eta)[0],(transpose(xi)#spT)[0]
if keyword_set(parity) then sgn= 1D0 else sgn= -1D0
rotation= atan(transpose(eta)#(tpF-tpE),-1D0*transpose(xi)#(tpF-tpE)) $
  -atan(imF[1]-imE[1],sgn*(imF[0]-imE[0]))
splog, "rotation:",rotation

; make header
if keyword_set(orthographic) then ctype= ['RA---SIN','DEC--SIN'] else $
  ctype= ['RA---TAN','DEC--TAN']
make_astr, astr, $
  CD       = double([[ sgn*scale*cos(rotation),sgn*scale*sin(rotation)], $
                     [-1D0*scale*sin(rotation),    scale*cos(rotation)]]), $
  DELT     = double([1.0,1.0]), $
  CRPIX    = double([0.5,0.5])+imT, $ ; NB: FITS CONVENTION (should this be [1,1]?
  CRVAL    = adT, $
  CTYPE    = ctype, $
  LONGPOLE = 1.8D2

return, astr
end
