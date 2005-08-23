;+
; BUGS:
;   - No proper header.
;-
pro hogg_mean_ad, aa,dd,meanaa,meandd,weight=weight
if (NOT keyword_set(weight)) then weight= 1D0
hogg_ad2xyz, aa,dd,xx,yy,zz
xx= total(xx*weight)
yy= total(yy*weight)
zz= total(zz*weight)
norm= sqrt(xx^2+yy^2+zz^2)
xx= xx/norm
yy= yy/norm
zz= zz/norm
hogg_xyz2ad, xx,yy,zz,meanaa,meandd
return
end
