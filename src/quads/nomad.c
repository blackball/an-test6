#include "nomad.h"

#if __BYTE_ORDER == __BIG_ENDIAN
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

// convert a u32 from little-endian to local.
static inline uint from_le32(uint i) {
#if IS_BIG_ENDIAN
	return __bswap_32(i);
#else
	return i;
#endif
}

int nomad_parse_entry(struct nomad_entry* entry, void* encoded) {
	u32* udata = encoded;
	u32 uval;
	int32_t ival;

	ival = uval = from_le32(udata[0]);
	entry->ra = uval * 0.001 / (60.0 * 60.0);

	ival = uval = from_le32(udata[1]);
	entry->dec = -90.0 + uval * 0.001 / (60.0 * 60.0);

	ival = uval = from_le32(udata[2]);
	entry->sigma_racosdec = uval * 0.001;

	ival = uval = from_le32(udata[3]);
	entry->sigma_dec = uval * 0.001;

	ival = uval = from_le32(udata[4]);
	entry->mu_racosdec = ival * 0.0001;

	ival = uval = from_le32(udata[5]);
	entry->mu_dec = ival * 0.0001;

	ival = uval = from_le32(udata[6]);
	entry->sigma_mu_racosdec = uval * 0.0001;

	ival = uval = from_le32(udata[7]);
	entry->sigma_mu_dec = uval * 0.0001;

	ival = uval = from_le32(udata[8]);
	entry->epoch_ra = uval * 0.001;

	ival = uval = from_le32(udata[9]);
	entry->epoch_dec = uval * 0.001;

	ival = uval = from_le32(udata[10]);
	entry->mag_B = ival * 0.001;

	ival = uval = from_le32(udata[11]);
	entry->mag_V = ival * 0.001;

	ival = uval = from_le32(udata[12]);
	entry->mag_R = ival * 0.001;

	ival = uval = from_le32(udata[13]);
	entry->mag_J = ival * 0.001;

	ival = uval = from_le32(udata[14]);
	entry->mag_H = ival * 0.001;

	ival = uval = from_le32(udata[15]);
	entry->mag_K = ival * 0.001;

	ival = uval = from_le32(udata[16]);
	entry->usnob_id = uval;

	ival = uval = from_le32(udata[17]);
	entry->twomass_id = uval;

	ival = uval = from_le32(udata[18]);
	entry->yb6_id = uval;

	ival = uval = from_le32(udata[19]);
	entry->ucac2_id = uval;

	ival = uval = from_le32(udata[20]);
	entry->tycho2_id = uval;

	ival = uval = from_le32(udata[21]);
	entry->astrometry_src = (uval >> 0) & 0x7;
	entry->blue_src       = (uval >> 3) & 0x7;
	entry->visual_src     = (uval >> 6) & 0x7;
	entry->red_src        = (uval >> 9) & 0x7;

	entry->usnob_fail       = (uval >> 12) & 0x1;
	entry->twomass_fail     = (uval >> 13) & 0x1;

	entry->tycho_astrometry = (uval >> 16) & 0x1;
	entry->alt_radec        = (uval >> 17) & 0x1;
	entry->alt_2mass        = (uval >> 18) & 0x1;
	entry->alt_ucac         = (uval >> 19) & 0x1;
	entry->alt_tycho        = (uval >> 20) & 0x1;
	entry->blue_o           = (uval >> 21) & 0x1;
	entry->red_e            = (uval >> 22) & 0x1;
	entry->twomass_only     = (uval >> 23) & 0x1;
	entry->hipp_astrometry  = (uval >> 24) & 0x1;
	entry->diffraction      = (uval >> 25) & 0x1;
	entry->confusion        = (uval >> 26) & 0x1;
	entry->bright_confusion = (uval >> 27) & 0x1;
	entry->bright_artifact  = (uval >> 28) & 0x1;
	entry->standard         = (uval >> 29) & 0x1;
	entry->external         = (uval >> 30) & 0x1;

	return 0;
}
