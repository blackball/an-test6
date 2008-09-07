/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2008 Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <math.h>
#include <string.h>
#include <assert.h>

#include "an-catalog.h"
#include "usnob.h"
#include "catalog.h"
#include "healpix.h"
#include "starutil.h"
#include "mathutil.h"
#include "fitsioutils.h"
#include "ioutils.h"
#include "boilerplate.h"
#include "errors.h"
#include "log.h"

#define OPTIONS "ho:N:n:m:M:H:d:ARBJb:gj:ps:IE"

static void print_help(char* progname) {
	boilerplate_help_header(stdout);
    printf("\nUsage: %s\n"
		   "   -o <output-objs-template>    (eg, an-sdss-%%02i.objs.fits)\n"
		   "  [-g]: include magnitudes in the output file\n"
		   "  [-E]: include magnitude errors in the output file\n"
           "  [-p]: include proper motions in the output file\n"
           "  [-I]: include star IDs in the output file\n"
		   "  [-H <big healpix>]  or  [-A] (all-sky)\n"
           "  [-s <big healpix Nside>]\n"
		   "  [-n <max-stars-per-fine-healpix-grid>]    (ie, number of sweeps)\n"
		   "  [-N <nside>]:   fine healpixelization grid; default 100.\n"
		   "  [-d <dedup-radius>]: deduplication radius (arcseconds)\n"
		   "  ( [-R] or [-B] or [-J] )\n"
           "     Cut based on:\n"
		   "        R: USNOB red bands, Tycho-2 V, H\n"
		   "        B: USNOB blue bands, Tycho-2 V, B, H\n"
		   "        J: 2MASS J band\n"
		   "  [-j <jitter-in-arcseconds>]: set index jitter size (default: 1 arcsec)\n"
		   "  [-b <boundary-size-in-healpixels>] (default 1) number of healpixes to add around the margins\n"
		   "  [-m <minimum-magnitude-to-use>]\n"
		   "  [-M <maximum-magnitude-to-use>]\n"
		   "  [-S <max-stars-per-(big)-healpix]         (ie, max number of stars in the cut)\n"
		   "  <input-file> [<input-file> ...]\n"
		   "\n"
		   "Input files must be Astrometry.net catalogs OR objs.fits catalogs with magnitude information.\n"
		   "If no ID filename template is given, ID files will not be written.\n"
		   "\n", progname);
}

static void add_to_header(qfits_header* hdr, char** args, int argc,
                          bool allsky, double jitter, int sweeps) {
    int k;
    if (allsky)
        qfits_header_add(hdr, "ALLSKY", "T", "All-sky catalog.", NULL);

    boilerplate_add_fits_headers(hdr);
    qfits_header_add(hdr, "HISTORY", "This file was generated by the program \"cut-an\".", NULL, NULL);
    qfits_header_add(hdr, "HISTORY", "cut-an command line:", NULL, NULL);
    fits_add_args(hdr, args, argc);
    qfits_header_add(hdr, "HISTORY", "(end of cut-an command line)", NULL, NULL);

    fits_header_add_double(hdr, "JITTER", jitter, "Positional error [arcsec]");

	// add placeholders...
	for (k=0; k< (sweeps ? sweeps : 100); k++) {
		char key[64];
		sprintf(key, "SWEEP%i", (k+1));
        qfits_header_add(hdr, key, "-1", "placeholder", NULL);
	}
}

extern char *optarg;
extern int optind, opterr, optopt;

struct stardata {
	double ra;
	double dec;
	uint64_t id;
	float mag;
	float mag_err;

    float sigma_ra;
    float sigma_dec;
    float motion_ra;
    float motion_dec;
    float sigma_motion_ra;
    float sigma_motion_dec;
};
typedef struct stardata stardata;

// globals used by get_magnitude().
static bool cutred = FALSE;
static bool cutblue = FALSE;
static bool cutj = FALSE;


static int sort_stardata_mag(const void* v1, const void* v2) {
	const stardata* d1 = v1;
	const stardata* d2 = v2;
	float diff = d1->mag - d2->mag;
	if (diff < 0.0) return -1;
	if (diff == 0.0) return 0;
	return 1;
}

