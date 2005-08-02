;+
; NAME:
;   hogg_points2astr
; PURPOSE:
;   use 2 image points to construct an approximate astrometric header
; INPUTS:
;   coords   - [2,5] array of U,V (image coords) and x,y,z (unit-vector
;              on the sphere coords) of 2 points.
;   parity   - 0 if parity is the usual astronomical parity, 1 if not
; OUTPUTS:
;   astr     - astrometric structure, which can be used by Goddard
;              routines
; BUGS:
;   - Fixes the tangent point automatically; ought to take an input
;   tangent point in U,V coordinates.  Unfortunately, this will make
;   the code substantially more complicated.
; REVISION HISTORY:
;   2005-08-01  started - Hogg (NYU)
;-
function hogg_pta_norm, vec
return, sqrt(total(vec^2))
end

function hogg_pta_cross, vec1,vec2
return, [vec1[1]*vec2[2],vec1[2]*vec2[0],vec1[0]*vec2[1]]
end

function hogg_points2astr, coords,parity
if keyword_set(parity) then uinx= 1 else uindx= 0

; split input into vectors, "im" for in image, "sp" for on sphere,
; and "tp" for on tangent plane
imE= double(reform(coords[0,uindx:1-uindx],2))
imF= double(reform(coords[1,uindx:1-uindx],2))
spE= double(reform(coords[0,2:4],3))
spE= spE/hogg_pta_norm(spE) ; not necessary
spF= double(reform(coords[1,2:4],3))
spF= spE/hogg_pta_norm(spF) ; not necessary

; compute pointing
imT= 0.5*(imE+imF)
foo= spE+spF
spT= foo/hogg_pta_norm(foo)
spZ= double([0,0,1])
if ((transpose(spZ)#spT) EQ 1.0) then begin
    imT= imE
    spT= spE
endif
splog, "pointing: image point",imT," points in direction",spT

; compute scale
tpE= spE/(transpose(spT)#spE)
tpF= spF/(transpose(spT)#spF)
scale= hogg_pta_norm(tpF-tpE)/hogg_pta_norm(imE-imF)
splog, "scale:",scale

; compute rotation
foo= spZ-(transpose(spT)#spZ)*spT
eta= foo/hogg_pta_norm(foo)
xi= hogg_pta_cross(spT,eta)
splog, "testing:",hogg_pta_norm(xi),hogg_pta_norm(eta)
rotation= atan(transpose(eta)#(tpF-tpE),transpose(xi)#(tpF-tpE)) $
  -atan(imF[1]-imE[1],imF[0]-imE[0])
splog, "rotation:",rotation

return, -1
end
