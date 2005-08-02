pro hogg_xyz2ad, xx,yy,zz,aa,dd
radperdeg= !DPI/1.8D2
aa= atan(yy,xx)/radperdeg
dd= asin(zz)/radperdeg
return
end
