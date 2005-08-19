;INPUTS
;
;  seed      -the seed for the random sequence (default 7l)
;  nfields   -number of fields wanted (default 1)
;  maxobj    -the number of sources to return per field (default 100)
;  band      -band to use to sort sources by psfflux (default 2)
;  /order    -return nfields in order.
;  /all      -get all fields
;  /ngc      -return only fields in the NGC

pro sdssfield, seed=seed,nfields=nfields,maxobj=maxobj,band=band, $
               order=order,all=all,ngc=ngc

; set filenames
if keyword_set(all) then outfile='sdssfieldall' $
else outfile='sdssfield'
openw,1,outfile+'.xyls'

objtype={ifield:0d, ra:0d, dec:0d, rowc:0d, colc:0d, rpsfflux:0d}

; read fields
if(n_tags(flist) eq 0) then window_read,flist=flist

; trim to NGC
glactc,ngpra,ngpdec,2000D,0D,90D,2,/degree
if keyword_set(ngc) then begin
    ngc= where(djs_diff_angle(flist.ra,flist.dec,ngpra,ngpdec) lt 60,maxfields)
    flist= flist[ngc]
endif else begin
    maxfields= n_elements(flist)
endelse

; set defaults
if not keyword_set(seed) then seed=7l
if not keyword_set(nfields) then nfields=1l
if not keyword_set(maxobj) then maxobj=100
if not keyword_set(band) then band=2
if keyword_set(all) then nfields=maxfields
printf,1,'NumFields='+strtrim(string(nfields),2)
print,nfields

; shuffle
if not keyword_set(order) then $
  flist= flist[shuffle_indx(n_elements(flist),seed=seed)]

for nn=0L,nfields-1L do begin
    field=sdss_readobj(flist[nn].run,flist[nn].camcol,flist[nn].field, $
                       rerun=flist[nn].rerun)
    ind=sdss_selectobj(field,objtype=['star','galaxy'],/trim)
    sindx= reverse(sort(field[ind].psfflux[band]))
    good= sindx[0:((maxobj < n_elements(sindx))-1)]
    obj=replicate(objtype,n_elements(good))
    obj.ifield=field[ind[good]].ifield
    obj.ra=field[ind[good]].ra
    obj.dec=field[ind[good]].dec
    obj.rowc=field[ind[good]].rowc[band]
    obj.colc=field[ind[good]].colc[band]
    obj.rpsfflux=field[ind[good]].psfflux[band]
    if (nn eq 0) then mwrfits,obj,outfile+'.fits',/creat $
    else mwrfits,obj,outfile+'.fits'
    print,'# objects which made the cuts',n_elements(good)
    printf,1,'# ifield: '+string(obj[0].ifield)
    tmp_str=strtrim(string(n_elements(obj)),2)
    for kk=0,n_elements(obj)-1 do begin
        tmp_str=tmp_str+','+strtrim(string(obj[kk].rowc),2)+','+strtrim(string(obj[kk].colc),2)
    endfor
    printf,1,tmp_str
endfor
close,1
end
