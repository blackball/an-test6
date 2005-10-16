;+
; NAME:
;   an_usno_select_test
; PURPOSE:
;   QA check the output of AN_USNO_SELECT
;-
pro an_usno_select_test
racen= 277.0                    ; deg
sdeccen= -0.07                  ; dimensionless
dra= 10.0                       ; deg
xrange= racen+[1,-1]*dra/2.0    ; scaled
dsd= dra/180.0
yrange= sdeccen+[-1,1]*dsd/2.0
prefix= 'an_usno_sdss'
splog, 'restoring'
restore, prefix+'.sav'
good= 0
mag= 0
sindx= 0
usno= 0
usnomag= 0
ind= 0
nra= n_elements(ra)
indx= shuffle_indx(nra,num_sub=10000000L)
ra= (temporary(ra))[indx]
sdec= sin((temporary(dec))[indx]*!DPI/1.8D2)
xsize= 7.5 & ysize= xsize
psfilename= prefix+'.ps'
set_plot, "PS"
device, file=psfilename,/inches,xsize=xsize,ysize=ysize, $
  xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color, bits=8
hogg_plot_defaults
splog, 'plotting'
xnpix= 180
ynpix= xnpix
hogg_scatterplot, temporary(ra),sdec, $
  xnpix=xnpix,xrange=xrange,xtitle='RA (deg)', $
  ynpix=ynpix,yrange=yrange,ytitle='sin(Dec)', $
  levels=1D0-double([1.0,0.1])/double(dra)/double(dsd), $
  /internal_weight
device,/close
splog, 'done'
return
end
