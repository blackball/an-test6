;+
; NAME:
;   dchildren
; PURPOSE:
;   deblend children of a parent
; CALLING SEQUENCE:
;   dchildren, base, iparent
; INPUTS:
;   base - FITS image base name
;   iparent - parent to process 
; OPTIONAL INPUTS:
; COMMENTS:
;   If you input 'myimage.fits' it outputs:
;    myimage-iparent-cat.fits (catalog)
;    myimage-iparent-atlas.fits (atlases)
; REVISION HISTORY:
;   11-Jan-2006  Written by Blanton, NYU
;-
;------------------------------------------------------------------------------
pro dchildren, base, iparent, plim=plim, psmooth=psmooth, $
               glim=glim, xstars=xstars, ystars=ystars, xgals=xgals, $
               ygals=ygals, psf=psf, nodeblend=nodeblend

common atv_point, markcoord

if(NOT keyword_set(plim)) then plim=10.
if(NOT keyword_set(glim)) then glim=10.
if(NOT keyword_set(psmooth)) then psmooth=40L

image=mrdfits(base+'-parents.fits',1+2*iparent)
ivar=mrdfits(base+'-parents.fits',2+2*iparent)
nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]
pnx=(size(psf,/dim))[0]
pny=(size(psf,/dim))[1]
sscale=dsigma(image,sp=10)*sqrt(median(ivar))
ivar=ivar/sscale^2

;; find all peaks 
if(NOT keyword_set(xstars)) then begin
    simage=dsmooth(image,1.)
    ssigma=dsigma(simage, sp=5)
    dpeaks, simage, xc=xc, yc=yc, sigma=ssigma, minpeak=plim*ssigma, $
      /refine, npeaks=nc, maxnpeaks=100, /check, saddle=3.
endif    

if(keyword_set(nc) gt 0 or $
   keyword_set(xstars) gt 0 or $
   keyword_set(xgals) gt 0) then begin
    
    if(keyword_set(xstars) eq 0 AND $
       keyword_set(xgals) eq 0) then begin 
;; try and guess which peaks are PSFlike
        xco=xc
        yco=yc
        ispsf=dpsfcheck(image, ivar, xc, yc, amp=amp, psf=psf)
        ipsf=where(ispsf gt 0., npsf)
        xstars=-1
        ystars=-1
        if(npsf gt 0) then begin
            xstars=xc[ipsf]
            ystars=yc[ipsf]
            ispsf=ispsf[ipsf]
            amp=amp[ipsf]
        endif
        
        model=fltarr(nx,ny)
        for i=0L, npsf-1L do $ 
          embed_stamp, model, amp[i]*psf, xstars[i]-float(pnx/2L), $
          ystars[i]-float(pny/2L)
        nimage= image-model
    endif
        
    if(keyword_set(xgals) eq 0) then begin
        ngals=0
        while(ngals eq 0 and psmooth gt 1L) do begin
            subpix=long(psmooth/3.) > 1L
            nxsub=nx/subpix
            nysub=ny/subpix
            simage=rebin(nimage[0:nxsub*subpix-1, 0:nysub*subpix-1], $
                         nxsub, nysub)
            simage=dsmooth(simage, psmooth/float(subpix))
            ssig=dsigma(simage, sp=4)
            sivar=fltarr(nxsub, nysub)+1./ssig^2
            dpeaks, simage, xc=xc, yc=yc, sigma=ssig, minpeak=glim*ssig, $
              /refine, npeaks=ngals
            
            if(ngals eq 0) then $
              psmooth=long(psmooth*0.8)>1L
        endwhile

        xgals=-1
        ygals=-1
        if(ngals gt 0) then begin
            xgals=(float(xc)+0.5)*float(subpix)
            ygals=(float(yc)+0.5)*float(subpix)

            ;; refine the centers
            for i=0L, ngals-1L do begin
                xst=long(xgals[i]-subpix*2.5) > 0L
                xnd=long(xgals[i]+subpix*2.5) < (nx-1L)
                yst=long(ygals[i]-subpix*2.5) > 0L
                ynd=long(ygals[i]+subpix*2.5) < (ny-1L)
                nxs=xnd-xst+1L
                nys=ynd-yst+1L
                subimg=nimage[xst:xnd, yst:ynd]
                subimg=dsmooth(subimg, 2)
                submax=max(subimg, imax)
                tmpx=float(imax mod nxs)
                tmpy=float(imax / nxs)
                if(tmpx gt 0 and tmpx lt nx-1. and $
                   tmpy gt 0 and tmpy lt ny-1.) then begin
                    dcen3x3, subimg[tmpx-1:tmpx+1, tmpy-1:tmpy+1], xr, yr
                    if(xr ge -0.5 and xr lt 2.5 and $
                       yr ge -0.5 and yr lt 2.5) then begin
                        tmpx=tmpx-1.+xr
                        tmpy=tmpy-1.+yr
                    endif else begin
                        tmpx=tmpx
                        tmpy=tmpy
                    endelse
                endif
                xgals[i]=float(xst)+tmpx
                ygals[i]=float(yst)+tmpy
            endfor
            
            x1=fltarr(2,n_elements(xstars))
            x1[0,*]=xstars
            x1[1,*]=ystars
            x2=fltarr(2,n_elements(xgals))
            x2[0,*]=xgals
            x2[1,*]=ygals
            matchnd, x1, x2, 10., m1=m1, m2=m2, nmatch=nm, nd=2
            if(nm gt 0) then begin
                kpsf=lonarr(n_elements(xstars))+1L 
                kpsf[m1]=0 
                ik=where(kpsf gt 0, nk) 
                if(nk gt 0) then begin
                    xstars=xstars[ik] 
                    ystars=ystars[ik] 
                endif else begin
                    xstars=-1
                    ystars=-1
                endelse
            endif
        endif
    endif
    
    npsf=n_elements(xstars)
    ngals=n_elements(xgals)
    if(1) then begin
        atv,image
        if(npsf gt 0) then $
          atvplot,xstars, ystars, psym=4, th=4, color='green'
        if(ngals gt 0) then $
          atvplot,xgals, ygals, psym=4, th=2, color='red'
    endif
    
;; deblend on those peaks
    if(xstars[0] ge 0 OR xgals[0] gt 0 and keyword_set(nodeblend) eq 0) $
      then begin
        deblend, image, ivar, nchild=nchild, xcen=xcen, ycen=ycen, $
          children=children, templates=templates, xgals=xgals, $
          ygals=ygals, xstars=xstars, ystars=ystars, psf=psf
        
        mwrfits, children[*,*,0], base+'-'+strtrim(string(iparent),2)+ $
          '-atlas.fits', hdr, /create
        for i=1L, nchild-1L do begin
            mwrfits, children[*,*,i], base+'-'+strtrim(string(iparent),2)+ $
              '-atlas.fits', hdr
        endfor
    endif 
endif

end
;------------------------------------------------------------------------------
