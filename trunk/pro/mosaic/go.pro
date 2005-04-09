pro go

path='/Volumes/LaCie Disk/data/4meter/'
;making the averaged zero:

filelist=file_search(path+'/2005-04-08/zero*')
avzero='zero_av168to177.fits'
mosaic_average_zero,filelist,avzero


;making the averaged dark

filelist=file_search(path+'/2005-04-07/dar12*)
filelist=[filelist,path+'/2005-04-07/dar158.fits',path+'/2005-04-07/dar159.fits',path+'/2005-04-07/dar160.fits']

avdark='dark_av121to123-158to160.fits'
mosaic_average_dark,filelist,avzero,avdark

;making the averaged flats
;g-band

filelist=[path+'/2005-04-07/dflat091.fits',path+'/2005-04-07/dflat092.fits',path+'/2005-04-07/dflat093.fits',path+'/2005-04-07/dflat094.fits',path+'/2005-04-07/dflat095.fits']

gflat='flat_g_091to095.fits'
mosaic_average_flat,filelist,avzero,avdark,gflat

;r-band

filelist=[path+'/2005-04-07/dflat096.fits',path+'/2005-04-07/dflat097.fits',path+'/2005-04-07/dflat098.fits',path+'/2005-04-07/dflat099.fits',path+'/2005-04-07/dflat100.fits']

rflat='flat_r_096to100.fits'
mosaic_average_flat,filelist,avzero,avdark,rflat

;i-band

filelist=[path+'/2005-04-07/dflat101.fits',path+'/2005-04-07/dflat102.fits',path+'/2005-04-07/dflat103.fits',path+'/2005-04-07/dflat104.fits',path+'/2005-04-07/dflat105.fits']

iflat='flat_i_101to105.fits'
mosaic_average_flat,filelist,avzero,avdark,iflat

;making the flattened willman 1
;g-band

filelist=['obj125.fits']
for i=126,130 do filelist=[filelist,'obj'+string(i,format='(I3.1)')+'.fits']
for i=138,142 do filelist=[filelist,'obj'+string(i,format='(I3.1)')+'.fits']
for i=0,11 do mosaic_flatten,path+'2005-04-07/'+filelist[i],avzero,avdark,gflat,path+'redux/Willman1/flatten_'+filelist[i]

;r-band
filelist=['obj132.fits']
for i=133,136 do filelist=[filelist,'obj'+string(i,format='(I3.1)')+'.fits']
for i=0,4 do mosaic_flatten,path+'2005-04-07/'+filelist[i],avzero,avdark,rflat,path+'redux/Willman1/flatten_'+filelist[i]
 
filelist=['obj179.fits']
for i=180,183 do filelist=[filelist,'obj'+string(i,format='(I3.1)')+'.fits']
for i=0,4 do mosaic_flatten,path+'2005-04-08/'+filelist[i],avzero,avdark,rflat,path+'redux/Willman1/flatten_'+filelist[i]

;i-band
filelist=['obj226.fits']
for i=227,232 do filelist=[filelist,'obj'+string(i,format='(I3.1)')+'.fits']
for i=0,6 do mosaic_flatten,path+'2005-04-09/'+filelist[i],avzero,avdark,iflat,path+'redux/Willman1/flatten_'+filelist[i]

; measure / fix / install astrometric headers (GSSS!)
dowcs, '/global/data/scr/mm1330/4meter/redux/Willman1'

; make mosaics
racen= 162.343
deccen= 51.051
dra=  0.05
ddec= 0.05
indir= '/global/data/scr/mm1330/4meter/redux/Willman1'
filelist= indir+'/af_obj'+['138','139','140','141','142']+'.fits'
gfilename= indir+'/Willman1-g.fits'
mosaic_mosaic, racen,deccen,dra,ddec,filelist,gfilename
filelist= indir+'/af_obj'+['179','180']+'.fits'
rfilename= indir+'/Willman1-r.fits'
mosaic_mosaic, racen,deccen,dra,ddec,filelist,rfilename
filelist= indir+'/af_obj'+['226']+'.fits'
ifilename= indir+'/Willman1-i.fits'
mosaic_mosaic, racen,deccen,dra,ddec,filelist,ifilename

; make jpg
rim= mrdfits(ifilename)
rim= rim-median(rim)
gim= mrdfits(rfilename)
gim= gim-median(gim)
bim= mrdfits(gfilename)
bim= bim-median(bim)
nw_rgb_make, rim,gim,bim,name='Willman1-irg.jpg', $
  scales=[3,3,3],nonlinearity=3, $
  quality=90

return
end
