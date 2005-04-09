pro mosaic_mosaic,racen,deccen,dra,ddec

if (not keyword_set(racen)) then racen=162.343
if (not keyword_set(deccen)) then deccen=51.051
if (not keyword_set(dra)) then dra=.02
if (not keyword_set(ddec)) then ddec=.02


;create RA---TAN, DEC--TAN wcs header
pixscale=.26
bigast=smosaic_hdr(racen,deccen,dra,dec,pixscale=pixscale)

