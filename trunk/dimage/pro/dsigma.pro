;+
; NAME:
;   dsigma
; PURPOSE:
;   find approximate sigma
; CALLING SEQUENCE:
;   sigma= dsigma(image)
; INPUTS:
;   image - [nx, ny] input image
; OUTPUTS:
;   sigma - estimated sigma
; COMMENTS:
;   Takes a bunch of differences between nearby pixels sparsely
;   sampled across the image.
; REVISION HISTORY:
;   2-Mar-2006  Written by Blanton, NYU (Fink's method)
;-
;------------------------------------------------------------------------------
function dsigma, image

nx=(size(image,/dim))[0]
ny=(size(image,/dim))[1]

; Set source object name
soname=filepath('libdimage.'+idlutils_so_ext(), $
                root_dir=getenv('DIMAGE_DIR'), subdirectory='lib')

sigma=0.
retval=call_external(soname, 'idl_dsigma', float(image), $
                     long(nx), long(ny), float(sigma))

return, sigma

end
;------------------------------------------------------------------------------
