;+
; PURPOSE:
;   Figure out the solve fraction as a function of stellar number
;   density and theta_AB.
; BUGS:
;   - No proper code header.
; REVISION HISTORY:
;   2006-02-06  started - Hogg
;-
pro solve_statistics

; loop over number density and thetaAB
; dN/dOmega in per arcmin^2
; thetaAB in arcmin
for logdNdOmega= -1.0,0.1,0.25 do for logthetaAB= -0.5,1.6,0.25 do begin
    dNdOmega= 10.0^logdNdOmega
    thetaAB= 10.0^logthetaAB

; convert to quad_statistics units and run
    xx= (2048*0.396/60.0)*sqrt(dNdOmega)
    yy= (1400*0.396/60.0)*sqrt(dNdOmega)
    scalethetaAB= thetaAB*sqrt(dNdOmega)
    quad_statistics, xx,yy,scalethetaAB,seed=seed,ntrial=ntrial, $
      solved=solved
    fraction= float(solved)/float(ntrial)

; output
    splog, dNdOmega,thetaAB,fraction

endfor
return
end
