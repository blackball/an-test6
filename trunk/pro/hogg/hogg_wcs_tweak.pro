;+
; NAME:
;   hogg_wcs_tweak
; PURPOSE:
;   Take a pretty-good WCS and tweak it up to best possible.
; CALLING SEQUENCE:
;   newastr= hogg_wcs_tweak(astr,xx,yy,siporder=2,jitter=2.0, $
;                           nsigma=5.0,usno=usno,chisq=chisq)
; COMMENTS:
;   - This code linearizes the problem around the tangent point, so it
;     is not *guaranteed* to improve the WCS.
;   - This code does a single iteration, to iterate, just re-run the
;     code (save time by calling with "usno=usno").
; INPUTS:
;   astr     - starting WCS structure (from extast or equivalent)
;   xx,yy    - positions of stars in the image
; OPTIONAL INPUTS:
;   siporder - order for SIP extension to WCS (default 2)
;   jitter   - jitter (uncertainty) to assume (arcsec; default 2.0)
;   nsigma   - number of multiples of jitter to allow (default 5.0)
;   usno     - USNO catalog entries used for fit (speeds operation)
; OUTPUTS:
;            - improved WCS
; OPTIONAL OUTPUTS:
;   usno     - USNO catalog entries used for fit
;   chisq    - chisq
; BUGS:
;   - Not yet written.
;   - Requires USNO in usno_read() format.
;   - Doesn't allow hard specification of which star is which.
;   - Doesn't allow different stars to have different jitters.
; REVISION HISTORY:
;   2005-08-24  started - Hogg (NYU)
;-
function hogg_wcs_tweak, astr,xx,yy,siporder=siporder, $
                         jitter=jitter,nsigma=nsigma,usno=usno,chisq=chisq
if (not keyword_set(siporder)) then siporder= 2
if (not keyword_set(jitter)) then jitter= 2D0
if (not keyword_set(nsigma)) then nsigma= 5D0

; get relevant USNO stars
xy2ad, xx,yy,astr,aa,dd
if (not keyword_set(usno)) then begin
    hogg_mean_ad, aa,dd,meanaa,meandd
    maxangle= max(djs_diff_angle(aa,dd,meanaa,meandd))
    usno= usno_read(meanaa,meandd,1.1*maxangle)
endif
nusno= n_elements(usno)
splog, 'using',nusno,' USNO stars'

; find star matches
spherematch, aa,dd,usno.ra,usno.dec,(jitter*nsigma/3.6D3), $
  match1,match2,distance12,maxmatch=0
xxsub= xx[match1]
yysub= yy[match1]
usnosub= usno[match2]
nmatch= n_elements(match1)
chisq= total((distance12/jitter)^2)
splog, 'BEFORE the fit there are',nmatch,' matches and a chisq of',chisq

; make trivial newastr structure, preserving only the tangent point
newastr= astr
newastr.cd= [[1.0,0.0],[0.0,1.0]]
newastr.cdelt= [1.0,1.0]
newastr.ctype= ['RA---TAN','DEC--TAN']

; make trivial tangent-plane coordinates
ad2xy, usnosub.ra,usnosub.dec,newastr,uu,vv

; set-up for fit of the form uu = AA . pars, vv = AA . pars
ndata= nmatch
AA1= [1D0,xx,yy]
for order= 2,siporder do for jj=0,order do AA1= [AA1,xx^(order-jj)*yy^jj]
nparam= n_elements(AA1)
AA= AA1#replicate(1D0,ndata)
AAtAA= transpose(AA)##AA
AAtAAinv= invert(AAtAA)

; fit

; interpret

; load into structure

; return
return, newastr
end
