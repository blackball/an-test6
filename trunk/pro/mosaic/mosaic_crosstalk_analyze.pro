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
quantiles= [0.16,0.5,0.84]
crosstalk_quant= dblarr(8,8,n_elements(quantiles))

psfilename= 'mosaic_crosstalk.ps'
xsize= 7.5 & ysize= 7.5
set_plot, "PS"
device, file=psfilename,/inches,xsize=xsize,ysize=ysize, $
  xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0,/color, bits=8
hogg_plot_defaults, axis_char_scale=axis_char_scale
tiny= 1.d-4
!P.MULTI= [0,8,8]

for ii=0,7 do for jj=0,7 do begin
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
endfor
device,/close

crosstalkname= 'mosaic_crosstalk.fits'
splog, 'writing '+crosstalkname
mwrfits, crosstalk_quant[*,*,1],crosstalkname,/create
return
end
