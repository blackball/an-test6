;+
; NAME:
;   mosaic_crosstalk_analyze
; PURPOSE:
;   Estimate all 64-8 mosaic chip crosstalk terms
; INPUTS:
;   filename   - name of a raw Mosaic filename to read/test
; REVISION HISTORY:
;   2005-05-05  started - Hogg
;-
pro mosaic_crosstalk_analyze

filelist=file_search('*_crosstalk.fits')
for ii=0,n_elements(filelist)-1 do begin
    this= mrdfits(filelist[ii])
    if keyword_set(crosstalk) then crosstalk= [[[crosstalk]],[[this]]] $
    else crosstalk= this
endfor

psfilename= 'mosaic_crosstalk.ps'
xsize= 7.5 & ysize= 7.5
set_plot, "PS"
device, file=psfilename,/inches,xsize=xsize,ysize=ysize, $
  xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color, bits=8
hogg_plot_defaults, axis_char_scale=axis_char_scale
tiny= 1.d-4
!P.MULTI= [0,8,8]

for ii=0,7 do for jj=0,7 do begin
    if (jj EQ 0) then ycharsize=1.0 else ycharsize=tiny
    quant= weighted_quantile(crosstalk[ii,jj,*],quant=[0.16,0.5,0.84])
    plot, crosstalk[ii,jj,*],psym=1,symsize=0.1,thick=3*!P.THICK, $
      yrange=[-0.003,0.003],ycharsize=ycharsize
    for kk=0,2 do oplot, [-100,100],[quant[kk],quant[kk]], $
      color=djs_icolor('grey')
    oplot, crosstalk[ii,jj,*],psym=1,symsize=0.1,thick=3*!P.THICK
endfor

device,/close
return
end
