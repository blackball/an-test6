pro dowcs, indir,outdir
filelist= file_search(indir+'/flatten_obj???.fits*')
nfile= n_elements(filelist)
for ii=0L,nfile-1 do begin
    path= strmid(filelist[ii],0,strpos(filelist[ii],'/',/reverse_search))
    nnstr= strmid(filelist[ii],strpos(filelist[ii],'obj',/reverse_search)+3,3)
    in= filelist[ii]
    out= outdir+'/af_obj'+nnstr+'.fits'
    if file_test(out) then begin
        splog, out+' already exists; skipping'
    endif else begin
        mosaic_wcs, in,out
        if file_test(outdir+'/af_obj'+nnstr+'_chip1.jpg') then begin
            read_jpeg, outdir+'/af_obj'+nnstr+'_chip1.jpg',canvas1
            for jj=2,4 do begin
                chip= string(jj,format='(I1)')
                read_jpeg, outdir+'/af_obj'+nnstr+'_chip'+chip+'.jpg',canvas
                canvas1= [[canvas1],[canvas]]
            endfor
            read_jpeg, outdir+'/af_obj'+nnstr+'_chip5.jpg',canvas2
            for jj=6,8 do begin
                chip= string(jj,format='(I1)')
                read_jpeg, outdir+'/af_obj'+nnstr+'_chip'+chip+'.jpg',canvas
                canvas2= [[canvas2],[canvas]]
            endfor
            canvas= [[[temporary(canvas1)]],[[bytarr(3,4272,20)]], $
                     [[temporary(canvas2)]]]
            write_jpeg, outdir+'/af_obj'+nnstr+'.jpg',canvas,true=1,quality=80
            cmd= '\rm '+outdir+'/af_obj'+nnstr+'_chip?.jpg'
            splog, cmd
            spawn, cmd
        endif
    endelse
endfor
return
end
