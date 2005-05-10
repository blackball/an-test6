pro dowcs, indir,outdir
if (NOT keyword_set(outdir)) then outdir= indir
filelist= file_search(indir+'/fobj???.fits*')
nfile= n_elements(filelist)
for ii=0L,nfile-1 do begin
    path= strmid(filelist[ii],0,strpos(filelist[ii],'/',/reverse_search))
    nnstr= strmid(filelist[ii],strpos(filelist[ii],'obj',/reverse_search)+3,3)
    in= filelist[ii]
    out= outdir+'/afobj'+nnstr+'.fits'
    if file_test(out) then begin
        splog, out+' already exists; skipping'
    endif else begin
        mosaic_wcs, in,out
    endelse
endfor
return
end
