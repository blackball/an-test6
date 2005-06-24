pro sdssfield

;this file might pick a field which is not in sam's index file(
;i.e. not in the north ) so check for the ra and dec!
startype={ifield:0d, ra:0d, dec:0d, rowc:0d, colc:0d, rpsfflux:0d}

if(n_tags(flist) eq 0) then window_read,flist=flist
seed=7

r=floor(randomu(seed)*n_elements(flist))

obj=sdss_readobj(flist[r].run,flist[r].camcol,flist[r].field,rerun=flist[r].rerun)

ind=sdss_selectobj(obj,objtype='star',/trim)

star=replicate(startype,n_elements(ind))
star.ifield=obj[ind].ifield
star.ra=obj[ind].ra
star.dec=obj[ind].dec
star.rowc=obj[ind].rowc[2]
star.colc=obj[ind].colc[2]
star.rpsfflux=obj[ind].psfflux[2]

mwrfits,star,'field-'+string(star[0].ifield)+'.fit'

end
