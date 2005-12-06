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
;   siporder - order for SIP extension to WCS (default 1; ie, no SIP)
;   jitter   - jitter (uncertainty) to assume (arcsec; default 2.0)
;   nsigma   - number of multiples of jitter to allow (default 5.0)
;   usno     - USNO catalog entries used for fit (speeds operation)
; KEYWORDS:
; OUTPUTS:
;            - improved WCS
; OPTIONAL OUTPUTS:
;   usno     - USNO catalog entries used for fit
;   chisq    - chisq
; BUGS:
;   - Can only handle (as input or output) SIP distortions.
;   - If you *believe* the input distortions (eg, with ACS images) but
;     don't believe the pointing and rotation, you will want to
;     preserve distortion information; this is not currently possible.
;   - Hard-codes duplicate of "extast.pro" code to make SIP structure.
;   - Requires USNO in usno_read() format.
;   - Doesn't allow hard specification of which star is which.
;   - Doesn't allow different stars to have different jitters.
;   - Doesn't use any information other than position to match stars.
; REVISION HISTORY:
;   2005-09-05  tested on SDSS fields and works - Hogg (NYU)
;-
function hogg_wcs_tweak, astr,xx,yy,siporder=siporder, $
                         jitter=jitter,nsigma=nsigma,usno=usno, $
                         chisq=chisq
if (not keyword_set(siporder)) then siporder= 1
if (not keyword_set(jitter)) then jitter= 2D0
if (not keyword_set(nsigma)) then nsigma= 5D0

; get relevant catalog stars
xy2ad, xx,yy,astr,aa,dd
if (not keyword_set(usno)) then begin
    hogg_mean_ad, aa,dd,meanaa,meandd
    maxangle= max(djs_diff_angle(aa,dd,meanaa,meandd))
    usno= usno_read(meanaa,meandd,1.1*maxangle)
endif
nusno= n_elements(usno)
splog, 'using',nusno,' catalog stars'

; find star matches
spherematch, aa,dd,usno.ra,usno.dec,(jitter*nsigma/3.6D3), $
  match1,match2,distance12,maxmatch=0
nmatch= n_elements(match1)
chisq= total(((distance12*3.6d3)/jitter)^2)
splog, 'BEFORE the fit there are',nmatch,' matches and a chisq of',chisq

; restrict vectors to matched stars
xxsub= xx[match1]-(astr.crpix[0]-1.0)
yysub= yy[match1]-(astr.crpix[1]-1.0)
usnosub= usno[match2]

; make trivial newastr structure, preserving only the tangent point
trivastr= struct_trimtags(astr,except_tags='DISTORT')
trivastr.crpix= [0,0]
trivastr.cd= [[1.0,0.0],[0.0,1.0]]
trivastr.cdelt= [1.0,1.0]
trivastr.ctype= ['RA---TAN','DEC--TAN']

; make trivial tangent-plane coordinates
ad2xy, usnosub.ra,usnosub.dec,trivastr,uu,vv

; linear fit (of the form uu = AA . upars, vv = AA . vpars)
AA= transpose([[replicate(1D0,nmatch)],[xxsub],[yysub]])
for order= 2,siporder do for jj=0,order do $
  AA= [AA,transpose(xxsub^(order-jj)*yysub^jj)]
AAtAA= transpose(AA)##AA
AAtAAinv= invert(AAtAA)
upars= transpose(AAtAAinv##(transpose(AA)##uu))
vpars= transpose(AAtAAinv##(transpose(AA)##vv))

; interpret and load into structure
xy2ad, upars[0],vpars[0],trivastr,crval0,crval1 ; NB: using trivial newastr
newastr= astr
newastr= hogg_tp_shift(newastr,[crval0,crval1])
newastr.cd= [[upars[1],vpars[1]],[upars[2],vpars[2]]]

; deal with SIP structure
if (siporder GT 1) then begin
    distort_flag= 'SIP'
    acoeffs= dblarr(siporder+1,siporder+1)
    bcoeffs= dblarr(siporder+1,siporder+1)
    ap= dblarr(siporder+1,siporder+1)
    bp= dblarr(siporder+1,siporder+1)

; get a and b coeffs by applying cdinv to the fit output
    cdinv= invert([[upars[1],vpars[1]],[upars[2],vpars[2]]])
    kk= 3
    for order= 2,siporder do for jj=0,order do begin
        abvec= cdinv#[upars[kk],vpars[kk]]
        acoeffs[order-jj,jj]= abvec[0]
        bcoeffs[order-jj,jj]= abvec[1]
        kk= kk+1
    endfor

; store in structure (code copied from extast.pro)
    distort = {name:distort_flag,a:acoeffs,b:bcoeffs,ap:ap,bp:bp}
    if (where(tag_names(newastr) EQ 'DISTORT') EQ -1) then begin
        splog, 'adding DISTORT parameters to newastr'
        newastr= create_struct(temporary(newastr),'distort',distort)
    endif else begin
        newastr.distort= distort
    endelse

; get ap coeffs by inverting the polynomial
    newastr= hogg_ab2apbp(newastr,[max(xx),max(yy)])
endif

; return
return, newastr
end
