;+
; NAME:
;   hogg_mcmc_wcs_tweak
; PURPOSE:
;   Tweak up input WCS using an astrometric catalog by the MCMC
;   method.
; INPUTS:
;   astr      - WCS structure
;   uu,vv     - list of positions of sources in pixel coordinates
;   nstep     - number of links to make in Markov chain
; OUTPUTS:
;   astrchain - chain of WCS structures, best first
;   like      - likelihoods for the chain
; BUGS:
;   - SUPER-SLOW because this is NOT the right problem for MCMC.
;   - Hard-coded to use the USNO-B1.0 catalog.
;   - Positional jitter and sigma limits hard coded.
;   - Soft sigma-clipping (see "like" function) is a hack.
; REVISION HISTORY:
;   2005-08-21  started - Hogg (NYU)
;-
function hogg_mcmc_wcs_tweak_like, astr
common hogg_mcmc_wcs_tweak_block, jitter,nsigma,usno,nu,xx,yy,nx,ch2norm,cdnorm
xy2ad, xx,yy,astr,aa,dd
chisq= 0D0
for ii=0L,nx-1L do begin
    err= djs_diff_angle(usno.ra,usno.dec,aa[ii],dd[ii])*3600.0 ; arcsec
    chisq= chisq+total(err^2/((err/nsigma)^2+jitter^2))
endfor
if (not keyword_set(ch2norm)) then ch2norm=chisq
like= exp(-0.5*(chisq-ch2norm))
return, like
end

function hogg_mcmc_wcs_tweak_step, seed,astr
common hogg_mcmc_wcs_tweak_block, jitter,nsigma,usno,nu,xx,yy,nx,ch2norm,cdnorm
angles_to_xyz, 1D0,astr.crval[0],9D1-astr.crval[1],crx,cry,crz
radianjitter= jitter*!DPI/(180D0*3600D0) ; arcsec to radians
crx= crx+radianjitter*randomn(seed)
cry= cry+radianjitter*randomn(seed)
crz= crz+radianjitter*randomn(seed)
xyz_to_angles, crx,cry,crz,rr,aa,tt
dd= 9D1-temporary(tt)
newastr= hogg_tp_shift(astr,[aa,dd])
if (not keyword_set(cdnorm)) then $
  cdnorm= sqrt(abs(astr.cd[0,0]*astr.cd[1,1]-astr.cd[0,1]*astr.cd[1,0]))
pixeljitter= jitter/(3600D3*cdnorm)
anglejitter= pixeljitter/max(abs([xx-astr.crpix[0],yy-astr.crpix[1]]))
newastr.cd= astr.cd+cdnorm*anglejitter*reform(randomu(seed,4),2,2)
return, newastr
end

pro hogg_mcmc_wcs_tweak, astr,uu,vv,nstep,astrchain,like
common hogg_mcmc_wcs_tweak_block, jitter,nsigma,usno,nu,xx,yy,nx,ch2norm,cdnorm

; read usno catalog
xy2ad, uu,vv,astr,aa,dd
hogg_mean_ad, aa,dd,meanaa,meandd
maxangle= max(djs_diff_angle(aa,dd,meanaa,meandd))
usno= usno_read(meanaa,meandd,1.1*maxangle)
nu= n_elements(usno)
xx= uu
yy= vv
nx= n_elements(xx)

; set MCMC stepping parameters
jitter= 2.0 ; arcsec
nsigma= 3.0

; run the chain
seed= 1L
astrchain= astr
hogg_mcmc, seed,'hogg_mcmc_wcs_tweak_step','hogg_mcmc_wcs_tweak_like', $
  nstep,astrchain,like
return
end
