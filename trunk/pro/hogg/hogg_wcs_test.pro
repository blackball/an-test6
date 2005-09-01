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
astr= hogg_make_astr(racen,deccen,0.5,0.5,pixscale=1.0/3600.,orientation=30.)
naxis1= astr.naxis1
naxis2= astr.naxis2
astr.ctype= ['RA-','DEC']+'--TAN-SIP'

; make "forward" distortion parameters -- randomly
siporder= 2
distort_flag= 'SIP'
acoeffs= dblarr(siporder+1,siporder+1)
bcoeffs= dblarr(siporder+1,siporder+1)
apcoeffs= acoeffs
bpcoeffs= bcoeffs
for order=2,siporder do for jj=0,order do begin
    norm= (naxis1/100.0)/(naxis1^order)
    acoeffs[order-jj,jj]= norm*randomn(seed)
    bcoeffs[order-jj,jj]= norm*randomn(seed)
endfor
distort = {name:distort_flag,a:acoeffs,b:bcoeffs,ap:apcoeffs,bp:bpcoeffs}
astr= create_struct(temporary(astr),'distort',distort)

; make "backward" distortion parameters
astr= hogg_ab2apbp(temporary(astr),[naxis1,naxis2])

; make FITS header
; image= fltarr(naxis1,naxis2)
; mkhdr, hdr,image
; putast, hdr,astr
; print, hdr

; check orientation
; psfilename= 'hogg_wcs_test.ps'
; xsize= 7.5 & ysize= 7.5
; set_plot, "PS"
; device, file=psfilename,/inches,xsize=xsize,ysize=ysize, $
;   xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color, bits=8
; hogg_plot_defaults
; plot, [0],[0],/nodata, $
;   xrange=[0,naxis1]-0.5,yrange=[0,naxis2]-0.5
; nw_ad_grid, hdr
; device, /close

return
end
