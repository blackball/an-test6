; make mosaics
pro makemosaics

racen= 158.72
deccen= 51.92
dra=  8.0/60.0
ddec= 8.0/60.0
nx= 5
ny= 5
indir= '/global/data/scr/morad/4meter/af'
prefix= indir+'/Ursa_Major_dwarf_KPNO'
gprefix= prefix+'_g'
rprefix= prefix+'_r'
iprefix= prefix+'_i'

filelist= indir+'/afobj'+['059','060','061','062','063', $
                           '185','186','187','188','189']+'.fits'
mosaic_mosaic_grid, filelist,gprefix,racen,deccen,dra,ddec,nx,ny 
mosaic_mosaic_grid_combine, gprefix,nx,ny,rebinfactor=2

filelist= indir+'/afobj'+['064','065','066','067','068', $
                           '191','192','193','194']+'.fits'
mosaic_mosaic_grid, filelist,rprefix,racen,deccen,dra,ddec,nx,ny
mosaic_mosaic_grid_combine, rprefix,nx,ny,rebinfactor=2

; filelist= indir+'/afobj'+['226','227','228','229','230','231','232']+'.fits'
filelist= indir+'/afobj'+['070','071','072','073','074', $
                           '223','224','225']+'.fits'
mosaic_mosaic_grid, filelist,iprefix,racen,deccen,dra,ddec,nx,ny
mosaic_mosaic_grid_combine, iprefix,nx,ny,rebinfactor=2

; make smosaics from SDSS data
hdr= headfits(gprefix+'fits')
sprefix= indir+'/Ursa_Major_dwarf_SDSS'
; smosaic_make

; combine SDSS with KPNO to replace saturated pixels

; make jpg
rim= mrdfits(iprefix+'.fits')
bw_est_sky, rim,sky
rim= rim-temporary(sky)
gim= mrdfits(rprefix+'.fits')
bw_est_sky, gim,sky
gim= gim-temporary(sky)
bim= mrdfits(gprefix+'.fits')
bw_est_sky, bim,sky
bim= bim-temporary(sky)
nw_rgb_make, rim,gim,bim,name=prefix+'.jpg', $
  scales=[5.0,4.0,8.0],nonlinearity=3.0,rebinfactor=1, $
  quality=90

return
end
