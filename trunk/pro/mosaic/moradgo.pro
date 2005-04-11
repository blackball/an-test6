pro moradgo
path='/global/data/scr/morad/4meter'
filelist=file_search(path+'/2005-04-0*/obj*')
nfile=n_elements(filelist)
print,nfile
redux='/global/data/scr/mm1330/4meter/redux/'
avzero=redux+'zero_av168to177.fits'
avdark=redux+'dark_av121to123-158to160.fits'
gflat=redux+'flat_g_091to095.fits'
rflat=redux+'flat_r_096to100.fits'
iflat=redux+'flat_i_101to105.fits'

for i=0,nfile-1 do begin

hdr=headfits(filelist[i])
band=sxpar(hdr,'FILTER')
if band eq 'g SDSS k1017      ' then $
mosaic_flatten,filelist[i],avzero,avdark,gflat,path+'/flatten/flatten_'+strmid(filelist[i],41,11)
if band eq 'r SDSS k1018      ' then $
mosaic_flatten,filelist[i],avzero,avdark,rflat,path+'/flatten/flatten_'+strmid(filelist[i],41,11)
if band eq 'i SDSS k1019      ' then $
mosaic_flatten,filelist[i],avzero,avdark,iflat,path+'/flatten/flatten_'+strmid(filelist[i],41,11)

endfor
return
end

