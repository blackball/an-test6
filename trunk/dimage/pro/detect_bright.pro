;+
; NAME:
;   detect_bright
; PURPOSE:
;   detect objects in image of bright object
; CALLING SEQUENCE:
;   dimage, image
; INPUTS:
;   image - [nx, ny] input image
; COMMENTS:
;   Assumes a sky-subtracted image
; REVISION HISTORY:
;   11-Jan-2006  Written by Blanton, NYU
;-
;------------------------------------------------------------------------------
pro detect_bright, image, psmooth=psmooth

common atv_point, markcoord

nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]

;; do general object detection
sigma=dsigma(image)
invvar=fltarr(nx,ny)+1./sigma^2
dobjects, image, invvar, object=oimage, plim=25., dpsf=1.

iobj=where(oimage eq oimage[nx/2L, ny/2L])
ixobj=iobj mod nx
iyobj=iobj / nx
xstart=min(ixobj)
xend=max(ixobj)
ystart=min(iyobj)
yend=max(iyobj)
nxnew=xend-xstart+1L
nynew=yend-ystart+1L

timage=image[xstart:xend, ystart:yend]
oimage=oimage[xstart:xend, ystart:yend]

;; now choose the object that includes the center
iobj=where(oimage eq oimage[nx/2L-xstart, ny/2L-ystart] OR oimage eq -1L)

iimage=randomn(seed, nxnew, nynew)*sigma
iimage[iobj]=timage[iobj]
iivar=fltarr(nxnew,nynew)+1./sigma^2

;; find all peaks 
dpeaks, iimage, xc=xc, yc=yc, sigma=sigma, minpeak=25.*sigma
xpeaks=xc*float(subpix)
ypeaks=yc*float(subpix)

;; try and guess which peaks are PSFlike

;; measure colors of PSFlike peaks

;; 

;; smooth, resample, and find peaks at a reasonable scale 
if(0) then begin
psmooth=6.
subpix=long(psmooth/3.) > 1L
nxsub=nx/subpix
nysub=ny/subpix
simage=rebin(iimage[0:nxsub*subpix-1, 0:nysub*subpix-1], nxsub, nysub)
simage=dsmooth(simage, psmooth/float(subpix))

ssig=dsigma(simage)
sivar=fltarr(nxsub, nysub)+1./ssig^2

deblend, simage, sivar, nchild=nchild, xcen=xcen, ycen=ycen, $
  children=children, templates=templates

dpeaks, simage, xc=xc, yc=yc, sigma=ssig, minpeak=25.*ssig
xpeaks=xc*float(subpix)
ypeaks=yc*float(subpix)

for i=0L, n_elements(xpeaks)-1L do begin
  xst=long(xpeaks[i]-1.5*subpix)>0L
  xnd=long(xpeaks[i]+1.5*subpix)<(nx-1L)
  yst=long(ypeaks[i]-1.5*subpix)>0L
  ynd=long(ypeaks[i]+1.5*subpix)<(ny-1L)
  subim=iimage[xst:xnd, yst:ynd]
  vmax=max(subim, imax)
  xpeaks[i]=(imax mod (xnd-xst+1L))+xst
  ypeaks[i]=(imax / (xnd-xst+1L))+yst
endfor
endif

splog, 'Mark stars and exit'
atv, iimage, /block

starcoord=markcoord

splog, 'Mark galaxies and exit'
atv, iimage, /block
galcoord=markcoord

xstars=transpose(starcoord[0,*])
ystars=transpose(starcoord[1,*])
xgals=transpose(galcoord[0,*])
ygals=transpose(galcoord[1,*])

;; deblend on those peaks
deblend, iimage, iivar, nchild=nchild, xcen=xcen, ycen=ycen, $
  children=children, templates=templates, xgals=xgals, ygals=ygals, $
  xstars=xstars, ystars=ystars
stop


;; 
save


end
;------------------------------------------------------------------------------
