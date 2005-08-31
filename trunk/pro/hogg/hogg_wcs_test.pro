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

; add SIP distortions
siporder= 3
acoeffs= dblarr(siporder+1,siporder+1)
bcoeffs= dblarr(siporder+1,siporder+1)
apcoeffs= dblarr(siporder+1,siporder+1)
bpcoeffs= dblarr(siporder+1,siporder+1)
acoeffs[0,2]= 0.001
distort = {name:distort_flag,a:acoeffs,b:bcoeffs,ap:ap,bp:bp}
newastr= create_struct(temporary(newastr),'distort',distort)

; make grid of points
xx= reform((dindgen(10)*naxis1/10)#replicate(1D0,10),100)
yy= reform(replicate(1D0,10)#(dindgen(10)*naxis1/10),100)
plot, xx,yy,psym=1

return
end

