;+
; NAME:
;   hogg_astrom_tweak
;
; PURPOSE:
;   Tweak GSSS astrometric solution, given a good initial guess
;
; CALLING SEQUENCE:
;   gsa_out = hogg_astrom_tweak( gsa_in, catra, catdec, imx, imy, $
;    [ dtheta=, errflag=, nmatch=, catind=, obsind=, /verbose, etc ] )
;
; INPUTS:
;   gsa_in        - Initial guess for astrometric solution (struct)
;   catra,catdec  - catalogue positions
;   imx,imy       - image star positions
;
; OPTIONAL KEYWORDS:
;   dtheta     - Match distance between catalog and image stars (deg);
;                default to 5/3600.
;   order      - default to 1; maximum 3.
;   verbose    - If set, then print sizes of offsets
;  
; OUTPUTS:
;   gsa_out    - returned guess for astrometric solution (struct);
;                0 if solution failed
;
; OUTPUT OUTPUTS:
;   errflag    - Set to 1 if fatal error occurs, 0 otherwise
;   nmatch     - Number of matched stars
;   catind     - Indices of CATLON,CATLAT for matched objects.
;   obsind     - Indices of XPOS,YPOS for matched objects.
;   sigmax     - sigma of matched stars in delta-x
;   sigmay     - sigma of matched stars in delta-y
;
; COMMENTS:
;   Uses preliminary solution given in astr structure to match image
;   and catalogue stars within maxsep pixels of each other.  These
;   are then used by astrom_warp to determine a new solution, returned
;   in astr.
; 
; BUGS:
;   - Duplicates code in gsssadxy and gsssxyad.
;
; PROCEDURES CALLED:
;   djs_angle_match()
;   gsssadxy
;   gsssxyad
;
; REVISION HISTORY:
;   02-Feb-2003  Written by D. Schlegel and D. Hogg, APO
;   2005-04-07   replace polywarp and added order parameter - Hogg (NYU)
;-
;-----------------------------------------------------------------------------
function hogg_astrom_tweak, gsa_in, catra, catdec, imx, imy, dtheta=dtheta, $
 errflag=errflag, nmatch=nmatch, catind=catind, obsind=obsind, order=order, $
 verbose=verbose, sigmax=sigmax, sigmay=sigmay

   errflag = 0
   if (NOT keyword_set(dtheta)) then dtheta = 5./3600.
   if (n_elements(order) EQ 0) then order = 1
   if (order GT 3) then begin
       splog, 'WARNING: order > 3; there is only one term at order > 3, and it is a fifth-order radial term'
   endif
   if (order EQ 4) then begin
       splog, 'WARNING: in the future, set order to 3 or 5, but never 4'
   endif
   if (order GT 5) then begin
       splog, 'WARNING: order > 5; setting order to 5'
       order= 5
   endif
   if (order LT 1) then begin
       splog, 'WARNING: order < 1; setting order to 1'
       order= 1
   endif

   ;----------
   ; Find matches between catalog (RA,DEC) and image X,Y

   gsssxyad, gsa_in, imx, imy, imra, imdec
   nmatch = djs_angle_match(imra, imdec, catra, catdec, dtheta=dtheta, $
    mcount=mcount, mindx=mindx)
   if (nmatch LT 5) then begin
      splog, 'Too few matches; returning original GSA'
      errflag = 1
      catind = -1L
      obsind = -1L
      return, gsa_in
   endif
   obsind = where(mcount GT 0)
   catind = mindx[obsind]

   ;----------
   ; Compute GSA internal coordinate system chi,eta,obx,oby
   ; This code is replicated from GSSSADXY

   ; This DUPLICATES CODE in gsssxyad.pro
   radeg = 180.0d/!DPI
   arcsec_per_radian= 3600.0d*radeg
   dec_rad = catdec[catind] / radeg
   ra_rad = catra[catind] / radeg
   pltra = gsa_in.crval[0] / radeg
   pltdec = gsa_in.crval[1] / radeg

   ; This DUPLICATES CODE in gsssxyad.pro
   cosd = cos(dec_rad)
   sind = sin(dec_rad)
   ra_dif = ra_rad - pltra

   ; This DUPLICATES CODE in gsssxyad.pro
   div = ( sind*sin(pltdec) + cosd*cos(pltdec) * cos(ra_dif))
   xi = cosd*sin(ra_dif) * arcsec_per_radian / div
   eta = (sind*cos(pltdec) - cosd*sin(pltdec) * cos(ra_dif)) $
    * arcsec_per_radian / div

   ; This DUPLICATES CODE in gsssxyad.pro
   obx = ( gsa_in.ppo3 - (gsa_in.xll + imx[obsind] + 0.5d) * gsa_in.xsz) / 1000.d
   oby = (-gsa_in.ppo6 + (gsa_in.yll + imy[obsind] + 0.5d) * gsa_in.ysz) / 1000.d

   ;----------
   ; Compute transformation between obx,oby <-> imx,imy
   ; first set-up parameters for fit
   ; This DUPLICATES CODE in gsssxyad.pro

   if (order GE 1) then fitindx= [0,1,2]
   if (order GE 2) then fitindx= [fitindx,3,4,5]
   if (order GE 3) then fitindx= [fitindx,7,8,9,10]
   if (order GE 5) then fitindx= [fitindx,12]

   AA= dblarr(13,n_elements(obx))
   AA[0,*]= obx
   AA[1,*]= oby
   AA[2,*]= 1.D0
   AA[3,*]= obx^2
   AA[4,*]= obx*oby
   AA[5,*]= oby^2
   AA[6,*]= (obx^2+oby^2)
   AA[7,*]= obx^3
   AA[8,*]= obx^2*oby
   AA[9,*]= obx*oby^2
   AA[10,*]= oby^3
   AA[11,*]= obx*(obx^2+oby^2)
   AA[12,*]= obx*(obx^2+oby^2)^2 ; 5th order!

   ; perform fit
   gsa_out = gsa_in
   gsa_out.amdx[*]= 0.
   ATA= transpose(AA[fitindx,*])##AA[fitindx,*]
   gsa_out.amdx[fitindx]= invert(ATA)#(AA[fitindx,*]#xi)

   AA= dblarr(13,n_elements(obx))
   AA[0,*]= oby
   AA[1,*]= obx
   AA[2,*]= 1.D0
   AA[3,*]= oby^2
   AA[4,*]= oby*obx
   AA[5,*]= obx^2
   AA[6,*]= (obx^2+oby^2)
   AA[7,*]= oby^3
   AA[8,*]= oby^2*obx
   AA[9,*]= oby*obx^2
   AA[10,*]= obx^3
   AA[11,*]= oby*(obx^2+oby^2)
   AA[12,*]= oby*(obx^2+oby^2)^2 ; 5th order!

   ; perform fit
   gsa_out.amdy[*]= 0.
   ATA= transpose(AA[fitindx,*])##AA[fitindx,*]
   gsa_out.amdy[fitindx]= invert(ATA)#(AA[fitindx,*]#eta)

   ;----------
   ; Report the RMS differences between catalog and CCD positions

   gsssadxy, gsa_out, catra[catind], catdec[catind], catx, caty
   xdiff = imx[obsind] - catx
   ydiff = imy[obsind] - caty
   sigmax= sqrt(mean(xdiff^2))
   sigmay= sqrt(mean(ydiff^2))

   if (keyword_set(verbose)) then begin
      gsssadxy, gsa_in, catra[catind], catdec[catind], catx, caty
      oxdiff = imx[obsind] - catx
      oydiff = imy[obsind] - caty
      splog, 'Input mean/rms offset in X = ',mean(oxdiff),sqrt(mean(oxdiff^2))
      splog, 'Input mean/rms offset in Y = ',mean(oydiff),sqrt(mean(oydiff^2))

      splog, 'Output mean/rms offset in X = ',mean(xdiff), sigmax
      splog, 'Output mean/rms offset in Y = ',mean(ydiff), sigmay
   endif

   return, gsa_out
end 
;-----------------------------------------------------------------------------
