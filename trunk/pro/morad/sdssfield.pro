;+
; NAME:
;  sdssfield
;
; PURPOSE:
;  Extract x,y list from an SDSS field for the astrometry.net demo
;  project.
;
; INPUTS:
;
; OPTIONAL INPUTS:
;  seed      - the seed for the random sequence (default 7l)
;  nfields   - number of fields wanted (default 1)
;  chunksize - number of fields to return in each chunk (default 10000)
;  maxobj    - the number of sources to return per field (default 300)
;  band      - band to use to sort sources by psfflux (default 2)
;  run       - all fields for the specified run will be returned
;
; KEYWORDS:
;  /order    - return nfields in order.
;  /ngc      - return only fields in the NGC
;
; BUGS:
;  - Won't, in general, give you back *exactly* the number of fields
;    you requested!  This is for dumb reasons, but trust us, it's for
;    your own good.
;
; REVISION HISTORY:
;  2005-08-??  started by Masjedi (NYU)
;  2005-09-14  chunks - Hogg
;-
pro sdssfield, seed=seed,nfields=nfields,maxobj=maxobj,band=band, $
               order=order,ngc=ngc,run=run,chunksize=chunksize

objtype= {run:0L, $
          rerun:0L, $
          camcol:0, $
          field:0L, $
          filter:0, $
          ifield:0L, $
          ra:0d, $
          dec:0d, $
          rowc:0d, $
          colc:0d, $
          psfflux:0d}

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
if not keyword_set(seed) then seed=7L
if not keyword_set(nfields) then nfields=1L
nfields= nfields < maxfields
if not keyword_set(chunksize) then chunksize=10000L
if not keyword_set(maxobj) then maxobj=300
if not keyword_set(band) then band=2
prefix= 'sdssfield'
splog, nfields

; shuffle
if not (keyword_set(order) or keyword_set(run)) then $
  flist= flist[shuffle_indx(n_elements(flist),seed=seed)]

; specific run
if keyword_set(run) then begin
    flist= flist[where(flist.run eq run)]
    nfields= n_elements(flist)
endif

; loop over chunks, setting filenames
nchunks= ceil(float(nfields)/float(chunksize))
for chunk=1L,nchunks do begin
    outfile= prefix
    if (nchunks GT 1) then begin
        ndigit= ceil(alog10(nchunks+1.0))
        format= '(I'+string(ndigit,'(I1)')+'.'+string(ndigit,'(I1)')+')'
        splog, format
        outfile= prefix+string(chunk,format=format)
    endif

    txtfile= outfile+'.xyls'
    if file_test(txtfile) then begin
        splog, 'file '+txtfile+' already exists; skipping'
    endif else begin
        wlun= 1
        openw, wlun,txtfile
        printf, wlun,'NumFields='+strtrim(string(chunksize),2)

        created= 0
        for rr=(chunk-1L)*chunksize,((chunk*chunksize-1L)<(nfields-1L)) do begin
            field=sdss_readobj(flist[rr].run,flist[rr].camcol, $
                               flist[rr].field,rerun=flist[rr].rerun)
            if (n_tags(field) GT 1) then begin
                ind= sdss_selectobj(field,objtype=['star','galaxy'],/trim)
                if (ind[0] GE 0) then begin
                    sindx= reverse(sort(field[ind].psfflux[band]))
                    good= sindx[0:((maxobj < n_elements(sindx))-1)]
                    obj= replicate(objtype,n_elements(good))
                    obj.run=    field[ind[good]].run
                    obj.rerun=  field[ind[good]].rerun
                    obj.camcol= field[ind[good]].camcol
                    obj.field=  field[ind[good]].field
                    obj.filter= band
                    obj.ifield= field[ind[good]].ifield
                    obj.ra=     field[ind[good]].ra
                    obj.dec=    field[ind[good]].dec
                    obj.rowc=   field[ind[good]].rowc[band]
                    obj.colc=   field[ind[good]].colc[band]
                    obj.psfflux=field[ind[good]].psfflux[band]
                    if (created EQ 0) then begin
                        fitsname= outfile+'.fits'
                        splog, 'starting file '+fitsname
                        mwrfits, obj,fitsname,/create
                        created= 1
                    endif else mwrfits,obj,fitsname
                    splog, n_elements(good),' objects made the cuts, brightest has flux',field[ind[good[0]]].psfflux[band]
                    printf, wlun,'#' $
                      +' '+strtrim(string(obj[0].run),2) $
                      +' '+strtrim(string(obj[0].rerun),2) $
                      +' '+strtrim(string(obj[0].camcol),2) $
                      +' '+strtrim(string(obj[0].field),2) $
                      +' '+strtrim(string(obj[0].filter),2) $
                      +' '+strtrim(string(obj[0].ifield),2)
                    tmp_str=strtrim(string(n_elements(obj)),2)
                    for kk=0,n_elements(obj)-1 do begin
                        tmp_str=tmp_str $
                          +','+strtrim(string(obj[kk].rowc, $
                                              format='(F8.1)'),2) $
                          +','+strtrim(string(obj[kk].colc, $
                                              format='(F8.1)'),2)
                    endfor
                    printf, wlun,tmp_str
                endif
            endif
        endfor
        close, wlun
    endelse
endfor
end
