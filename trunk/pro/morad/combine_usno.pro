pro combine_usno

path='/scratch/usnob_fits'

star=0
for subdir = 57,177 do begin
    dirstr=string(subdir, format='(I3.3)')
    fitsfile= path+'/'+dirstr+'/'+dirstr+'sweep.fit'
    temp_star=mrdfits(fitsfile,1)
    if n_elements(star) gt 1 then star=[star,temp_star] else star=temp_star
    print,dirstr,n_elements(temp_star),n_elements(star)
endfor
mwrfits,star,path+'/allstarsweep.fit',/create
return
end