static bool find_duplicate(stardata* sd, int hp, int Nside,
						   bl** starlists, double dedupr2,
						   int* duphp, int* dupindex) {
	double xyz[3];
	int neigh[8];
	int nn;
	double xyz2[3];
	int j, k;
	radecdeg2xyzarr(sd->ra, sd->dec, xyz);
	// Check this healpix...
	for (j=0; j<bl_size(starlists[hp]); j++) {
		stardata* sd2 = bl_access(starlists[hp], j);
		radecdeg2xyzarr(sd2->ra, sd2->dec, xyz2);
		if (!distsq_exceeds(xyz, xyz2, 3, dedupr2)) {
			if (duphp)
				*duphp = hp;
			if (dupindex)
				*dupindex = j;
			return TRUE;
		}
	}
	// Check neighbouring healpixes...
	nn = healpix_get_neighbours(hp, neigh, Nside);
	for (k=0; k<nn; k++) {
		int nhp = neigh[k];
		if (!starlists[nhp])
			continue;
		for (j=0; j<bl_size(starlists[nhp]); j++) {
			stardata* sd2 = bl_access(starlists[nhp], j);
			radecdeg2xyzarr(sd2->ra, sd2->dec, xyz2);
			if (!distsq_exceeds(xyz, xyz2, 3, dedupr2)) {
				if (duphp)
					*duphp = nhp;
				if (dupindex)
					*dupindex = j;
				return TRUE;
			}
		}
	}
	return FALSE;
}

static int get_magnitude(an_entry* an,
						 float* p_mag,
                         float* p_sig) {
	float redmag = 0.0;
    float redsig = 0.0;
	float bluemag = 0.0;
    float bluesig = 0.0;
	int j;
	float jmag = 1e6;
    float jsig = 1e6;

	float mag = 1e6;
    float sig = 1e6;

    bool blue, red, jband;

    float s, m;
    // accumulators.
    float rsigacc = 0.0;
    float rmagacc = 0.0;
    float bsigacc = 0.0;
    float bmagacc = 0.0;
    float jsigacc = 0.0;
    float jmagacc = 0.0;

    // Assume the mags are Gaussian distributed and return the maximum-likelihood
    // mean and sigma of the product of the individual distributions.

    // sigacc = 1/s^2 = 1/s_1^2 + 1/s_2^2 + ...
    // magacc = mu / s^2 = mu_1 / s_1^2 + mu_2 / s_2^2 + ...


    for (j=0; j<an->nobs; j++) {
        unsigned char band = an->obs[j].band;
        uint8_t cat = an->obs[j].catalog;
        blue = red = jband = FALSE;
        s = an->obs[j].sigma_mag;
        m = an->obs[j].mag;
        if (cat == AN_SOURCE_USNOB) {
            red = usnob_is_band_red(band);
            blue = usnob_is_band_blue(band);
        } else if (cat == AN_SOURCE_TYCHO2) {
            // H band covers V and B.
            // B is blue
            // V is red.
            red = (band == 'H' || band == 'V');
            blue = (band == 'H' || band == 'B');
        } else if (cat == AN_SOURCE_2MASS && band == 'J') {
            jband = TRUE;
        }
        if (red) {
            rsigacc += 1.0 / (s * s);
            rmagacc += m   / (s * s);
        }
        if (blue) {
            bsigacc += 1.0 / (s * s);
            bmagacc += m   / (s * s);
        }
        if (jband) {
            jsigacc += 1.0 / (s * s);
            jmagacc += m   / (s * s);
        }
    }

    blue = red = jband = FALSE;
    if (rmagacc > 0) {
        red = TRUE;
        redmag = rmagacc / rsigacc;
        redsig = sqrt(1.0 / rsigacc);
    }
    if (bmagacc > 0) {
        blue = TRUE;
        bluemag = bmagacc / bsigacc;
        bluesig = sqrt(1.0 / bsigacc);
    }
    if (jmagacc > 0) {
        jband = TRUE;
        jmag = jmagacc / jsigacc;
        jsig = sqrt(1.0 / jsigacc);
    }

    if (cutred) {
        if (!red) {
            return -1;
        } else {
            mag = redmag;
            sig = redsig;
        }
    }
    if (cutblue) {
        if (!blue) {
            return -1;
        } else {
            mag = bluemag;
            sig = bluesig;
            // We tried this at one point but it didn't work too well...
            //mag += epsilon * (bluemag - redmag);
        }
    }
    if (cutj) {
        if (!jband) {
            return -1;
        } else {
            mag = jmag;
            sig = jsig;
        }
    }
    *p_mag = mag;
    *p_sig = sig;
    return 0;
}

