;-
; INPUTS:
;   xx,yy    - dimensions of rectangular field in units of the mean
;              inter-star separation on the sky
;   thetaAB  - max theta_AB for quads being in index
; BUGS:
;   - No proper code header.
; REVISION HISTORY:
;   2006-02-05  started - Hogg
;-
pro quad_statistics, xx,yy,thetaAB,seed=seed,ntrial=ntrial
if (not keyword_set(seed)) then seed= -1L
if (not keyword_set(ntrial)) then ntrial= 100L
tiny= 1D-5
nquadthresh= 3

; loop over trials
solved= 0L
for tt=0L,ntrial-1L do begin
    nquad= 0L

; get poisson stars
    mean= xx*yy             ; they are in units of the mean separation
;    splog, trial,'mean',mean
    nstar= (poidev(mean,seed=seed))[0]
;    splog, trial,'nstar',nstar
    xxx= xx*randomu(seed,nstar)
    yyy= yy*randomu(seed,nstar)

; loop over A,B pairs
    for aa=0L,nstar-2L do for bb=aa+1L,nstar-1L do begin
        dx= xxx[bb]-xxx[aa]
        dy= yyy[bb]-yyy[aa]
        norm2= dx^2+dy^2
;        splog, trial,aa,bb,'norm2',norm2
        if (norm2 LT thetaAB^2) then begin

; make rotation matrix and apply it
            mat1= (dy+dx)/norm2
            mat2= (dy-dx)/norm2
            uu=  mat1*(xxx-xxx[aa])+mat2*(yyy-yyy[aa])
            vv= -mat2*(xxx-xxx[aa])+mat1*(yyy-yyy[aa])

; count C,D pairs
            foo= where((uu GT tiny) AND $
                       (uu LT (1D0-tiny)) AND $
                       (vv GT tiny) AND $
                       (vv LT (1D0-tiny)),nfoo)
;            splog, trial,aa,bb,'nfoo',nfoo
            if (nfoo GE 2) then begin
                nquad1= nfoo*(nfoo-1L)/2L
                nquad= nquad+nquad1
            endif

        endif
    endfor
    splog, trial,'nquad',nquad
    if (nquad GE nquadthresh) then solved= solved+1L
endfor
splog, 'solve fraction:',float(solved)/float(ntrial)
return
end
