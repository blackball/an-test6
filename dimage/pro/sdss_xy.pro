pro sdss_xy, run, camcol, field

image=uint(mrdfits(getenv('PHOTO_DATA')+'/'+strtrim(string(run),2)+ $
                   '/fields/'+strtrim(string(camcol),2)+'/idR-'+ $
                   string(f='(i6.6)', run)+'-r'+strtrim(string(camcol),2)+'-'+$
                   string(f='(i4.4)', field)+'.fit.Z'))

astromxy, image, x, y, flux


end