// Used for debugging / drawing nice pictures.
#include "rdlist.h"
void write_radeclist(bool* owned, int Nside, char* fn) {
	int HP = 12 * Nside * Nside;
    int i;
    rdlist_t* rd = rdlist_open_for_writing(fn);
    rdlist_write_primary_header(rd);
    rdlist_write_header(rd);
    for (i=0; i<HP; i++) {
        double ra, dec;
        if (!owned[i])
            continue;
        healpix_to_radecdeg(i, Nside, 0.5, 0.5, &ra, &dec);
        rdlist_write_one_radec(rd, ra, dec);
    }
    rdlist_fix_header(rd);
    rdlist_fix_primary_header(rd);
}

int main(int argc, char** args) {
	char* outfn = NULL;
	int c;
	int startoptind;
	int i, k, HP;
	int Nside = 100;
	bl** starlists;
	int sweeps = 0;
	int nkeep;
	double minmag = -1.0;
	double maxmag = 30.0;
	bool* owned;
	int maxperbighp = 0;
    int bignside = 1;
	int bighp = -1;
	char fn[256];
	catalog* cat;
	int nwritten;
	int BLOCK = 100000;
	double deduprad = 0.0;
	double dedupr2 = 0.0;
	bool allsky = FALSE;
	stardata* sweeplist;
	int npix;
	stardata** stararrays;
	int* stararrayN;
	int nmargin = 1;
	bool domags = FALSE;
	bool domagerrs = FALSE;
    bool domotion = FALSE;
    bool doid = FALSE;
	double jitter = 1.0;
    qfits_header* catheader;
    int loglvl = LOG_MSG;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
        case 's':
            bignside = atoi(optarg);
            break;
        case 'p':
            domotion = TRUE;
            break;
        case 'I':
            doid = TRUE;
            break;
		case 'j':
			jitter = atof(optarg);
			break;
		case 'g':
			domags = TRUE;
			break;
		case 'E':
			domagerrs = TRUE;
			break;
		case 'b':
			nmargin = atoi(optarg);
			break;
		case 'A':
			allsky = TRUE;
			break;
		case 'R':
			cutred = TRUE;
			break;
		case 'B':
			cutblue = TRUE;
			break;
		case 'J':
			cutj = TRUE;
			break;
		case 'd':
			deduprad = atof(optarg);
			break;
		case 'H':
			bighp = atoi(optarg);
			break;
		case 'S':
			maxperbighp = atoi(optarg);
			break;
		case 'N':
			Nside = atoi(optarg);
			break;
		case 'o':
			outfn = optarg;
			break;
		case 'n':
			sweeps = atoi(optarg);
			break;
		case 'm':
			minmag = atof(optarg);
			break;
		case 'M':
			maxmag = atof(optarg);
			break;
		}
    }

    log_init(loglvl);

	if (!outfn || (optind == argc)) {
		printf("Specify catalog names and input files.\n");
		print_help(args[0]);
		exit(-1);
	}
	i=0;
	if (cutred) i++;
	if (cutblue) i++;
	if (cutj) i++;
	if (i != 1) {
		printf("Must choose exactly one of red (-R), blue (-B), or J-band (-J) cuts.\n");
		print_help(args[0]);
		exit(-1);
	}
	if ((bighp == -1) && !allsky) {
		printf("Either specify a healpix (-H) or all-sky (-A).\n");
		print_help(args[0]);
		exit(-1);
	}

    if (Nside % bignside) {
        printf("Fine healpixelization Nside (-N) must be a multiple of the coarse healpixelization Nside (-s).\n");
        exit(-1);
    }

	HP = 12 * Nside * Nside;

	logmsg("Nside=%i, HP=%i, sweeps=%i, max number of stars = HP*sweeps = %i.\n", Nside, HP, sweeps, HP*sweeps);
	logmsg("Healpix side length: %g arcmin.\n", healpix_side_length_arcmin(Nside));

	if (allsky) {
		logmsg("Writing an all-sky catalog.\n");
        bignside = 0;
    } else
		logmsg("Writing big healpix %i.\n", bighp);

	if (deduprad > 0.0) {
		logmsg("Deduplication radius %f arcsec.\n", deduprad);
		dedupr2 = arcsec2distsq(deduprad);
	}

	starlists = calloc(HP, sizeof(bl*));

	// find the set of small healpixes that this big healpix owns
	// and add the margin.
	if (!allsky) {
		il* q = il_new(32);
		int hp;
		int nowned = 0;
        int bigbighp; // one of 12
        int bighpx, bighpy;
        int ninside;

        healpix_decompose_xy(bighp, &bigbighp, &bighpx, &bighpy, bignside);
        
		owned = calloc(HP, sizeof(bool));

		for (i=0; i<(Nside / bignside); i++) {
			for (k=0; k<(Nside / bignside); k++) {
                int xx = i + bighpx * (Nside / bignside);
                int yy = k + bighpy * (Nside / bignside);
				hp = healpix_compose_xy(bigbighp, xx, yy, Nside);
				owned[hp] = 1;
				nowned++;
			}
		}
        ninside = nowned;
        logmsg("Number of fine healpixes owned: %i\n", nowned);

        //write_radeclist(owned, Nside, "step0.rdls");

		// Prime the queue with the boundaries of the healpix.
		for (i=0; i<(Nside / bignside); i++) {
            int xx = i + bighpx * (Nside / bignside);
            int yy = i + bighpy * (Nside / bignside);
            int y0 =     bighpy * (Nside / bignside);
            int y1 =(1 + bighpy)* (Nside / bignside) - 1;
            int x0 =     bighpx * (Nside / bignside);
            int x1 =(1 + bighpx)* (Nside / bignside) - 1;
            
            assert(xx < Nside);
            assert(yy < Nside);
            assert(x0 < Nside);
            assert(x1 < Nside);
            assert(y0 < Nside);
            assert(y1 < Nside);

			hp = healpix_compose_xy(bigbighp, xx, y0, Nside);
            assert(owned[hp]);
			il_append(q, hp);
			hp = healpix_compose_xy(bigbighp, xx, y1, Nside);
            assert(owned[hp]);
			il_append(q, hp);
			hp = healpix_compose_xy(bigbighp, x0, yy, Nside);
            assert(owned[hp]);
			il_append(q, hp);
			hp = healpix_compose_xy(bigbighp, x1, yy, Nside);
            assert(owned[hp]);
			il_append(q, hp);
		}
        logmsg("Number of boundary healpixes on the primed queue: %i\n",
               il_size(q));

		// Now we want to add "nmargin" levels of neighbours.
		for (k=0; k<nmargin; k++) {
			int Q = il_size(q);
			for (i=0; i<Q; i++) {
				int j;
				int nn, neigh[8];
				hp = il_get(q, i);
				// grab the neighbours...
				nn = healpix_get_neighbours(hp, neigh, Nside);
				for (j=0; j<nn; j++) {
					// for any neighbour we haven't already looked at...
					if (!owned[neigh[j]]) {
						// add it to the queue
						il_append(q, neigh[j]);
						// mark it as fair game.
						owned[neigh[j]] = 1;
						nowned++;
					}
				}
			}
			il_remove_index_range(q, 0, Q);

            /*
             char fn[16];
             sprintf(fn, "step%02i.rdls", k+1);
             write_radeclist(owned, Nside, fn);
             */
		}
		il_free(q);

        //write_radeclist(owned, Nside, "final.rdls");

		logmsg("%i healpixes in this big healpix, plus %i boundary make %i total.\n",
               ninside, nowned - ninside, nowned);

	} else
		owned = NULL;

	nwritten = 0;

	add_sigbus_mmap_warning();

	sprintf(fn, outfn, bighp);
	cat = catalog_open_for_writing(fn);
	if (!cat) {
		fflush(stdout);
		fprintf(stderr, "Couldn't open file %s for writing catalog.\n", fn);
		exit(-1);
	}
	cat->healpix = bighp;
    cat->hpnside = bignside;
    catheader = catalog_get_header(cat);
    add_to_header(catheader, args, argc, allsky, jitter, sweeps);

	if (catalog_write_header(cat)) {
		ERROR("Failed to write catalog header");
		exit(-1);
	}

	// We are making "sweeps" sweeps through the healpixes, but since we
	// may remove stars from neighbouring healpixes because of duplicates,
	// we may want to keep a couple of extra stars for protection.
	if (sweeps)
		if (deduprad > 0)
			nkeep = sweeps + 3;
		else
			nkeep = sweeps;
	else
		nkeep = 0;

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		int i, N = 0;
		an_catalog* ancat = NULL;
		catalog* cat = NULL;
        rdlist_t* rdls = NULL;
        rd_t* rd = NULL;
        float* mag_array = NULL;
        float* magerr_array = NULL;
        
		int ndiscarded;
		int nduplicates;
		qfits_header* hdr;
		char* key;
		char* valstr;
		int lastgrass;

		infn = args[optind];

		hdr = qfits_header_read(infn);
		if (!hdr) {
			fprintf(stderr, "Couldn't read FITS header from file %s.\n", infn);
			if (!file_exists(infn))
				fprintf(stderr, "(file \"%s\" doesn't exist)\n", infn);
			exit(-1);
		}
		// look for AN_FILE (Astrometry.net filetype) in the FITS header.
		valstr = qfits_pretty_string(qfits_header_getstr(hdr, "AN_FILE"));
		if (valstr &&
			(strncasecmp(valstr, AN_FILETYPE_CATALOG, strlen(AN_FILETYPE_CATALOG)) == 0)) {
			logmsg("Looks like a catalog.\n");
			cat = catalog_open(infn);
			if (!cat) {
				ERROR("Couldn't open file \"%s\" as a catalog", infn);
				exit(-1);
			}
			if (!catalog_has_mag(cat)) {
				ERROR("Catalog file \"%s\" doesn't contain magnitudes!  "
                      "These are required (use \"-g\" to include them).\n",
                      infn);
				exit(-1);
			}
            if (domagerrs && !cat->mag_err) {
                ERROR("Catalog file \"%s\" doesn't contain required "
                      "magnitude errors table.  (Use -E to include it)", infn);
            }
            if (domotion &&
                !(cat->sigma_radec && cat->proper_motion && cat->sigma_pm)) {
                ERROR("Catalog file \"%s\" doesn't contain required "
                      "sigma_radec, proper_motion, and sigma_pm tables.  "
                      "(Use -p to include them)", infn);
            }
            if (doid && !cat->starids) {
                ERROR("Catalog file \"%s\" doesn't contain required "
                      "starid table.  (Use -I to include it)", infn);
            }
			N = cat->numstars;
        } else if (valstr && strcasecmp(valstr, AN_FILETYPE_RDLS) == 0) {
            logmsg("Looks like an RA,Dec list.\n");
            rdls = rdlist_open(infn);
            if (!rdls) {
                ERROR("Couldn't open file \"%s\" as an RDlist.\n", infn);
                exit(-1);
            }
            rd = rdlist_read_field(rdls, NULL);
            if (!rd) {
                ERROR("Failed to read a field from RDLS file \"%s\"\n", infn);
                exit(-1);
            }
            mag_array = rdlist_read_tagalong_column(rdls, "MAG", fitscolumn_float_type());
            if (domagerrs) {
                magerr_array = rdlist_read_tagalong_column(rdls, "MAGERR", fitscolumn_float_type());
                if (!magerr_array) {
                    ERROR("Failed to read magnitude errors (column MAGERR) from \"%s\".\n", infn);
                    exit(-1);
                }
            }
            if (domotion || doid) {
                ERROR("Unimplemented: motion and id flags for RDLS inputs.\n");
                exit(-1);
            }
            N = rd_n(rd);

		} else {
			// "AN_CATALOG" gets truncated...
			key = qfits_header_findmatch(hdr, "AN_CAT");
			if (key &&
				qfits_header_getboolean(hdr, key, 0)) {
				logmsg("Looks like an Astrometry.net catalog (AN_CATALOG = T header).\n");
				ancat = an_catalog_open(infn);
				if (!ancat) {
					fprintf(stderr, "Couldn't open file \"%s\" as an Astrometry.net catalog.\n", infn);
					exit(-1);
				}
				N = an_catalog_count_entries(ancat);
				an_catalog_set_blocksize(ancat, BLOCK);
			} else {
				logmsg("File \"%s\": doesn't seem to be an Astrometry.net "
						"catalog or a cut catalog (.objs.fits file).\n", infn);
			}
		}
        qfits_header_destroy(hdr);

		if (!(ancat || cat || rdls)) {
			fflush(stdout);
			fprintf(stderr, "Couldn't open input file \"%s\".\n", infn);
			exit(-1);
		}
		logmsg("Reading   %i entries from %s...\n", N, infn);
		fflush(stdout);
		ndiscarded = 0;
		nduplicates = 0;

		lastgrass = 0;
		for (i=0; i<N; i++) {
			stardata sd;
			int hp;
			an_entry* an = NULL;

			if ((i * 80 / N) != lastgrass) {
				logmsg(".");
				fflush(stdout);
				lastgrass = i * 80 / N;
			}

			if (ancat) {
				an = an_catalog_read_entry(ancat);
				if (!an) {
					fflush(stdout);
					fprintf(stderr, "Failed to read Astrometry.net catalog entry.\n");
					exit(-1);
				}
				sd.ra               = an->ra;
				sd.dec              = an->dec;
				sd.id               = an->id;
                sd.sigma_ra         = an->sigma_ra;
                sd.sigma_dec        = an->sigma_dec;
                sd.motion_ra        = an->motion_ra;
                sd.motion_dec       = an->motion_dec;
                sd.sigma_motion_ra  = an->sigma_motion_ra;
                sd.sigma_motion_dec = an->sigma_motion_dec;

            } else if (rdls) {
                sd.ra  = rd_getra (rd, i);
                sd.dec = rd_getdec(rd, i);
                sd.mag = mag_array[i];
                if (magerr_array)
                    sd.mag_err = magerr_array[i];

			} else {
				double* xyz;
				xyz = catalog_get_star(cat, i);
				xyzarr2radecdeg(xyz, &sd.ra, &sd.dec);
				sd.mag = cat->mag[i];
                if (domagerrs)
                    sd.mag_err = cat->mag_err[i];
                if (doid)
                    sd.id = cat->starids[i];
                if (domotion) {
                    sd.sigma_ra         = cat->sigma_radec[2*i];
                    sd.sigma_dec        = cat->sigma_radec[2*i + 1];
                    sd.motion_ra        = cat->proper_motion[2*i];
                    sd.motion_dec       = cat->proper_motion[2*i + 1];
                    sd.sigma_motion_ra  = cat->sigma_pm[2*i];
                    sd.sigma_motion_dec = cat->sigma_pm[2*i + 1];
                }
			}

			hp = radecdegtohealpix(sd.ra, sd.dec, Nside);

			if (owned && !owned[hp]) {
				ndiscarded++;
				continue;
			}

			if (ancat) {
				if (get_magnitude(an, &sd.mag, &sd.mag_err))
					continue;
			}

			if (sd.mag < minmag)
				continue;
			if (sd.mag > maxmag)
				continue;

			if (!starlists[hp])
				starlists[hp] = bl_new(nkeep ? nkeep : 10, sizeof(stardata));

			if (nkeep && (bl_size(starlists[hp]) >= nkeep)) {
				// is this list full?
				stardata* last = bl_access(starlists[hp], nkeep-1);
				if (sd.mag > last->mag)
					// this new star is dimmer than the "nkeep"th one in the list...
					continue;
			}
			if (dedupr2 > 0.0) {
				int duphp=-1, dupindex=-1;
				stardata* dupsd;
				if (find_duplicate(&sd, hp, Nside, starlists,
								   dedupr2, &duphp, &dupindex)) {
					nduplicates++;
					// Which one is brighter?
					dupsd = bl_access(starlists[duphp], dupindex);
					if (dupsd->mag <= sd.mag)
						// The existing star is brighter; just skip this star.
						continue;
					else
						// Remove the old star.
						bl_remove_index(starlists[duphp], dupindex);
				}
			}
			bl_insert_sorted(starlists[hp], &sd, sort_stardata_mag);
		}
		logmsg("\n");

		if (bighp != -1)
			logmsg("Discarded %i stars not in this big healpix.\n", ndiscarded);
		logmsg("Discarded %i duplicate stars.\n", nduplicates);

		if (nkeep)
			for (i=0; i<HP; i++) {
				int size;
				stardata* d;
				if (!starlists[i]) continue;
				size = bl_size(starlists[i]);
				if (size)
					d = bl_access(starlists[i], size-1);
				if (size < nkeep) continue;
				bl_remove_index_range(starlists[i], nkeep, size-nkeep);
			}

		if (ancat)
			an_catalog_close(ancat);
        else if (rdls) {
            rd_free(rd);
            rdlist_close(rdls);
            free(mag_array);
            free(magerr_array);
        } else
			catalog_close(cat);
			
	}
	free(owned);

	// compact the stars into arrays.
	npix = 0;
	for (i=0; i<HP; i++)
		if (starlists[i]) {
			starlists[npix] = starlists[i];
			npix++;
		}
	starlists = realloc(starlists, npix * sizeof(bl*));
	// (reuse the bl* storage as stardata* storage; see below)
	stararrays = (stardata**)starlists;

	stararrayN = malloc(npix * sizeof(int));

	for (i=0; i<npix; i++) {
		bl* list;
		int n;
		// reusing storage: here we save the bl*
		list = starlists[i];
		n = bl_size(list);
		if (sweeps && sweeps < n)
			n = sweeps;
		// then store the stardata* in it.
		stararrays[i] = malloc(n * sizeof(stardata));
		// then copy the bl's data.
		bl_copy(list, 0, n, stararrays[i]);
		bl_free(list);
		stararrayN[i] = n;
	}

	sweeplist = malloc(npix * sizeof(stardata));

	// sweep through the healpixes...
	for (k=0;; k++) {
		char key[64];
		char val[64];
		int nsweep = 0;
		// gather up the stars that will be used in this sweep...
		for (i=0; i<npix; i++) {
			stardata* sd;
			int N;
			if (!stararrays[i])
				continue;
			N = stararrayN[i];
			if (k >= N) {
				free(stararrays[i]);
				stararrays[i] = NULL;
				continue;
			}
			sd = stararrays[i] + k;
			memcpy(sweeplist + nsweep, sd, sizeof(stardata));
			nsweep++;
		}
		if (!nsweep)
			break;

		// sort them by magnitude...
		qsort(sweeplist, nsweep, sizeof(stardata), sort_stardata_mag);

		// write them out...
		for (i=0; i<nsweep; i++) {
			double xyz[3];
			stardata* sd = sweeplist + i;
			radecdeg2xyzarr(sd->ra, sd->dec, xyz);

			if (catalog_write_star(cat, xyz)) {
				ERROR("Failed to write star to catalog.");
				exit(-1);
			}
			if (domags)
                catalog_add_mag(cat, sd->mag);
			if (domagerrs)
                catalog_add_mag_err(cat, sd->mag_err);
            if (doid)
                catalog_add_id(cat, sd->id);
            if (domotion) {
                catalog_add_sigmas(cat, sd->sigma_ra, sd->sigma_dec);
                catalog_add_sigma_pms(cat, sd->sigma_motion_ra, sd->sigma_motion_dec);
                catalog_add_pms(cat, sd->motion_ra, sd->motion_dec);
            }

			nwritten++;
			if (nwritten == maxperbighp)
				break;
		}

		// add to FITS header...
		if (sweeps || (k<100)) {
			sprintf(key, "SWEEP%i", (k+1));
			sprintf(val, "%i", nsweep);
			qfits_header_mod(catheader, key, val, "");
		}

		logmsg("sweep %i: got %i stars (%i total)\n", k, nsweep, nwritten);
		fflush(stdout);
		// if we broke out of the loop...
		if (nwritten == maxperbighp)
			break;
	}
	logmsg("Made %i sweeps through the healpixes.\n", k);

	free(sweeplist);
	free(stararrayN);
	for (i=0; i<npix; i++)
		free(stararrays[i]);
	free(stararrays);

	if (catalog_fix_header(cat)) {
		fprintf(stderr, "Failed to fix catalog header.\n");
		exit(-1);
	}
	if (domags) {
        logmsg("Writing magnitudes...\n");
		if (catalog_write_mags(cat)) {
            ERROR("Failed to write magnitudes");
            exit(-1);
        }
    }
	if (domagerrs) {
        logmsg("Writing magnitude errors...\n");
		if (catalog_write_mag_errs(cat)) {
            ERROR("Failed to write magnitude errors");
            exit(-1);
        }
    }
    if (doid) {
        logmsg("Writing IDs...\n");
        if (catalog_write_ids(cat)) {
            ERROR("Failed to write star IDs");
            exit(-1);
        }
    }
    if (domotion) {
        logmsg("Writing sigmas...\n");
        if (catalog_write_sigmas(cat)) {
            ERROR("Failed to write star motions");
            exit(-1);
        }
        logmsg("Writing proper motions...\n");
        if (catalog_write_pms(cat)) {
            ERROR("Failed to write star motions");
            exit(-1);
        }
        logmsg("Writing sigma proper motions...\n");
        if (catalog_write_sigma_pms(cat)) {
            ERROR("Failed to write star motions");
            exit(-1);
        }
    }
	if (catalog_close(cat)) {
		ERROR("Failed to close output catalog");
		exit(-1);
	}

	return 0;
}

