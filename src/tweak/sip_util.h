#ifndef SIP_UTIL_H

#include "libwcs/fitsfile.h"
#include "libwcs/wcs.h"
#include "sip.h"
#include "fitsio.h"

typedef struct WorldCoor wcs_t;
void copy_wcs_into_sip(wcs_t* wcs, sip_t* sip);
wcs_t* get_wcs_from_fits_header(char* buf);
wcs_t* load_wcs_from_fitsio(fitsfile* fptr);
sip_t* load_sip_from_fitsio(fitsfile* fptr);
void print_sip(sip_t*);

#endif //SIP_UTIL_H
