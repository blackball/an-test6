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
;   - Hard-coded to use the USNO-B1.0 catalog.
;   - Positional jitter and sigma limits hard coded.
;   - Soft sigma-clipping (see "like" function) is a hack.
; REVISION HISTORY:
;   2005-08-21  started - Hogg (NYU)
;-
function hogg_mcmc_wcs_tweak_like, astr
common hogg_mcmc_wcs_tweak_block, jitter,nsigma,usno,nu,xx,yy,nx
xy2ad, xx,yy,astr,aa,dd
chisq= 0D0
for ii=0L,nx-1L do begin
    err= djs_diff_angle(usno.ra,usno.dec,aa[ii],dd[ii])*3600.0 ; arcsec
    chisq= chisq+err^2/((err/nsigma)^2+jitter^2)
endfor
chisq= total((yy-mean)*yy_ivar*(yy-mean),/double)
like= exp(-0.5*(chisq-n_elements(yy)))
return, like
end

function hogg_mcmc_wcs_tweak_step, seed,astr
return, astr
end

pro hogg_mcmc_wcs_tweak, astr,uu,vv,nlink,astrchain,astrlike
common hogg_mcmc_wcs_tweak_block, jitter,nsigma,usno,nu,xx,yy,nx

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
