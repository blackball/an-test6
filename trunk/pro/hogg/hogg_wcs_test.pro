;+
; NAME:
;   hogg_wcs_test.pro
; PURPOSE:
;   A test suite for the TAN-SIP WCS routines.
;-
pro hogg_wcs_test

; make vanilla WCS
racen= 190.0
deccen= 10.0
astr= smosaic_hdr(racen,deccen,0.1,0.1,pixscale=1.0/3600.)
naxis1= astr.naxis1
naxis2= astr.naxis2

; make "forward" distortion parameters -- randomly
siporder= 2
distort_flag= 'SIP'
acoeffs= dblarr(siporder+1,siporder+1)
bcoeffs= dblarr(siporder+1,siporder+1)
apcoeffs= acoeffs
bpcoeffs= bcoeffs
for order=2,siporder do for jj=0,order do begin
    norm= 20.0/naxis1^(order-jj)/naxis2^jj
    acoeffs[order-jj,jj]= norm*randomn(seed)
    bcoeffs[order-jj,jj]= norm*randomn(seed)
endfor
distort = {name:distort_flag,a:acoeffs,b:bcoeffs,ap:apcoeffs,bp:bpcoeffs}
astr= create_struct(temporary(astr),'distort',distort)

; make "backward" distortion parameters
ng= siporder+1
xx= reform((dindgen(ng)*naxis1/(ng-1))#replicate(1D0,ng),ng*ng)
yy= reform(replicate(1D0,ng)#(dindgen(ng)*naxis1/(ng-1)),ng*ng)
xy2ad, xx,yy,astr,aa,dd
ad2xy, aa,dd,astr,xxback,yyback
splog, 'mean square error before AP, BP set:', $
  (total((xxback-xx)^2)+total((yyback-yy)^2))/n_elements(xx)
amatrix= dblarr((siporder+1)*(siporder+2)/2-3,ng*ng)
kk= 0
xdif= xxback-(astr.crval[0]-1)
ydif= yyback-(astr.crval[0]-1)
for order=2,siporder do for jj=0,order do begin
    amatrix[kk,*]= xdif^(order-jj)*ydif^jj
    kk= kk+1
endfor
atainv= invert(transpose(amatrix)##amatrix)
xxpars= atainv##(transpose(amatrix)##(xx-xxback))
yypars= atainv##(transpose(amatrix)##(yy-yyback))
kk= 0
for order=2,siporder do for jj=0,order do begin
    astr.distort.ap[order-jj,jj]= xxpars[kk]
    astr.distort.bp[order-jj,jj]= yypars[kk]
    kk= kk+1
endfor

; test backward parameters
ad2xy, aa,dd,astr,xxback,yyback
splog, 'mean square error after AP, BP set:', $
  (total((xxback-xx)^2)+total((yyback-yy)^2))/n_elements(xx)

return
end

