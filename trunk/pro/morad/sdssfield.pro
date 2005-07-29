;INPUTS
;
;  seed      -the seed for the random sequence (if not set seed=7l)
;  nfields   -number of fields wanted (if not set  nfields=1)
;  minmag    -the faint end cut on the r-band magnitude(if not set
;  minmag=19.) this is one magnetude fanter that USNO cut.  
; this code only returns fields within 60 degree of the north galactic pole




pro sdssfield,seed=seed,nfields=nfields,minmag=minmag

openw,1,'sdssfield.xyls'
objtype={ifield:0d, ra:0d, dec:0d, rowc:0d, colc:0d, rpsfflux:0d, rmag:0d}

if(n_tags(flist) eq 0) then window_read,flist=flist

if not keyword_set(seed) then seed=7l
if not keyword_set(nfields) then nfields=1l
if not keyword_set(minmag) then minmag=19.0

printf,1,'NumFields='+strtrim(string(nfields),2)

glactc,ngpra,ngpdec,2000,0,90,2,/degree
nn=0l
while (nn lt nfields) do begin
    r=floor(randomu(seed)*n_elements(flist))
    if (djs_diff_angle(flist[r].ra,flist[r].dec,ngpra,ngpdec) lt 60) then begin
        field=sdss_readobj(flist[r].run,flist[r].camcol,flist[r].field,rerun=flist[r].rerun)
        
        
        ind=sdss_selectobj(field,objtype=['star','galaxy'],/trim)
        rmag=22.5-2.5*alog10(field[ind].psfflux[2])
        good=where(rmag lt minmag)
        obj=replicate(objtype,n_elements(good))
        obj.ifield=field[ind[good]].ifield
        obj.ra=field[ind[good]].ra
        obj.dec=field[ind[good]].dec
        obj.rowc=field[ind[good]].rowc[2]
        obj.colc=field[ind[good]].colc[2]
        obj.rpsfflux=field[ind[good]].psfflux[2]
        obj.rmag=rmag[good]
        mwrfits,obj,'field-'+string(format='(I6.6)',obj[0].ifield)+'.fits',/creat
        print,'# objects which made the cuts',n_elements(good)
;        printf,1,'# ifield: '+string(obj[0].ifield)
        tmp_str=strtrim(string(n_elements(obj)),2)
        for kk=0,n_elements(obj)-1 do begin
            tmp_str=tmp_str+','+strtrim(string(obj[kk].rowc),2)+','+strtrim(string(obj[kk].colc),2)
        endfor
        printf,1,tmp_str
        nn=nn+1
    endif
endwhile
close,1
end
