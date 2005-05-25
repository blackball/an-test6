function hogg_acs_filter, hdr
if (strmid(sxpar(hdr,'INSTRUME'),0,3) NE 'ACS') then return, '0'
filter1= sxpar(hdr,'FILTER1')
filter2= sxpar(hdr,'FILTER2')
if (strmid(filter1,0,1) EQ 'F') then filter= filter1 else filter= filter2
filter= strtrim(filter,2)
return, filter
end
