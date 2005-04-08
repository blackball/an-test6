;;+
; NAME:
;   mosaic_gsainit
;
; PURPOSE:
;   Initialize GSSS structure
;
; CALLING SEQUENCE:
;   mosaic_gsainit, hdr, scale=scale, cd=cd, racard=racard, deccard=deccard, $
;                   angle=angle, crval=crval
;
; INPUTS:
;   cd      - cd matrix
;   crpix   - crpix (reference pixel)
;   crval   - crval ([ra,dec] of reference)
;
; OUTPUT:
;   function returns gsa = GSSS astrometry structure
;
; REVISION HISTORY:
;   4-Nov-2000  Written by Finkbeiner 
;   2005-04-07  adapted by Hogg (NYU)
;-
;------------------------------------------------------------------------------

function mosaic_gsainit, cd,crval,crpix

  if (NOT keyword_set(racard))  then racard  = 'RADEG'
  if (NOT keyword_set(deccard)) then deccard = 'DECDEG'

; empty gsss_astrometry structure
  gsa = {gsss_astrometry, CTYPE: strarr(2), XLL:0, YLL:0, XSZ:0.D, YSZ:0.D, $
         PPO3:0.0D, PPO6:0.0D, CRVAL: dblarr(2), PLTSCL:0.0D, $
         AMDX:dblarr(13), AMDY:dblarr(13) }
  
  gsa.crval = crval
  gsa.ctype = ['RA---GSS', 'DEC--GSS']

  gsa.xll = 0
  gsa.yll = 0
  gsa.xsz =  1.d
  gsa.ysz =  1.d
  gsa.ppo3 = gsa.xsz*(crpix[0]-0.5)  ; treat orientation coeffs as CRPIX
  gsa.ppo6 = gsa.ysz*(crpix[1]-0.5)

; Map CD matrix onto coefficients, including cubic and quintic terms

  gsa.amdx[0] = cd[0, 0] *3600. ; x  coeff
  gsa.amdx[1] = cd[0, 1] *3600. ; y  

  gsa.amdy[0] = cd[1, 1] *3600. ; y  coeff (not a typo!)
  gsa.amdy[1] = cd[1, 0] *3600. ; x

  gsa.pltscl = 1.d

  return, gsa
end
