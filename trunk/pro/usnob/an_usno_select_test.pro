;+
; NAME:
;   an_usno_select_test
; PURPOSE:
;   QA check the output of AN_USNO_SELECT
;-
pro an_usno_select_test
prefix= 'an_usno_sdss'
splog, 'restoring'
restore, prefix+'.sav'
xsize= 7.5 & ysize= 7.5
psfilename= prefix+'.ps'
set_plot, "PS"
device, file=psfilename,/inches,xsize=xsize,ysize=ysize, $
  xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color, bits=8
hogg_plot_defaults
splog, 'plotting'
hogg_scatterplot, temporary(ra),sin(temporary(dec)*!DPI/1.8D2), $
  xrange=[360.0,0.0],xlabel='RA (deg)', $
  yrange=[-1,1],ylabel='sin(Dec)'
device,/close
splog, 'done'
return
end
