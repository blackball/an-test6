;+
; NAME:
;  an_convert_usnob_to_fits
; PURPOSE:
;  Write one FITS file for each USNO-B1.0 "zone".
; BUGS:
;  - Not tested.
;  - Relies on *insane* idlutils code.
;  - Copies unchecked crap from idlutils code.
; REVISION HISTORY:
;  2006-03-01  started - Hogg
;-
pro an_convert_usnob_to_fits
catpath= getenv('USNOB10_PATH')
epochfile= catpath+'/USNO-B-epochs.fit'
ep= mrdfits(epochfile, 1, /silent)
hash= fltarr(10000)
hash[ep.field]= ep.epoch
ra0= 0.0D0
ra1= 3.6D2
prefix= 'b'
rec_len= 80L
prefix= 'b'
hoggprefix= 'hogg_'
for zone=0,1799 do begin
    dirstr= string(zone/10,format='(I3.3)')
    zonestr= string(zone,format='(I4.4)')
    path= catpath+'/'+dirstr
    splog, 'reading zone '+zonestr
    usno_readzone, path,zone,ra0,ra1,rec_len,prefix,data, $
      /swap_if_big_endian
    usnostruct= usnob10_extract(data)
    usnostruct.fldepoch = hash[usnostruct.fldid]
    fitsname= path+'/'+hoggprefix+zonestr+'.fits'
    splog, 'writing '+fitsname
    mwrfits, usnostruct,fitsname,/create
endfor
return
end
