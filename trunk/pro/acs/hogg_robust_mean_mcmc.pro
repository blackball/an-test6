;+
; BUGS:
;   - No good comment header.
;   - The pars structure is a recipe for structure conflict disasters.
;-

;;;; function to take a random step in parameter space
function hogg_robust_mean_step, seed,pars
common hogg_robust_mean_block, x,x_ivar,nx,minx,maxx
newpars= pars
newpars.mean= pars.mean+randomn(seed)/sqrt(median(x_ivar))
newpars.p= ((pars.p+0.1*randomn(seed)) > 0.) < 1.
change= where(randomu(seed,nx) LT (1.0/nx),nchange)
if (nchange GT 0) then newpars.use[change]= 1-pars.use[change]
return, newpars
end

;;;; function to compute likelihood for a given mean, p, and bits
function hogg_robust_mean_like, pars
common hogg_robust_mean_block, x,x_ivar,nx,minx,maxx
if ((pars.mean GT minx) and (pars.mean LT maxx)) then prior= 1.0/(maxx-minx) $
else prior= 0.0
return, product(pars.p*pars.use[0:nx-1] $
                *exp(-0.5*x_ivar*(x-pars.mean)^2)*sqrt(x_ivar/(2*!DPI)) $
                +(1.0-pars.p)*(1.0-pars.use[0:nx-1])*prior)
end

function hogg_robust_mean_mcmc, inx,inx_ivar,seed=seed,nstep=nstep, $
                                pars=pars,qa=qa,hot=hot
common hogg_robust_mean_block, x,x_ivar,nx,minx,maxx
x= inx
x_ivar= inx_ivar
nx= n_elements(x)
minx= min(x)
maxx= max(x)
pars= {hogg_robust_mean_struct, $
       mean  : 0D, $
       p     : 1.0D, $
       use   : bytarr(nx)+1 }
pars.p= 0.5                     ; start at 50/50 rejection
pars.mean= total(x*pars.use*x_ivar)/total(pars.use*x_ivar)
if (not keyword_set(nstep)) then nstep= 100
hogg_mcmc, seed,'hogg_robust_mean_step','hogg_robust_mean_like',nstep,pars,like
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
    !Y.OMARGIN= [0,0]
    !P.TITLE= 'MCMC trials'
    hogg_plothist, pars.mean,xtitle='mean'
    oplot, [1,1]*pars[0].mean,[!Y.CRANGE[0],!Y.CRANGE[1]],linestyle=1
    hogg_plothist, pars.p,xtitle='p'
    oplot, [1,1]*pars[0].p,[!Y.CRANGE[0],!Y.CRANGE[1]],linestyle=1
    hogg_scatterplot, pars.mean,pars.p,weight=like, $
      xtitle= 'mean', $
      ytitle= 'p'
    meanuse= total(reform(pars.use,nx,nstep),2)/nstep
    plot, x,meanuse,psym=6,thick=2*!P.THICK, $
      xtitle= 'x', $
      yrange=[-0.1,1.1],ytitle= 'fraction'
    djs_oploterr, x,meanuse,xerr=1.0/sqrt(x_ivar)
    device, /close

endif
return, pars[0].mean
end
