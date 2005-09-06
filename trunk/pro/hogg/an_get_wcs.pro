;+
; NAME:
;   an_get_wcs.pro
; PURPOSE:
;   Refine quad-indexer output to proper, precise WCS for the
;   astrometry.net project.
; CALLING SEQUENCE:
;   astr= an_get_wcs(uu,vv,xx,yy,zz,su,sv,siporder=2)
; INPUT:
;   coords   - [5,2] array of U,V (image coords) and x,y,z (unit-vector
;              on the sphere coords) of 2 points.
;   uu,vv    - catalog including positions of compact sources in image coords
; OPTIONAL INPUT:
;   parity   - 0 (default) if parity is the usual astronomical parity,
;              1 if not
;   siporder - if >1, make TAN-SIP WCS to this order, otherwise
;              vanilla TAN
; OUTPUT:
;   astr     - astrometry WCS structure
; BUGS:
;   - Code not yet written!
; REVISION HISTORY:
;   2005-09-05  started - Hogg (NYU)
;-
function an_get_wcs, coords,uu,vv,parity=parity,siporder=siporder
if (not keyword_set(parity)) then parity=0
if (not keyword_set(siporder)) then siporder=1
startastr= hogg_points2astr(coords,parity)
nsigma= 3.0
astr= hogg_wcs_tweak(startastr,uu,vv,siporder=1,jitter=2.0, $
                     nsigma=nsigma,usno=usno,chisq=chisq)
astr= hogg_wcs_tweak(astr,uu,vv,siporder=1,jitter=2.0, $
                     nsigma=nsigma,usno=usno)
astr= hogg_wcs_tweak(astr,uu,vv,siporder=siporder,jitter=2.0, $
                     nsigma=nsigma,usno=usno)
astr= hogg_wcs_tweak(astr,uu,vv,siporder=siporder,jitter=2.0, $
                     nsigma=nsigma,usno=usno)
astr= hogg_wcs_tweak(astr,uu,vv,siporder=siporder,jitter=2.0, $
                     nsigma=nsigma,usno=usno)
astr= hogg_wcs_tweak(astr,uu,vv,siporder=siporder,jitter=2.0, $
                     nsigma=nsigma,usno=usno)
hogg_plot_defaults
plot, uu,vv,psym=1
ad2xy, usno.ra,usno.dec,astr,xx,yy
oplot, xx,yy,psym=6
return, astr
end
