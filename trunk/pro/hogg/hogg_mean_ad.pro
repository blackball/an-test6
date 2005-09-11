;+
; BUGS:
;   - No proper header.
;-
pro hogg_mean_ad, aa,dd,meanaa,meandd,weight=weight
if (NOT keyword_set(weight)) then weight= 1D0
angles_to_xyz, 1D0,aa,9D1-dd,xx,yy,zz
xx= total(xx*weight)
yy= total(yy*weight)
zz= total(zz*weight)
norm= sqrt(xx^2+yy^2+zz^2)
xx= xx/norm
yy= yy/norm
zz= zz/norm
xyz_to_angles, 1D0,xx,yy,zz,meanaa,meantt
meandd= 9D1-meantt
return
end
