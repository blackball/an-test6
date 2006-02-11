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
seed= -1L
prefix= 'solve_statistics'
savefile= prefix+'.sav'
if (not file_test(savefile)) then begin

; set up arrays
    nx= 20
    ny= 20
    dx= 0.1
    dy= 0.1
    xbin= -1.5+dx*findgen(nx)
    ybin= -1.0+dy*findgen(ny)
    fraction= fltarr(nx,ny)
    indexsize= fltarr(nx,ny)

; loop over number density and thetaAB
; dN/dOmega in per arcmin^2
; thetaAB in arcmin
    for ii=0L,nx-1L do for jj=0L,ny-1L do begin
        dNdOmega= 10.0^xbin[ii]
        thetaAB= 10.0^ybin[jj]
        indexsize[ii,jj]= dNdOmega^4*thetaAB^6

; convert to quad_statistics units and run
        xx= (2048*0.396/60.0)*sqrt(dNdOmega)
        yy= (1400*0.396/60.0)*sqrt(dNdOmega)
        scalethetaAB= thetaAB*sqrt(dNdOmega)
        ntrial= 1000L
        quad_statistics, xx,yy,scalethetaAB,seed=seed,ntrial=ntrial, $
          solved=solved
        fraction[ii,jj]= float(solved)/float(ntrial)

; output
        splog, dNdOmega,thetaAB,fraction[ii,jj],indexsize[ii,jj]

    endfor
    save, filename= savefile
endif
restore, savefile

set_plot, 'ps'
xsize= 7.5 & ysize= 7.5
device, file=prefix+'.eps',/inches,xsize=xsize,ysize=ysize, $
  xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color, bits=8
hogg_plot_defaults, axis_char_scale=2.0
!X.OMARGIN=!X.OMARGIN+[2,-2]

help, fraction, xbin, ybin

contour, fraction,xbin,ybin, $
  levels= [0.90,0.99,0.999],c_thick=[2,2,5], $
  xrange= minmax(xbin)+0.5*[-dx,dx], $
  xtitle= 'log!d10!n number of stars per square arcmin', $
  yrange= minmax(ybin)+0.5*[-dy,dy], $
  ytitle= 'log!d10!n max theta_AB (arcmin)'
contour, alog10(indexsize),xbin,ybin, $
  levels=[0,1,2],c_thick=[2,2,2], $
  color=djs_icolor('red'), $
  /overplot
  
device,/close

return
end
