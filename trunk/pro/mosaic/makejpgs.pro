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
scales= [10.0,10.0,10.0]
nonlinearity= 3
quality= 90
rebinfactor= 1
nw_rgb_make, rim,gim,bim,name=prefix+'_gr.jpg', $
  scales=scales,nonlinearity=nonlinearity,rebinfactor=rebinfactor, $
  quality=quality

gim= rim
rim= mrdfits(iprefix+'.fits')
bw_est_sky, rim,sky
rim= rim-temporary(sky)
scales= [10.0,8.0,16.0]
nw_rgb_make, rim,gim,bim,name=prefix+'_gri.jpg', $
  scales=scales,nonlinearity=nonlinearity,rebinfactor=rebinfactor, $
  quality=quality

return
end
