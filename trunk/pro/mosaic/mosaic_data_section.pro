pro mosaic_data_section, filename,hdu,xmin,xmax,ymin,ymax
hdr= headfits(filename,exten=hdu)
string= sxpar(hdr,'DATASEC')
startxmin= strpos(string,'[')
startxmax= strpos(string,':')
startymin= strpos(string,',')
startymax= strpos(string,':',startymin)
endymax= strpos(string,']')
xmin= long(strmid(string,startxmin+1,startxmax-startxmin-1))-1
xmax= long(strmid(string,startxmax+1,startymin-startxmax-1))-1
ymin= long(strmid(string,startymin+1,startymax-startymin-1))-1
ymax= long(strmid(string,startymax+1,endymax-startymax-1))-1
help, xmin,xmax,ymin,ymax
return
end
