;+
; NAME:
;   mosaic_crosstalk_analyze
; PURPOSE:
;   Estimate all 64-8 mosaic chip crosstalk terms from files created
;   by mosaic_crosstalk.
; INPUTS:
;   filelist   - list of files made by mosaic_crosstalk
; KEYWORDS:
;   redo       - if set, assume that crosstalk is already computed,
;                and we are looking at the residuals in the crosstalk,
;                to be added to the existing crosstalk calculation for
;                future use.
; REVISION HISTORY:
;   2005-05-05  started - Hogg
;-
pro mosaic_crosstalk_analyze, filelist,redo=redo

; deal with file names (and old crosstalk values if "redo" is set)
prefix= 'mosaic_crosstalk'
if keyword_set(redo) then oldcrosstalk= mrdfits(prefix+'.fits')
if keyword_set(redo) then prefix= 'redo_'+prefix
crosstalkname= prefix+'.fits'
psfilename= prefix+'.ps'
htmlname= prefix+'.html'

for ii=0,n_elements(filelist)-1 do begin
    this= mrdfits(filelist[ii],/silent)
    if keyword_set(redo) then this= oldcrosstalk+this
    if keyword_set(crosstalk) then crosstalk= [[[crosstalk]],[[this]]] $
    else crosstalk= this
endfor
quantiles= [0.16,0.5,0.84]
median= 1
crosstalk_quant= dblarr(8,8,n_elements(quantiles))

; open postscript file
xsize= 7.5 & ysize= 7.5
set_plot, "PS"
device, file=psfilename,/inches,xsize=xsize,ysize=ysize, $
  xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color, bits=8
hogg_plot_defaults, axis_char_scale=axis_char_scale
tiny= 1.d-4
!P.MULTI= [0,8,8]

; open HTML file
openw, wlun,htmlname,/get_lun
printf, wlun,'<html><head><title>'
title= 'KPNO Mosaic chip-chip cross-talk coefficients'
printf, wlun,title
printf, wlun,'</title></head><body>'
printf, wlun,'<h1>'
printf, wlun,title
printf, wlun,'</h1>'
printf, wlun,'<p>To read this table, do the following:'
printf, wlun,'Find the <i>column</i> corresponding to the "target" chip'
printf, wlun,'(the chip you want to read).'
printf, wlun,'Look for significant amplitudes.'
printf, wlun,'For each "cross-talk" chip that has'
printf, wlun,'a significant amplitude in the column,'
printf, wlun,'subtract the amplitude times the cross-talk-chip image'
printf, wlun,'from the target-chip image.</p>'
printf, wlun,'<tt><table>'
printf, wlun,'<tr><th>chip<th>1<th>2<th>3<th>4<th>5<th>6<th>7<th>8

; compute quantiles and plot
for ii=0,7 do begin
    printf, wlun,'<tr><th>'+string(ii+1,format='(I1.1)')
    for jj=0,7 do begin
        if (ii EQ 7) then xcharsize=1.0 else xcharsize=tiny
        if (jj EQ 0) then ycharsize=1.0 else ycharsize=tiny
        crosstalk_quant[ii,jj,*]= $
          weighted_quantile(crosstalk[ii,jj,*],quant=quantiles)
        plot, crosstalk[ii,jj,*],/nodata, $
          xcharsize=xcharsize, $
          yrange=[-0.00499,0.00499],ycharsize=ycharsize
        for kk=0,2 do oplot, [-100,100],[crosstalk_quant[ii,jj,kk], $
                                         crosstalk_quant[ii,jj,kk]], $
          color=djs_icolor('grey')
        oplot, crosstalk[ii,jj,*],psym=1,symsize=0.1,thick=3*!P.THICK
        if (ii eq jj) then begin
            printf, wlun,'<td>'
        endif else begin
            if (crosstalk_quant[ii,jj,median] GT 1D-4) then $
              format='("<td bgcolor=yellow>"' $
            else format='("<td>"'
            if (crosstalk_quant[ii,jj,median] GT 0) then $
              format= format+',"+",F7.5)' $
            else format= format+',F8.5)'
            printf, wlun,crosstalk_quant[ii,jj,median], $
              format=format
        endelse
    endfor
endfor
device,/close

; finish html file
printf, wlun,'</table></tt>'
printf, wlun,'<p>These coefficients were found with'
printf, wlun,'<tt>mosaic_crosstalk</tt> and <tt>mosaic_crosstalk_analyze</tt>'
printf, wlun,'in the <a href="http://astrometry.net/">astrometry.net</a>'
printf, wlun,'codebase.</p>'
printf, wlun,'</body></html>'
close, wlun
free_lun, wlun

; write fits file
; splog, 'writing '+crosstalkname
mwrfits, crosstalk_quant[*,*,median],crosstalkname,/create,/silent
return
end
