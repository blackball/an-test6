;+
; NAME:
;   an_sdss_read
; PURPOSE:
;   A swap-in replacement for an_usno_read, but reading SDSS catalog
;     files, not USNO-B1.0 catalog files.
; INPUTS:
;   racen     - RA of region center (J2000 deg)
;   deccen    - dec of region center (J2000 deg)
;   rad       - radius of region (deg)
; OPTIONAL INPUTS:
;   rerun     - SDSS rerun (default 137)
; OUTPUTS:
;   []        - structure of SDSS objects; -1 if no matches
; DEPENDENCIES:
;   idlutils
;   photoop
; BUGS:
;   - Not written.
;   - Not tested.
;   - Depends on photoop.
; REVISION HISTORY:
;   2005-12-04  started - Hogg (NYU)
;-
function an_sdss_read, racen,deccen,rad,rerun=rerun
if (NOT keyword_set(rerun)) then rerun= 137
fields= SDSS_ASTR2FIELDS(radeg=racen,decdeg=deccen,radius=rad,rerun=rerun)
if (n_tags(fields) GT 1) then begin
    obj= SDSS_READOBJ(fields.run,fields.camcol,fields.field,rerun=rerun)
    spherematch, racen,deccen,obj.ra,obj.dec,rad, $
      match1,match2,maxmatch=0
    if (match2[0] NE -1) then begin
        obj= obj[match2]
        sindx= sort(obj.petroflux)
        obj= obj[sindx]
        return, obj
    endif
endif
splog, 'ERROR: no matches'
return, -1
end
