;+
; NAME:
;   hogg_mean_ad
; BUGS:
;   - No proper header.
;-
pro hogg_mean_ad, aa,dd,meanaa,meandd,weight=weight
if (NOT keyword_set(weight)) then weight= replicate(1D0,n_elements(aa))
angles_to_xyz, weight,aa,9D1-dd,xx,yy,zz
xx= total(xx)
yy= total(yy)
zz= total(zz)
xyz_to_angles, xx,yy,zz,rr,meanaa,meantt
meandd= 9D1-meantt
return
end
