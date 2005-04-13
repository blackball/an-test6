; make log of mosaic data from headers
pro mosaic_log

path='/global/data/scr/morad/4meter'
filelist=file_search(path+'/2005-04-0*/obj*.fits*')
nfile=n_elements(filelist)

for ii=0,nfile-1 do begin
    hdr= headfits(filelist[ii])
    tmp= strsplit(filelist[ii],'/',/extract)
    shortfile= tmp[n_elements(tmp)-1]
    exptime= sxpar(hdr,'EXPTIME')
    filter= strmid(sxpar(hdr,'FILTER'),0,1)
    object= sxpar(hdr,'OBJECT')
    line= shortfile+string(round(exptime))+' '+filter+' '+object
    line= strjoin(strsplit(line,' ',/extract),' ')
    splog, line
endfor
return
end
