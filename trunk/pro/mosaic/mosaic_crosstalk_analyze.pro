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

for ii=0,7 do for jj=0,7 do $
  plot, crosstalk[ii,jj,*],psym=6, $
  yrange=[-0.01,0.01]

device,/close

stop
return
end
