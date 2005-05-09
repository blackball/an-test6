pro go

path='/global/data/scr/morad/4meter/'

; estimate cross-talk:
;filelist=file_search(path+'/2005-04-??/obj*.fits*')
;for ii=0,n_elements(filelist)-1 do mosaic_crosstalk, filelist[ii]
mosaic_crosstalk_analyze

; make the averaged zero:
filelist=file_search(path+'/2005-04-08/zero*')
avzero='zero_av168to177.fits'
mosaic_average_zero,filelist,avzero

; make the averaged dark
filelist=file_search(path+'/2005-04-07/dar12*)
filelist=[filelist,path+'/2005-04-07/dar158.fits',path+'/2005-04-07/dar159.fits',path+'/2005-04-07/dar160.fits']
avdark='dark_av121to123-158to160.fits'
mosaic_average_dark,filelist,avzero,avdark

; make the averaged flats
; g-band
filelist=[path+'/2005-04-07/dflat091.fits',path+'/2005-04-07/dflat092.fits',path+'/2005-04-07/dflat093.fits',path+'/2005-04-07/dflat094.fits',path+'/2005-04-07/dflat095.fits']
gflat='flat_g_091to095.fits'
mosaic_average_flat,filelist,avzero,avdark,gflat

; r-band
filelist=[path+'/2005-04-07/dflat096.fits',path+'/2005-04-07/dflat097.fits',path+'/2005-04-07/dflat098.fits',path+'/2005-04-07/dflat099.fits',path+'/2005-04-07/dflat100.fits']
rflat='flat_r_096to100.fits'
mosaic_average_flat,filelist,avzero,avdark,rflat

; i-band
filelist=[path+'/2005-04-07/dflat101.fits',path+'/2005-04-07/dflat102.fits',path+'/2005-04-07/dflat103.fits',path+'/2005-04-07/dflat104.fits',path+'/2005-04-07/dflat105.fits']
iflat='flat_i_101to105.fits'
mosaic_average_flat,filelist,avzero,avdark,iflat

; making the flattened willman 1
; g-band
filelist=['obj125.fits']
for i=126,130 do filelist=[filelist,'obj'+string(i,format='(I3.1)')+'.fits']
for i=138,142 do filelist=[filelist,'obj'+string(i,format='(I3.1)')+'.fits']
for i=0,11 do mosaic_flatten,path+'2005-04-07/'+filelist[i],avzero,avdark,gflat,path+'redux/Willman1/flatten_'+filelist[i]

; r-band
filelist=['obj132.fits']
for i=133,136 do filelist=[filelist,'obj'+string(i,format='(I3.1)')+'.fits']
for i=0,4 do mosaic_flatten,path+'2005-04-07/'+filelist[i],avzero,avdark,rflat,path+'redux/Willman1/flatten_'+filelist[i]
 
filelist=['obj179.fits']
for i=180,183 do filelist=[filelist,'obj'+string(i,format='(I3.1)')+'.fits']
for i=0,4 do mosaic_flatten,path+'2005-04-08/'+filelist[i],avzero,avdark,rflat,path+'redux/Willman1/flatten_'+filelist[i]

; i-band
filelist=['obj226.fits']
for i=227,232 do filelist=[filelist,'obj'+string(i,format='(I3.1)')+'.fits']
for i=0,6 do mosaic_flatten,path+'2005-04-09/'+filelist[i],avzero,avdark,iflat,path+'redux/Willman1/flatten_'+filelist[i]

; make bitmask
 bitmaskname='mosaic_bitmask.fits' 
 mosaic_bitmask,avzero,avdark,gflat,bitmaskname

; measure / fix / install astrometric headers (GSSS!)
dowcs, '/global/data/scr/morad/4meter/flatten', $
       '/global/data/scr/morad/4meter/newaf'
spawn, '\rm -rfv /global/data/scr/morad/4meter/oldaf'
spawn, '\mv -fv /global/data/scr/morad/4meter/af /global/data/scr/morad/4meter/oldaf'
spawn, '\mv -fv /global/data/scr/morad/4meter/newaf /global/data/scr/morad/4meter/af'

; calibrate images by comparison with SDSS
; [THIS NEEDS TO BE DONE]

; make mosaics
; makemosaics

return
end
