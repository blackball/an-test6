pro test_an_tweak
imagefile= '~dwh2/astrometry/data/beth/cand11+440018.red.fits'
xyfile= '~dwh2/astrometry/data/beth/gchip1_18.als'
readcol, xyfile,starID,sx,sy,mag,magerr,skyval, $
  niter,CHI,SHARP,/silent
starlist= replicate({x:0,y:0},n_elements(sx))
starlist.x= sx
starlist.y= sy
newastr= an_tweak(imagefile,xylist=starlist,qa='test_an_tweak.ps')
return
end

