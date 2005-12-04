;+
; NAME:
;   an_usno_read
; PURPOSE:
;   Read USNO catalog.
; COMMENTS:
;   - For now, this is a trival wrapper on IDLUTILS usno_read; that
;   might change in the future.
; INPUTS:
;   racen     - RA of region center (J2000 deg)
;   deccen    - dec of region center (J2000 deg)
;   rad       - radius of region (deg)
; OPTIONAL INPUTS:
; OUTPUTS:
;   []        - structure of USNO objects; -1 if no matches
; DEPENDENCIES:
;   idlutils
; BUGS:
; REVISION HISTORY:
;   2005-12-04  started - Hogg (NYU)
;-
function an_usno_read, racen,deccen,rad
usno= usno_read(racen,deccen,rad,catname='USNO-B1.0')
sindx= sort(usno.rmag)
return, usno[sindx]
end
