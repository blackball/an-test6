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

psmooth=30.

nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]

;; do general object detection
sigma=dsigma(image)
invvar=fltarr(nx,ny)+1./sigma^2
dobjects, image, invvar, object=oimage, plim=25., dpsf=1.

;; now choose the object that includes the center
iobj=where(oimage eq oimage[nx/2L, ny/2L] OR oimage eq -1L)
iimage=randomn(seed, nx, ny)*sigma
iimage[iobj]=image[iobj]
iivar=fltarr(nx,ny)+1./sigma^2

;; smooth, resample, and find peaks at a reasonable scale 
subpix=long(psmooth/3.) > 1L
nxsub=nx/subpix
nysub=ny/subpix
simage=rebin(iimage[0:nxsub*subpix-1, 0:nysub*subpix-1], nxsub, nysub)
simage=dsmooth(simage, psmooth/float(subpix))

ssig=dsigma(simage)
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

;; deblend on those peaks
deblend, iimage, iivar, nchild=nchild, xcen=xcen, ycen=ycen, $
  children=children, templates=templates, xpeaks=xpeaks, ypeaks=ypeaks

;; 
save


end
;------------------------------------------------------------------------------
