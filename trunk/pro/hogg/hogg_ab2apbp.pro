;+
; NAME:
;   hogg_ab2apbp
; PURPOSE:
;   Find best "reverse" polynomial terms given "forward" polynomial
;   terms for a TAN-SIP WCS header.
; INPUTS:
;   astr   - input WCS structure of SIP form with distort.a and
;            distort.b set
;   naxis  - [2] vector with image dimensions NAXIS1,NAXIS2
; OUTPUTS:
;          - WCS structure but with distort.ap and distort.bp set
; COMMENTS:
;   - This works by putting down a grid of points and least-square
;     fitting.  Not sophisticated!
;   - The ap matrix must be present in astr.distort and of the correct
;     size, because this code just reads the order off of it.
; BUGS:
;   - Various things hard-coded.
;   - Assumes APORDER = BPORDER.
; REVISION HISTORY:
;   2005-08-31  started - Hogg (NYU)
;-
function hogg_ab2apbp, astr,naxis
newastr= astr
newastr.distort.ap[*,*]= 0.0
newastr.distort.bp[*,*]= 0.0
siporder= (size(newastr.distort.ap,/dimens))[0]-1
splog, 'assuming APORDER = BPORDER =',siporder
ng= 2*(siporder+1)
xx= reform((dindgen(ng)*naxis[0]/(ng-1))#replicate(1D0,ng),ng*ng)
yy= reform(replicate(1D0,ng)#(dindgen(ng)*naxis[1]/(ng-1)),ng*ng)
xy2ad, xx,yy,newastr,aa,dd
ad2xy, aa,dd,newastr,xxback,yyback
splog, 'mean square error (pix) before AP, BP set:', $
  (total((xxback-xx)^2)+total((yyback-yy)^2))/n_elements(xx)
amatrix= dblarr((siporder+1)*(siporder+2)/2-3,ng*ng)
kk= 0
xdif= xxback-(newastr.crval[0]-1)
ydif= yyback-(newastr.crval[0]-1)
for order=2,siporder do for jj=0,order do begin
    amatrix[kk,*]= xdif^(order-jj)*ydif^jj
    kk= kk+1
endfor
atainv= invert(transpose(amatrix)##amatrix)
xxpars= atainv##(transpose(amatrix)##(xx-xxback))
yypars= atainv##(transpose(amatrix)##(yy-yyback))
kk= 0
for order=2,siporder do for jj=0,order do begin
    newastr.distort.ap[order-jj,jj]= xxpars[kk]
    newastr.distort.bp[order-jj,jj]= yypars[kk]
    kk= kk+1
endfor
ad2xy, aa,dd,newastr,xxback,yyback
splog, 'mean square error (pix) after AP, BP set:', $
  (total((xxback-xx)^2)+total((yyback-yy)^2))/n_elements(xx)
return, newastr
end
