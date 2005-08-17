pro hogg_xyz2ad, xx,yy,zz,aa,dd
radperdeg= !DPI/1.8D2
aa= atan(yy,xx)/radperdeg
while (aa LT 0.0) do aa= aa+3.6D2
dd= asin(zz)/radperdeg
return
end
