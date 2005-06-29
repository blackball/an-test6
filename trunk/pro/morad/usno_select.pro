pro usno_select,minmag=minmag,maxmag=maxmag,poserror=poserror,propcut=propcut

path='/scratch/usnob_fits'


;set defaults

;defining the cuts
if not keyword_set(minmag) then minmag=18.5 ; faintest R-band  magnitude accepted 
if not keyword_set(maxmag) then maxmag=14.0   ; objects brighter that maxmag in any band (B,R,I) are rejecdted
if not keyword_set(poserror) then poserror=200. ; maximum error allowed in the RA or Dec in milli arcsec
if not keyword_set(propcut) then propcut=10.   ; the maximum proper motion allowed in milli arcsec per year 


sweeptype={ra:0d, dec:0d,mag:fltarr(5)}

glactc,ngpra,ngpdec,2000,0,90,2,/degree

for subdir = 57,179 do begin
    sweep=0
    for i=0,9 do begin
        fname= 'b'+string(subdir*10+i,format='(I4.4)')
        dirstr=string(subdir, format='(I3.3)')
        fitsfile= path+'/'+dirstr+'/'+fname+'.fit'
        star=mrdfits(fitsfile,1,/silent)
        sdsscut=where(djs_diff_angle(star.ra,star.dec,ngpra,ngpdec) lt 60)
        if sdsscut[0] ne -1 then begin
            sdssstar=star[sdsscut]
            mcut=where(sdssstar.mag[3] lt minmag)
            sdeccut=where((sdssstar[mcut].sde lt poserror) and (sdssstar[mcut].sre lt poserror))
            prop=sqrt(sdssstar[mcut[sdeccut]].mura^2+sdssstar[mcut[sdeccut]].mudec^2)
            propcut=where(prop lt propcut)
            allmagcut=where((sdssstar[mcut[sdeccut[propcut]]].mag[2] gt maxmag) and (sdssstar[mcut[sdeccut[propcut]]].mag[3] gt maxmag) and (sdssstar[mcut[sdeccut[propcut]]].mag[4] gt maxmag))
            temp_sweep=replicate(sweeptype,n_elements(allmagcut))
            temp_sweep.ra=sdssstar[mcut[sdeccut[propcut[allmagcut]]]].ra
            temp_sweep.dec=sdssstar[mcut[sdeccut[propcut[allmagcut]]]].dec
            temp_sweep.mag=sdssstar[mcut[sdeccut[propcut[allmagcut]]]].mag
            if (n_elements(sweep) gt 1 ) then sweep=[sweep,temp_sweep] else sweep=temp_sweep
        endif
        print, fname,n_elements(sdsscut),n_elements(temp_sweep)
    endfor
if (n_elements(sweep) gt 1 ) then mwrfits,sweep,path+'/'+dirstr+'/'+dirstr+'sweep.fit',/create
endfor
return
end
