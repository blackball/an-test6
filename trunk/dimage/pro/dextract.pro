;+
; NAME:
;   dextract
; PURPOSE:
;   extract objects in an image
; CALLING SEQUENCE:
;   dextract, image, invvar, object=object, extract=extract [ , small=]
; INPUTS:
;   image - [nx, ny] input image
;   invvar - [nx, ny] input image inverse variance
;   object - [nx, ny] which object each pixel corresponds to
; OPTIONAL INPUTS:
;   small - if set, only process objects with size of detected area
;           smaller than this value
; OUTPUTS:
;   extract - [N] arrays of structures with objects and properties
;                 .XST - X start of atlas
;                 .YST - Y start of atlas
;                 .XND - X end of atlas
;                 .YND - Y end of atlas
;                 .XCEN
;                 .YCEN
;                 .ATLAS - atlas structure
; REVISION HISTORY:
;   11-Jan-2006  Written by Blanton, NYU
;-
;------------------------------------------------------------------------------
pro dextract, image, invvar, object=object, extract=extract, small=small

nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]
asize=2*small+1L

isort=sort(object)
iuniq=uniq(object[isort])
istart=iuniq[0]+1
xcen=fltarr(max(object)+1)
ycen=fltarr(max(object)+1)
xfit=fltarr(max(object)+1)
yfit=fltarr(max(object)+1)
xst=lonarr(max(object)+1)
yst=lonarr(max(object)+1)
xnd=lonarr(max(object)+1)
ynd=lonarr(max(object)+1)
good=lonarr(max(object)+1)
atlas=fltarr(asize,asize,max(object)+1)
atlas_ivar=fltarr(asize,asize,max(object)+1)
for i=1L, n_elements(iuniq)-1L do begin
    iend=iuniq[i]
    ipix=isort[istart:iend]
    ipgood=where(invvar[ipix] gt 0, npgood)
    ix=ipix mod nx              ;
    iy=ipix / nx;
    ixlims=minmax(ix)
    iylims=minmax(iy)
    
    mit=1
    if(keyword_set(small)) then begin
        mit= (ixlims[1]-ixlims[0] lt small AND $
              iylims[1]-iylims[0] lt small)
    endif
    if(mit gt 0) then begin
        subimg=image[ixlims[0]:ixlims[1], $
                     iylims[0]:iylims[1]]
        suboim=object[ixlims[0]:ixlims[1], $
                      iylims[0]:iylims[1]]
        subinv=invvar[ixlims[0]:ixlims[1], $
                      iylims[0]:iylims[1]]
        inot=where(suboim ge 0 and suboim ne i-1 and subinv gt 0, nnot)
        sm=dsmooth(subimg, 2.)
        smmax=max(sm, icen)
        xst[i-1]=ixlims[0]
        xnd[i-1]=ixlims[1]
        yst[i-1]=iylims[0]
        ynd[i-1]=iylims[1]
        xcen[i-1]=(icen mod (ixlims[1]-ixlims[0]+1))+ixlims[0]
        ycen[i-1]=(icen / (ixlims[1]-ixlims[0]+1))+iylims[0]
        dcen3x3, sm[xcen[i-1]-ixlims[0]-1:xcen[i-1]-ixlims[0]+1, $
                    ycen[i-1]-iylims[0]-1:ycen[i-1]-iylims[0]+1], xx, yy
        xfit[i-1]=xx+float(xcen[i-1])-1.
        yfit[i-1]=yy+float(ycen[i-1])-1.
        if(xx le -1. OR xx ge 3. OR yy le -1. OR yy ge 3.) then stop

        sigma=1./sqrt(median(subinv))
        atlas[*,*,i-1]=randomn(seed, asize,asize)*sigma
        atlas_ivar[*,*,i-1]=sigma
        ioff=(asize/2L)-(xcen[i-1]-xst[i-1])
        joff=(asize/2L)-(ycen[i-1]-yst[i-1])
        ixall=lindgen(asize)#replicate(1., asize)
        iyall=transpose(ixall)
        ixim=ixall+xcen[i-1]-asize/2L
        iyim=iyall+ycen[i-1]-asize/2L
        iin=where(ixim ge 0L and ixim lt nx and $
                  iyim ge 0L and iyim lt nx, nin)
        for l=0L, nin-1L do $
          atlas[ixall[iin[l]],iyall[iin[l]], i-1]= $
          randomn(seed, 1)*1./sqrt(invvar[ixim[iin[l]], iyim[iin[l]]])
        for l=0L, nin-1L do $
          atlas_ivar[ixall[iin[l]],iyall[iin[l]], i-1]= $
          invvar[ixim[iin[l]], iyim[iin[l]]]
          
        ixat=ix[ipgood]-xst[i-1]+ioff
        iyat=iy[ipgood]-yst[i-1]+joff
        iin=where(ixat ge 0L and ixat lt asize AND $
                  iyat ge 0L and iyat lt asize, nin)
        for l=0L, nin-1L do $
          atlas[ixat[iin[l]], iyat[iin[l]], i-1]= image[ipix[ipgood[iin[l]]]]
        for l=0L, nin-1L do $
          atlas_ivar[ixat[iin[l]], iyat[iin[l]], i-1]= $
          invvar[ipix[ipgood[iin[l]]]]
        good[i-1]=1
    endif
    istart=iend+1
endfor

igood=where(good, ngood)
extract=replicate({xcen:0., $
                   ycen:0., $
                   xfit:0., $
                   yfit:0., $
                   xst:0L, $
                   yst:0L, $
                   xnd:0L, $
                   ynd:0L, $
                   atlas:fltarr(asize, asize), $
                   atlas_ivar:fltarr(asize, asize)}, ngood)
extract.xcen=xcen[igood]
extract.ycen=ycen[igood]
extract.xfit=xfit[igood]
extract.yfit=yfit[igood]
extract.xst=xst[igood]
extract.yst=yst[igood]
extract.xnd=xnd[igood]
extract.ynd=ynd[igood]
extract.atlas=atlas[*,*,igood]
extract.atlas_ivar=atlas_ivar[*,*,igood]

end
;------------------------------------------------------------------------------
