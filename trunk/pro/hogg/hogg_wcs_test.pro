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
siporder= 3
distort_flag= 'SIP'
acoeffs= dblarr(siporder+1,siporder+1)
bcoeffs= dblarr(siporder+1,siporder+1)
apcoeffs= acoeffs
bpcoeffs= bcoeffs
for order=2,siporder-1 do for jj=0,order do begin
    norm= 20.0/naxis1^(order-jj)/naxis2^jj
    acoeffs[order-jj,jj]= norm*randomn(seed)
    bcoeffs[order-jj,jj]= norm*randomn(seed)
endfor
distort = {name:distort_flag,a:acoeffs,b:bcoeffs,ap:apcoeffs,bp:bpcoeffs}
astr= create_struct(temporary(astr),'distort',distort)

; make "backward" distortion parameters
astr= hogg_ab2apbp(temporary(astr),[naxis1,naxis2])

return
end
