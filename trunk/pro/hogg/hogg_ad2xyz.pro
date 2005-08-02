pro hogg_ad2xyz, aa,dd,xx,yy,zz
radperdeg= !DPI/1.8D2
xx= cos(aa*radperdeg)*cos(dd*radperdeg)
yy= sin(aa*radperdeg)*cos(dd*radperdeg)
zz= sin(dd*radperdeg)
return
end
