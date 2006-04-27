#include <endian.h>
#include <netinet/in.h>
#include <byteswap.h>
#include <assert.h>
#include <stdio.h>

#if __BYTE_ORDER == __BIG_ENDIAN
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

#include "usnob.h"

// convert a u32 from little-endian to local.
static inline uint from_le32(uint i) {
#if IS_BIG_ENDIAN
	return __bswap_32(i);
#else
	return i;
#endif
}

int usnob_parse_entry(unsigned char* line, usnob_entry* usnob) {
	uint ival, obs;
	uint A, S, P, i, j, M, R, Q, y, x, k, e, v, u;

	// bytes 0-3: uint, RA in units of 0.01 arcsec.
	ival = from_le32(*(uint*)line);
	//assert(ival < (100*60*60*360));
	if (ival > (100*60*60*360)) {
		fprintf(stderr, "USNOB: RA should be in [0, %u), but got %u.\n",
				100*60*60*360, ival);
		return -1;
	}
	usnob->ra = (double)ival / (100.0 * 60.0 * 60.0);

	// bytes 4-7: uint, SPD (south polar distance) in units of 0.01 arcsec.
	ival = from_le32(*(uint*)(line + 4));
	assert(ival <= (100*60*60*180));
	// DEC = south polar distance - 90 degrees
	usnob->dec = (double)ival / (100.0 * 60.0 * 60.0) - 90.0;

	// bytes 8-11: uint, packed in base-10:
	//    iPSSSSAAAA
	ival = from_le32(*(uint*)(line + 8));
	A = (ival % 10000);
	ival /= 10000;
	S = (ival % 10000);
	ival /= 10000;
	P = (ival % 10);
	ival /= 10;
	i = (ival % 10);

	// A: mu_RA, in units of 0.002 arcsec per year, offset by
	//    -10 arcsec per year.
	usnob->mra = -10.0 + (0.002 * A);

	// S: mu_SPD, in units of 0.002 arcsec per year, offset by
	//    -10 arcsec per year.
	// I assume this is a derivative so is equal to mu_DEC.
	usnob->mdec = -10.0 + (0.002 * S);

	// P: total mu probability, in units of 0.1.
	usnob->mprob = 0.1 * P;

	// i: motion catalog flag: 0=no, 1=yes.
	assert((i == 0) || (i == 1));
	usnob->motion_catalog = i;


	// bytes 12-15: uint, packed in base-10:
	//     jMRQyyyxxx
	ival = from_le32(*((uint*)(line + 12)));
	x = (ival % 1000);
	ival /= 1000;
	y = (ival % 1000);
	ival /= 1000;
	Q = (ival % 10);
	ival /= 10;
	R = (ival % 10);
	ival /= 10;
	M = (ival % 10);
	ival /= 10;
	j = (ival % 10);

	/*
	  printf("bytes 12-15: 0x%02x%02x%02x%02x "
	  "%10u : %01u%01u%01u%01u%03u%03u, M=%u\n",
	  line[12], line[13], line[14], line[15],
	  from_le32(*(uint*)(line + 12)), j, M, R, Q, y, x, M);
	*/

	// x: sigma_mu_RA, in units of 0.001 arcsec per year.
	usnob->sig_mra = 0.001 * x;

	// y: sigma_mu_SPD, in units of 0.001 arcsec per year.
	// I assume this is equal to sigma_mu_DEC.
	usnob->sig_mdec = 0.001 * y;

	// Q: sigma_RA_fit, in units of 0.1 arcsec.
	usnob->sig_mra_fit = 0.1 * Q;

	// R: sigma_SPD_fit, in units of 0.1 arcsec.
	usnob->sig_mdec_fit = 0.1 * R;

	// M: number of detections; in [2, 5].
	//assert(M >= 2);
	//assert(M <= 5);
	/*
	  if ((M < 2) || (M > 5)) {
	  fprintf(stderr, "USNOB: M (number of detections) should be between 2 and 5, but got %u\n", M);
	  return -1;
	  }
	*/
	usnob->ndetections = M;

	// j: diffraction spike flag: 0=no, 1=yes.
	assert((j == 0) || (j == 1));
	usnob->diffraction_spike = j;


	// bytes 16-19: uint, packed in base-10:
	//     keeevvvuuu
	ival = from_le32(*(uint*)(line + 16));
	u = (ival % 1000);
	ival /= 1000;
	v = (ival % 1000);
	ival /= 1000;
	e = (ival % 1000);
	ival /= 1000;
	k = (ival % 10);

	// u: sigma_RA, in units of 0.001 arcsec.
	usnob->sig_ra = 0.001 * u;

	// v: sigma_SPD, in units of 0.001 arcsec.
	// I assume this is equal to sigma_DEC.
	usnob->sig_dec = 0.001 * v;

	// e: mean epoch, in 0.1 yr, offset by -1950.
	usnob->epoch = 1950.0 + 0.1 * e;

	// k: YS4.0 correlation flag: 0=no, 1=yes.
	usnob->ys4 = k;


	for (obs=0; obs<5; obs++) {
		uint G, S, F, m, C, r, R;

		// bytes 20-23, 24-27, ...: uint, packed in base-10:
		//     GGSFFFmmmm
		ival = from_le32(*(uint*)(line + 20 + obs * 4));
		m = (ival % 10000);
		ival /= 10000;
		F = (ival % 1000);
		ival /= 1000;
		S = (ival % 10);
		ival /= 10;
		G = (ival % 100);

		/*
		  printf("bytes %i-%i: 0x%02x%02x%02x%02x : %10u : %02u%01u%03u%04u, G=%u.  M=%u.\n",
		  20+obs*4, 20+obs*4+3,
		  line[20 + obs*4],
		  line[20 + obs*4+1],
		  line[20 + obs*4+2],
		  line[20 + obs*4+3],
		  from_le32(*(uint*)(line+20+obs*4)), G, S, F, m, G, M);
		*/

		// m: magnitude, in units of 0.01 mag.
		usnob->obs[obs].mag = 0.01 * m;

		// F: field number in the original survey; 1-937.
		//assert(F >= 1);
		assert(F <= 937);
		usnob->obs[obs].field = F;

		// S: survey number of original survey.
		usnob->obs[obs].survey = S;

		// G: star-galaxy estimate.  0=galaxy, 11=star.
		//assert(G <= 11);
		if ((G > 11) && (G != 19)) {
			fprintf(stderr, "USNOB: star/galaxy estimate should be in {[0, 11], 19}, but found %u.\n", G);
		}
		usnob->obs[obs].star_galaxy = G;


		// bytes 40-43, 44-47, ...: uint, packed in base-10:
		//     CrrrrRRRR
		ival = from_le32(*(uint*)(line + 40 + obs * 4));
		R = (ival % 10000);
		ival /= 10000;
		r = (ival % 10000);
		ival /= 10000;
		C = (ival % 10);

		// R: xi residual, in units of 0.01 arcsec, offset by -50 arcsec.
		usnob->obs[obs].xi_resid = -50.0 + 0.01 * R;

		// r: eta residual, in units of 0.01 arcesc, offset by -50.
		usnob->obs[obs].eta_resid = -50.0 + 0.01 * r;

		// C: source of photometric calibration.
		usnob->obs[obs].calibration = C;


		// bytes 60-63, 64-67, ...: uint
		// index into PMM scan file.
		ival = from_le32(*(uint*)(line + 60 + obs * 4));
		assert(ival <= 9999999);
		usnob->obs[obs].pmmscan = ival;
	}

	return 0;
}


