#ifndef SIP_QFITS_H
#define SIP_QFITS_H

#include "qfits.h"
#include "sip.h"

qfits_header* sip_create_header(sip_t* sip);

qfits_header* tan_create_header(tan_t* tan);

void sip_add_to_header(qfits_header* hdr, sip_t* sip);

void tan_add_to_header(qfits_header* hdr, tan_t* tan);

sip_t* sip_read_header(qfits_header* hdr, sip_t* dest);

tan_t* tan_read_header(qfits_header* hdr, tan_t* dest);

#endif
