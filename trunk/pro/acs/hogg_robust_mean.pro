;+
; NAME:
;   hogg_robust_mean
; PURPOSE:
;   Determine the weighted mean of a set of measurements, considering
;   all possible rejection scenarios and all possible penalties for
;   rejection.
; INPUTS:
;   x      - N measurements to average
;   x_ivar - N inverse variances on the x measurements (assumed gaussian)
; OPTIONAL INPUTS:
;   qa    - name of PostScript file for QA output; default to no QA output
; KEYWORDS:
; OUTPUT:
;   mean      - Maximum likelihood mean
; OPTIONAL OUTPUTS
;   mean_ivar - approximate inverse variance on that
; COMMENTS:
;   - Assumes gaussian PDF for each point.
;   - Assumes that each point is equally likely of "telling a lie".
;   - Integrates over all possible rejection penalties and rejection
;     scenarios!
; BUGS:
;   - Ought to output the full PDF for the mean, not just the peak and
;     second moment.
; REVISION HISTORY:
;   2005-06-02  started - Hogg (NYU)
;-
function hogg_robust_mean, x,x_ivar,mean_ivar=mean_ivar,qa=qa
nx= n_elements(x)
xrange= [min(x-5.0/sqrt(x_ivar)),max(x+5.0/sqrt(x_ivar))]
nxvec= 1000
xvec= xrange[0]+(dindgen(nxvec)-0.5)*(xrange[1]-xrange[0])/nxvec
np= 100
pvec= (dindgen(np)+0.5)/np
if keyword_set(qa) then begin
    set_plot, 'ps'
    xsize= 7.5 & ysize= 7.5
    device, file=qa,/inches,xsize=xsize,ysize=ysize, $
      xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color,bits=8
    hogg_plot_defaults
    !P.MULTI= [0,2,2]
    !X.MARGIN= !X.OMARGIN
    !X.OMARGIN= [0,0]
    !Y.MARGIN= !Y.OMARGIN
    !y.OMARGIN= [0,0]
endif
x_pdf= dblarr(nxvec)
p_pdf= dblarr(np)
u_pdf= dblarr(nx)
for trial=2L^nx-1L,0L,-1L do begin
    use= ((trial AND 2L^lindgen(nx)) < 1) ; measurements to use
    !P.TITLE= 'trial '+strtrim(string(trial),2)+'; ' $
      +strjoin(string(use,format='(I1.1)'),' ')
    thispdf= dblarr(nx,nxvec)
    for ii=0L,nx-1L do thispdf[ii,*]= $
      use[ii]*exp(-0.5*x_ivar[ii]*(xvec-x[ii])^2) $
      +(1.0-use[ii])*replicate(1,nxvec)
    thispdf= thispdf/(total(thispdf,2)#replicate(1,nxvec))
    if keyword_set(qa) then begin
        plot, xvec,thispdf[0,*],psym=10, $
          xtitle= 'x', $
          yrange= [-0.1,1.1]*max(thispdf)
        for ii=1L,nx-1L do oplot, xvec,thispdf[ii,*],psym=10
    endif
    trial_x_pdf= dblarr(nxvec)
    trial_p_pdf= dblarr(np)
    for jj=0L,np-1 do begin
        thatpdf= pvec[jj]*(use#replicate(1,nxvec))*thispdf $
          +(1.0-pvec[jj])*((1-use)#replicate(1,nxvec))*thispdf
        trial_x_pdf= trial_x_pdf+product(thatpdf,1)
        trial_p_pdf[jj]= total(product(thatpdf,1))
    endfor
    if keyword_set(qa) then $
      plot, xvec,trial_x_pdf,psym=10, $
      xtitle= 'x', $
      yrange=[-0.1,1.1]*max(trial_x_pdf)
    if keyword_set(qa) then $
      plot, pvec,trial_p_pdf,psym=10, $
      xtitle= 'p', $
      yrange=[-0.1,1.1]*max(trial_p_pdf)
    x_pdf= x_pdf+trial_x_pdf
    p_pdf= p_pdf+trial_p_pdf
    u_pdf= u_pdf+use*total(trial_x_pdf)
    if keyword_set(qa) then $ 
      plot, xvec,x_pdf,psym=10, $
      xtitle= 'x', $
      yrange=[-0.1,1.1]*max(x_pdf)
endfor
if keyword_set(qa) then begin
    !P.TITLE= 'final result'
    plot, xvec,x_pdf,psym=10, $
      xtitle= 'x', $
      yrange=[-0.1,1.1]*max(x_pdf)
    plot, pvec,p_pdf,psym=10, $
      xtitle= 'p', $
      yrange=[-0.1,1.1]*max(p_pdf)
    plot, x,u_pdf,psym=6,thick=2*!P.THICK, $
      xrange=minmax(xvec),xtitle= 'x', $
      yrange=[-0.1,1.1]*max(u_pdf)
    djs_oploterr, x,u_pdf,xerr=1.0/sqrt(x_ivar)
    device, /close
endif
return, 0
end
