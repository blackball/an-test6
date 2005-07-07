;INPUTS
;
;  seed      -the seed for the random sequence (if not set seed=7l)
;  nfields   -number of fields wanted (if not set set nfields=1)
; this code only returns fields within 60 degree of the north galactic pole




pro sdssfield,seed=seed,nfields=nfields


objtype={ifield:0d, ra:0d, dec:0d, rowc:0d, colc:0d, rpsfflux:0d, rmag:0d}

if(n_tags(flist) eq 0) then window_read,flist=flist

if not keyword_set(seed) then seed=7l
if not keyword_set(nfields) then nfields=1l

glactc,ngpra,ngpdec,2000,0,90,2,/degree
nn=0l
while (nn lt nfields) do begin
    r=floor(randomu(seed)*n_elements(flist))
    if (djs_diff_angle(flist[r].ra,flist[r].dec,ngpra,ngpdec) lt 60) then begin
        field=sdss_readobj(flist[r].run,flist[r].camcol,flist[r].field,rerun=flist[r].rerun)

        
        
        ind=sdss_selectobj(field,objtype=['star','galaxy'],/trim)
        obj=replicate(objtype,n_elements(ind))
        obj.ifield=field[ind].ifield
        obj.ra=field[ind].ra
        obj.dec=field[ind].dec
        obj.rowc=field[ind].rowc[2]
        obj.colc=field[ind].colc[2]
        obj.rpsfflux=field[ind].psfflux[2]
        obj.rmag=22.5-2.5*alog10(field[ind].psfflux[2])
        mwrfits,obj,'field-'+string(format='(I6.6)',obj[0].ifield)+'.fits'
        nn=nn+1
    endif
endwhile
end
