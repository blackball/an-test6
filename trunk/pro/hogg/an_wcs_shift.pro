;+
; NAME:
;  an_wcs_shift
; PURPOSE:
;  apply Fink-Hogg Hough-transform algorithm to shift (improve) close
;  WCS
; BUGS:
;-
function an_wcs_shift, astr,xylist,standards,dmax=dmax,binsz=binsz
ad2xy, standards.ra,standards.dec,astr,x2,y2
if (not keyword_set(dmax)) then dmax= 1000.0
if (not keyword_set(binsz)) then binsz= 1.0
offset= offset_from_pairs(xylist.x,xylist.y,x2,y2, $
                          dmax=dmax,binsz=binsz,/verbose)
splog, 'offset:',offset
tmpastr= astr
tmpastr.crpix= astr.crpix-offset
xy2ad, astr.crpix[0],astr.crpix[1],tmpastr,crval1,crval2
newastr= hogg_tp_shift(astr,[crval1,crval2])
return, newastr
end
