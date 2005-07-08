;+
; NAME:
;   hogg_acs_distort
; PURPOSE:
;   Apply distortions as read from an ACS image header to input x,y
;   positions and return output x,y positions that are on a square,
;   tangent-plane coordinate system.
; INPUTS:
;   hdr        - ACS image header as read by headfits() or equiv
;   inx,iny    - input x,y position vectors
; OUTPUTS:
;   outx,outy  - output x,y positions, after distortions have been
;                applied.
; COMMENTS:
;   - This does *not* compute RA, Dec; it just distorts to a square
;     tangent plane.
;   - The input x,y and output x,y should be zero-indexed, so the
;     center of the first pixel in the array should be (0,0).
;   - DO NOT apply this to an already-drizzled or already-distortion-
;     corrected frame!
; BUGS:
;   - Doesn't read or return header information in any useful or
;     reusable way, like it should.
;   - Relies on sxpar() returning zero when hdr doesn't contain the
;     asked-for coefficient.
;   - Ought to not ever take powers; it should multiply in xd and yd
;     with each iteration.
; REVISION HISTORY:
;   2005-??-??  original script written - Burles (MIT)
;   2005-05-24  made a procedure - Hogg (NYU)
;   2005-07-07  huge bugfix found when I found Shupe et al - Hogg
;-
pro hogg_acs_distort, hdr,inx,iny,outx,outy
xd= inx-(sxpar(hdr,'CRPIX1')-1)
yd= iny-(sxpar(hdr,'CRPIX2')-1)
aorder= sxpar(hdr,'A_ORDER')
border= sxpar(hdr,'B_ORDER')
maxorder= aorder>border
outx= inx
outy= iny
for pp=0,maxorder do begin
    for qq=0,(maxorder-pp) do begin
        a= sxpar(hdr,string('A_',pp,'_',qq,format='(a,i1,a,i1)'))
        outx= outx+a*xd^pp*yd^qq
        b= sxpar(hdr,string('B_',pp,'_',qq,format='(a,i1,a,i1)'))
        outy= outy+b*xd^pp*yd^qq
    endfor
endfor
return
end
