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
               ygals=ygals, psf=psf, nodeblend=nodeblend, hand=hand

common atv_point, markcoord

if(NOT keyword_set(plim)) then plim=5.
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

fit_mult_gauss, psf, 1, amp, psfsig, model=model
xx=reform(replicate(1., pnx)#findgen(pny), pnx*pny)-float(pnx/2L)
yy=reform(findgen(pnx)#replicate(1., pny), pnx*pny)-float(pny/2L)
rr=sqrt(xx^2+yy^2)
ii=where(rr gt 15.*psfsig[0], nii)
if(nii gt 0) then psf[ii]=psf[ii]<(max(psf)*exp(-0.5*(rr/psfsig[0])^2))

;; find all peaks 
if(NOT keyword_set(xstars)) then begin
    simage=dsmooth(image,1.)
    ssigma=dsigma(simage, sp=5)
    dpeaks, simage, xc=xc, yc=yc, sigma=ssigma, minpeak=plim*ssigma, $
      /refine, npeaks=nc, maxnpeaks=1000, /check, saddle=3.
endif    

if(keyword_set(nc) gt 0 or $
   keyword_set(xstars) gt 0 or $
   keyword_set(xgals) gt 0 or $
   keyword_set(hand) gt 0) then begin
    
    if(keyword_set(xstars) eq 0 AND $
       keyword_set(xgals) eq 0) then begin 
;; try and guess which peaks are PSFlike
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
          embed_stamp, model, amp[i]*psf/max(psf), xstars[i]-float(pnx/2L), $
          ystars[i]-float(pny/2L)
        nimage= image-model
    endif
        
    if(keyword_set(xgals) eq 0) then begin
        ngals=0
        psmooth=5L
        while(ngals eq 0 and psmooth gt 1L) do begin
            subpix=long(psmooth/3.) > 1L
            nxsub=nx/subpix
            nysub=ny/subpix
            simage=rebin(nimage[0:nxsub*subpix-1, 0:nysub*subpix-1], $
                         nxsub, nysub)
            simage=dsmooth(simage, psmooth/float(subpix))
            ssig=dsigma(simage, sp=10)
            sivar=fltarr(nxsub, nysub)+1./ssig^2
            dpeaks, simage, xc=xc, yc=yc, sigma=ssig, minpeak=glim*ssig, $
              /refine, npeaks=ngals, saddle=100., /check
            
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
    if(keyword_set(hand)) then begin

        notdone=1
        while(notdone) do begin
            mark=0
            if(npsf gt 0) then $
              mark=transpose(reform([xstars, ystars], n_elements(xstars),2))
            splog, 'Hand mark stars in the image!'
            splog, '(d: shows marks, m: makes mark, k: kills mark)'
            atv,nimage, /block, mark= mark
            
            ;; refine stellar peaks 
            if(keyword_set(mark)) then begin
                xstars=fltarr(n_elements(mark)/2L)
                ystars=fltarr(n_elements(mark)/2L)
                for i=0L, n_elements(mark)/2L-1L do begin
                    tmpx=mark[0,i]
                    tmpy=mark[1,i]
                    xst=long(mark[0,i]-3) > 0L
                    xnd=long(mark[0,i]+3) < (nx-1L)
                    yst=long(mark[1,i]-3) > 0L
                    ynd=long(mark[1,i]+3) < (ny-1L)
                    nxs=xnd-xst+1L
                    nys=ynd-yst+1L
                    subimg=nimage[xst:xnd, yst:ynd]
                    subimg=dsmooth(subimg, 1)
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
                    xstars[i]=float(xst)+tmpx
                    ystars[i]=float(yst)+tmpy
                endfor
            endif else begin
                xstars=-1
                ystars=-1
            endelse
            
            mark=0
            if(ngals gt 0) then $
              mark=transpose(reform([xgals, ygals], n_elements(xgals),2))
            splog, 'Hand mark galaxies in the image!'
            splog, '(d: shows marks, m: makes mark, k: kills mark)'
            atv,nimage, /block, mark= mark
            
            ;; refine stellar peaks 
            if(keyword_set(mark)) then begin
                xgals=fltarr(n_elements(mark)/2L)
                ygals=fltarr(n_elements(mark)/2L)
                xgals[*]=mark[0,*]
                ygals[*]=mark[1,*]
            endif else begin
                xgals=-1
                ygals=-1
            endelse
            
            if(0) then begin
                if(keyword_set(mark)) then begin
                    xgals=fltarr(n_elements(mark)/2L)
                    ygals=fltarr(n_elements(mark)/2L)
                    for i=0L, n_elements(mark)/2L-1L do begin
                    tmpx=mark[0,i]
                    tmpy=mark[1,i]
                    xst=long(mark[0,i]-3) > 0L
                    xnd=long(mark[0,i]+3) < (nx-1L)
                    yst=long(mark[1,i]-3) > 0L
                    ynd=long(mark[1,i]+3) < (ny-1L)
                    nxs=xnd-xst+1L
                    nys=ynd-yst+1L
                    subimg=nimage[xst:xnd, yst:ynd]
                    subimg=dsmooth(subimg, 1)
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
            endif else begin
                xgals=-1
                ygals=-1
            endelse
        endif

            print, 'Are you done? (y/[n])'
            donestr=''
            read, donestr
            if(donestr eq 'y') then $
              notdone=0
        endwhile
            
    endif
    
;; deblend on those peaks
    if(xstars[0] ge 0 OR xgals[0] gt 0 and keyword_set(nodeblend) eq 0) $
      then begin
        psfd=psf
        if(xstars[0] ge 0) then begin
            psfd=fltarr(pnx, pny, n_elements(xstars))
            for i=0L, n_elements(xstars)-1L do begin
                psfd[*,*,i]=sshift2d(psf, [xstars[i]-long(xstars[i]), $
                                           ystars[i]-long(ystars[i])])
            endfor
        endif
        dtemplates, nimage, xgals, ygals, templates=templates, /sersic
        deblend, nimage, ivar, nchild=nchild, xcen=xcen, ycen=ycen, $
          children=children, templates=templates, xgals=xgals, $
          ygals=ygals, xstars=-1L, ystars=-1L, psf=psfd
        
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
