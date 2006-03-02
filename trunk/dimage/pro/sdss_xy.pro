pro sdss_xy, run, camcol, field

image=uint(mrdfits(getenv('PHOTO_DATA')+'/'+strtrim(string(run),2)+ $
                   '/fields/'+strtrim(string(camcol),2)+'/idR-'+ $
                   string(f='(i6.6)', run)+'-u'+strtrim(string(camcol),2)+'-'+$
                   string(f='(i4.4)', field)+'.fit.Z'))

simplexy, image, x, y, flux

outstr=replicate({x:0., y:0., flux:0.}, n_elements(x))
outstr.x=x
outstr.y=y
outstr.flux=flux
mwrfits, outstr, 'sdss-xy-'+string(f='(i6.6)', run)+'-u'+ $
  strtrim(string(camcol),2)+'-'+string(f='(i4.4)', field)+'.fits', /create


end
