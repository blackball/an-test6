pro bw_est_sky, image, sky, skyerr

;--------------------;
; Estimates the sky level of an image
; either return a scalar or an array
; with the same dimensions as the image
;
; Also estimates the 'skyerror'
;--------------------;
nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]

;-------------------------;
; Try to exclude objects
;
; taken from Blanton's
; scatlight_west
;-------------------------;
;fwhm=5.
;back=median(image)
;noise=sqrt(back)
;nsig=10.
;hmin=nsig*noise
;find, image-back, sx, sy, sflux, sharp, round, hmin, fwhm, [-1., 1.], [0.2, 1.], /silent
;wt=image*0.+1.
;for j=0L, n_elements(sx)-1L do begin
;   xst=(sx[j]-8L)>0
;   xnd=(sx[j]+8L)<(nx-1L)
;   yst=(sy[j]-8L)>0
;   ynd=(sy[j]+8L)<(ny-1L)
;   wt[xst:xnd,yst:ynd]=0.
;endfor
;img_noobj = wt*image
;ii = where (img_noobj eq 0)
;img_noobj[ii] = back

img_noobj = image

skybins = 256 ;in pixels
skystep = skybins
xvec = findgen(nx)/skystep
yvec = findgen(ny)/skystep

;-----------------------------------------------------;
; Start binned sky array inpixel (0,0)
;
; its n_elements+2 because 
; 1 to round up integer number of bins and
; 1 to account for the 1/2 bin sized 'bin' at 
;         the array edge
;-----------------------------------------------------;
binned_sky = FLTARR(2+((nx-skybins/2)/skystep),2+((ny-skybins/2)/skystep))
for j = 0, n_elements(binned_sky[0,*])-1 do begin
    c = j*skystep - skybins/2
    d = c+skybins
    if c lt 0 then c = 0
    if d ge ny then d = ny-1
    for k = 0, n_elements(binned_sky[*,0])-1 do begin
        a = k*skystep - skybins/2
        b = a+skybins
        if a lt 0 then a = 0
        if b ge nx then b = nx-1
        binned_sky[k,j] = MEDIAN(img_noobj[a:b,c:d])
    endfor
endfor
sky = INTERPOLATE(binned_sky, xvec, yvec, /grid)

;surface, binned_sky, AZ = 60
;plot, yvec, sky[0,*], yrange = [MIN(sky[0,*]) - 10, MAX(sky[0,*])+10], xstyle = 1, ystyle = 1

skyerr = sky^0.5

RETURN
END
