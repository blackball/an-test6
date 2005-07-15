; make JPGs
pro makejpgs

indir= '/global/data/scr/morad/4meter/af'
prefix= indir+'/Willman_1_KPNO' ; '/Ursa_Major_dwarf_KPNO'
gprefix= prefix+'_g'
rprefix= prefix+'_r'
iprefix= prefix+'_i'

rim= mrdfits(rprefix+'.fits')
bw_est_sky, rim,sky
rim= rim-temporary(sky)
bim= mrdfits(gprefix+'.fits')
bw_est_sky, bim,sky
bim= bim-temporary(sky)
gim= (rim+bim)/2.0
scales= [10.0,10.0,10.0]*2.0
nw_rgb_make, rim,gim,bim,name=prefix+'.jpg', $
  scales=scales,nonlinearity=3.0,rebinfactor=1, $
  quality=90

return
end
