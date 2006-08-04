;+
; NAME:
;   dpsfcheck
; PURPOSE:
;   check how well a PSF fits some data
; CALLING SEQUENCE:
;   dpsfcheck, image, ivar, x, y [, psf= ]
; INPUTS:
;   image - [nx, ny] input image
;   ivar - [nx, ny] input invverse variance
;   x, y - [N] positions to check
; OPTIONAL INPUTS:
;   psf - sigma of PSF
; REVISION HISTORY:
;   1-Mar-2006  Written by Blanton, NYU
;-
;------------------------------------------------------------------------------
function dpsfcheck, image, ivar, x, y, amp=amp, psf=psf

nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]
npx=(size(psf,/dim))[0]
npy=(size(psf,/dim))[1]
cc=fltarr(npx,npy)+1.
xx=reform(replicate(1., npx)#findgen(npy), npx*npy)/float(npx)-0.5
yy=reform(findgen(npx)#replicate(1., npy), npx*npy)/float(npy)-0.5
rr=sqrt((xx-npx*0.5)^2+(yy-npy*0.5)^2)

cmodel=fltarr(npx*npy, 4)
cmodel[*,0]=reform(psf/max(psf), npx*npy)
cmodel[*,1]=xx
cmodel[*,2]=yy
cmodel[*,3]=cc

ispsf=fltarr(n_elements(x))+1
amp=fltarr(n_elements(x))
for i=0L, n_elements(x)-1L do begin
    cutout_image=fltarr(npx,npy)
    cutout_ivar=fltarr(npx,npy)
    embed_stamp, cutout_image, image, npx/2L-x[i], npy/2L-y[i]
    embed_stamp, cutout_ivar, ivar, npx/2L-x[i], npy/2L-y[i]
    cutout_ivar=cutout_ivar>0.

    hogg_iter_linfit, cmodel, reform(cutout_image, npx*npy), $
      reform(cutout_ivar, npx*npy), coeffs, nsigma=20
    
    model=reform(coeffs#cmodel, npx, npy)
    resid=(cutout_image-model)^2*cutout_ivar*(psf>0.1)
    igd=where(cutout_ivar gt 0, ngd)
    chi2sig=(total(resid[igd])-ngd)/sqrt(ngd)
    help,i,chi2sig
    amp[i]=coeffs[0]
    ispsf[i]=chi2sig lt 100. AND coeffs[3] lt 0.2*coeffs[0]
    help,ispsf[i]
    atv, cutout_image
    stop
endfor

return, ispsf

end
;------------------------------------------------------------------------------
