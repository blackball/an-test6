pro go

path='/global/data/scr/morad/4meter'

; estimate cross-talk:
filelist=file_search(path+'/2005-04-??/obj*.fits*')
for ii=0,n_elements(filelist)-1 do mosaic_crosstalk, filelist[ii]
mosaic_crosstalk_analyze

; make the averaged zero:
avzero='zero_av168to177.fits'
if (NOT file_test(avzero)) then begin
    filelist=file_search(path+'/2005-04-08/zero*.fits')
    mosaic_average_zero,filelist,avzero
endif

; make the averaged dark
avdark='dark_av121to123-158to160.fits'
if (NOT file_test(avdark)) then begin
    filelist=file_search(path+'/2005-04-07/dark*.fits')
    mosaic_average_dark,filelist,avzero,avdark
endif

; make the averaged flats
; g-band
gflat= 'flat_g_091to095.fits'
if (NOT file_test(gflat)) then begin
    filelist= path+'/2005-04-07/dflat'+['091','092','093','094','095']+'.fits'
    mosaic_average_flat,filelist,avzero,avdark,gflat
endif

; r-band
rflat= 'flat_r_096to100.fits'
if (NOT file_test(rflat)) then begin
    filelist= path+'/2005-04-07/dflat'+['096','097','098','099','100']+'.fits'
    mosaic_average_flat,filelist,avzero,avdark,rflat
endif

; i-band
iflat= 'flat_i_101to105.fits'
if (NOT file_test(iflat)) then begin
    filelist= path+'/2005-04-07/dflat'+['101','102','103','104','105']+'.fits'
    mosaic_average_flat,filelist,avzero,avdark,iflat
endif

; flatten obj files
flatdir= path+'/f'
cmd= 'mkdir -p '+flatdir
splog, cmd
spawn, cmd
filelist=file_search(path+'/2005-04-??/obj*.fits*')
for ii=0,n_elements(filelist)-1 do begin
    tmp= strsplit(filelist[ii],'/',/extract)
    outfile= flatdir+'/f'+tmp[n_elements(tmp)-1]
    if (NOT file_test(outfile)) then begin
        filter= sxpar(headfits(filelist[ii]),'FILTER')
        if (strmid(filter,0,1) EQ 'g') then flat= gflat $
        else if (strmid(filter,0,1) EQ 'r') then flat= rflat $
        else if (strmid(filter,0,1) EQ 'i') then flat= iflat $
        else begin
            splog, 'ERROR: unrecognized filter: '+filter
            return
        endelse
        splog, 'making '+outfile+' from '+filelist[ii]+' using '+flat
        mosaic_flatten,filelist[ii],avzero,avdark,flat,outfile
    endif else begin
        splog, outfile+' already exists, skipping'
    endelse
endfor

; measure / fix / install astrometric headers (GSSS!)
spawn, 'mkdir -p '+path+'/newaf'
dowcs, flatdir,path+'/newaf'
spawn, '\rm -rfv '+path+'/oldaf'
spawn, '\mv -fv '+path+'/af '+path+'/oldaf'
spawn, '\mv -fv '+path+'/newaf '+path+'/af'

; make bitmask
bitmaskname= 'mosaic_bitmask.fits' 
if (NOT file_test(bitmaskname)) then begin
    mosaic_bitmask,avzero,avdark,gflat,bitmaskname
endif

; update the flats on large scales using comparisons to SDSS sources

; readjust flattening

; calibrate to SDSS

; make mosaics
; makemosaics

return
end
