; make mosaics
pro makemosaics

racen= 162.343 ; 158.72
deccen= 51.051 ; 51.92
dra=  6.0/60.0
ddec= 6.0/60.0
nx= 5
ny= 5
indir= '/global/data/scr/morad/4meter/af'
prefix= indir+'/Willman_1_KPNO' ; '/Ursa_Major_dwarf_KPNO'
gprefix= prefix+'_g'
rprefix= prefix+'_r'
iprefix= prefix+'_i'
gbitmaskname= './mosaic_bitmask_g.fits'
rbitmaskname= './mosaic_bitmask_r.fits'
ibitmaskname= './mosaic_bitmask_i.fits'

; filelist= indir+'/afobj'+['059','060','061','062','063', $
;                           '185','186','187','188','189']+'.fits'
filelist= indir+'/afobj'+['125','126','127','128','129','130', $
                          '138','139','140','141','142']+'.fits'
mosaic_mosaic_grid, filelist,gprefix,racen,deccen,dra,ddec,nx,ny, $
  bitmaskname=gbitmaskname
mosaic_mosaic_grid_combine, gprefix,nx,ny,rebinfactor=2

; filelist= indir+'/afobj'+['064','065','066','067','068', $
;                           '191','192','193','194']+'.fits'
filelist= indir+'/afobj'+['132','133','134','135','136', $
                          '179','180','181','182','183']+'.fits'
mosaic_mosaic_grid, filelist,rprefix,racen,deccen,dra,ddec,nx,ny, $
  bitmaskname=rbitmaskname
mosaic_mosaic_grid_combine, rprefix,nx,ny,rebinfactor=2

; filelist= indir+'/afobj'+['070','071','072','073','074', $
;                           '223','224','225']+'.fits'
filelist= indir+'/afobj'+['226','227','228','229','230','231','232']+'.fits'
mosaic_mosaic_grid, filelist,iprefix,racen,deccen,dra,ddec,nx,ny, $
  bitmaskname=ibitmaskname
mosaic_mosaic_grid_combine, iprefix,nx,ny,rebinfactor=2

; make smosaics from SDSS data
hdr= headfits(gprefix+'.fits')
naxis1= sxpar(hdr,'NAXIS1')
naxis2= sxpar(hdr,'NAXIS2')
extast, hdr,bigast
naxis= create_struct('naxis1',naxis1,'naxis2',naxis2)
bigast= struct_addtags(bigast,naxis)
sprefix= indir+'/Willman_1_SDSS' ; '/Ursa_Major_dwarf_SDSS'
smosaic_make, astrans=bigast,prefix=sprefix,/fpbin,rerun=137, $
  smfwhm=0.8,nskyframe=3,/dropweights

; combine SDSS with KPNO to replace saturated pixels

return
end
