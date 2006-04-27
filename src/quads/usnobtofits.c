#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <endian.h>
#include <netinet/in.h>
#include <byteswap.h>
#include <assert.h>

#include "usnob.h"
#include "qfits.h"
#include "healpix.h"
#include "starutil.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define IS_LITTLE_ENDIAN 1
#else
#define IS_LITTLE_ENDIAN 0
#endif

#define OPTIONS "ho:"
/* r:R:d:D:*/

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename-template>\n"
		   "  [-N <healpix-nside>]  (default = 8; should be power of two.)\n"
		   "  <input-file> [<input-file> ...]\n",
		   progname);
		   /*
			 "  [-r <minimum RA>]\n"
			 "  [-R <maximum RA>]\n"
			 "  [-d <minimum DEC>]\n"
			 "  [-D <maximum DEC>]\n"
			 "           In units of degrees.\n"
			 "           If your values are negative, add \"--\" in between the argument and value\n"
			 "           eg,   -r -- -100\n"
		   */
}

extern char *optarg;
extern int optind, opterr, optopt;

int fits_add_column(qfits_table* table, int column, tfits_type type,
					int ncopies, char* units, char* label) {
	int atomsize;
	int colsize;
	switch (type) {
	case TFITS_BIN_TYPE_A:
	case TFITS_BIN_TYPE_X:
	case TFITS_BIN_TYPE_L:
	case TFITS_BIN_TYPE_B:
		atomsize = 1;
		break;
	case TFITS_BIN_TYPE_I:
		atomsize = 2;
		break;
	case TFITS_BIN_TYPE_J:
	case TFITS_BIN_TYPE_E:
		atomsize = 4;
		break;
		//case TFITS_BIN_TYPE_K:
	case TFITS_BIN_TYPE_D:
		atomsize = 8;
		break;
	default:
		fprintf(stderr, "Unknown atom size for type %i.\n", type);
		return -1;
	}
	if (type == TFITS_BIN_TYPE_X)
		// bit field: convert bits to bytes, rounding up.
		ncopies = (ncopies + 7) / 8;
	colsize = atomsize * ncopies;

	qfits_col_fill(table->col + column, ncopies, 0, atomsize, type, label, units,
				   "", "", 0, 0, 0, 0, table->tab_w);
	table->tab_w += colsize;
	return 0;
}

static inline void dstn_swap_bytes(unsigned char* c1, unsigned char* c2) {
	unsigned char tmp = *c1;
	*c1 = *c2;
	*c2 = tmp;
}

static inline void hton64(void* ptr) {
#if IS_LITTLE_ENDIAN
	unsigned char* c = (unsigned char*)ptr;
	dstn_swap_bytes(c+0, c+7);
	dstn_swap_bytes(c+1, c+6);
	dstn_swap_bytes(c+2, c+5);
	dstn_swap_bytes(c+3, c+4);
#endif
}

static inline void hton32(void* ptr) {
#if IS_LITTLE_ENDIAN
	unsigned char* c = (unsigned char*)ptr;
	dstn_swap_bytes(c+0, c+3);
	dstn_swap_bytes(c+1, c+2);
#endif
}

static inline void hton16(void* ptr) {
#if IS_LITTLE_ENDIAN
	unsigned char* c = (unsigned char*)ptr;
	dstn_swap_bytes(c+0, c+1);
#endif
}

int fits_add_column_D(FILE* fid, double value) {
	assert(sizeof(double) == 8);
	hton64(&value);
	if (fwrite(&value, 8, 1, fid) != 1) {
		fprintf(stderr, "Failed to write a double to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_add_column_E(FILE* fid, float value) {
	assert(sizeof(float) == 4);
	hton32(&value);
	if (fwrite(&value, 4, 1, fid) != 1) {
		fprintf(stderr, "Failed to write a float to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_add_column_B(FILE* fid, unsigned char value) {
	if (fwrite(&value, 1, 1, fid) != 1) {
		fprintf(stderr, "Failed to write a bit array to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_add_column_X(FILE* fid, unsigned char value) {
	return fits_add_column_B(fid, value);
}

int fits_add_column_I(FILE* fid, int16_t value) {
	hton16(&value);
	if (fwrite(&value, 2, 1, fid) != 1) {
		fprintf(stderr, "Failed to write a short to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_add_column_J(FILE* fid, int32_t value) {
	hton32(&value);
	if (fwrite(&value, 4, 1, fid) != 1) {
		fprintf(stderr, "Failed to write an int to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int init_fits_file(FILE** fids, qfits_header** headers, char* outfn,
				   int hp, qfits_table* table, int Nside) {
	char fn[256];
	char val[256];
    qfits_header* tablehdr;
    qfits_header* header;

	sprintf(fn, outfn, hp);
	fids[hp] = fopen(fn, "wb");
	if (!fids[hp]) {
		fprintf(stderr, "Couldn't open output file %s for writing: %s\n", fn, strerror(errno));
		return -1;
	}

	// the header
	headers[hp] = header = qfits_table_prim_header_default();

	// header remarks...
	sprintf(val, "%u", hp);
	qfits_header_add(header, "HEALPIX", val, "The healpix number of this catalog.", NULL);
	sprintf(val, "%u", Nside);
	qfits_header_add(header, "NSIDE", val, "The healpix resolution.", NULL);
	sprintf(val, "%u", 0);
	qfits_header_add(header, "NOBJS", val, "Number of objects in this catalog.", NULL);
	// etc...

	qfits_header_dump(header, fids[hp]);
	tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, fids[hp]);
	qfits_header_destroy(tablehdr);
	return 0;
}

int main(int argc, char** args) {
	char* outfn = NULL;
	/*
	  double ramin = 0.0;
	  double ramax = 360.0;
	  double decmin = -90.0;
	  double decmax = 90.0;
	*/
    int c;
	int startoptind;
	uint nrecords, nobs, nfiles;
	int Nside = 8;

	FILE** fids;
	uint* hprecords;
	qfits_header** headers;

	qfits_table* table;

	int i, HP;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'n':
			Nside = atoi(optarg);
			/*
			  case 'd':
			  decmin = atof(optarg);
			  break;
			  case 'D':
			  decmax = atof(optarg);
			  break;
			  case 'r':
			  ramin = atof(optarg);
			  break;
			  case 'R':
			  ramax = atof(optarg);
			  break;
			*/
		case 'o':
			outfn = optarg;
			break;
		}
    }

	if (!outfn || (optind == argc)) {
		print_help(args[0]);
		exit(-1);
	}

	if (Nside < 1) {
		fprintf(stderr, "Nside must be >= 1.\n");
		print_help(args[0]);
		exit(-1);
	}

	HP = 12 * Nside * Nside;
	fids = malloc(HP * sizeof(FILE*));
	memset(fids, 0, HP * sizeof(FILE*));

	headers = malloc(HP * sizeof(qfits_header*));
	memset(headers, 0, HP * sizeof(qfits_header*));

	hprecords = malloc(HP * sizeof(uint));
	memset(hprecords, 0, HP*sizeof(uint));

	// init FITS table definition...
	{
		uint datasize;
		uint ncols, nrows, tablesize;
		int ob;
		int col;

		// one big table: the sources.
		// dummy values here...
		datasize = 0;
		ncols = 54;
		nrows = 0;
		tablesize = datasize * nrows * ncols;
		table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
		table->tab_w = 0;
		col = 0;
		// col 0: RA (double)
		fits_add_column(table, col++, TFITS_BIN_TYPE_D, 1, "Degrees", "RA");
		// col 1: DEC (double)
		fits_add_column(table, col++, TFITS_BIN_TYPE_D, 1, "Degrees", "DEC");
		// col 2: flags (bit array)
		//  -diffraction_spike
		//  -motion_catalog
		//  -ys4
		//  -reject star ?
		//  -Tycho2 star ?
		fits_add_column(table, col++, TFITS_BIN_TYPE_X, 3, "(binary flags)", "FLAGS");
		// col 3: sig_RA (float)
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "sigma_RA");
		// col 4: sig_DEC (float)
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "sigma_DEC");

		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "sigma_RA_fit");
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "sigma_DEC_fit");

		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Years", "epoch");

		// motion
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "(probability)", "mu_probability");
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "mu_RA");
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "mu_DEC");
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "sigma_mu_RA");
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "sigma_mu_DEC");

		fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", "Num Detections");

		for (ob=0; ob<5; ob++) {
			fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Mag", "Magnitude");
			fits_add_column(table, col++, TFITS_BIN_TYPE_I, 1, "", "Field number");
			fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", "Survey number");
			fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", "Star/Galaxy");
			fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "xi residual");
			fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "eta residual");
			fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", "Calibration source");
			fits_add_column(table, col++, TFITS_BIN_TYPE_J, 1, "", "PMM backpointer");
		}

		assert(col == ncols);

		table->tab_w = datasize = qfits_compute_table_width(table);
	}

	nrecords = 0;
	nobs = 0;
	nfiles = 0;

	printf("Reading USNO files... ");
	fflush(stdout);

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		FILE* fid;
		unsigned char* map;
		size_t map_size;
		int i;

		infn = args[optind];
		fid = fopen(infn, "rb");
		if (!fid) {
			fprintf(stderr, "Couldn't open input file %s: %s\n", infn, strerror(errno));
			exit(-1);
		}

		if ((optind > startoptind) && ((optind - startoptind) % 100 == 0)) {
			printf("\nReading file %i of %i: %s\n", optind - startoptind,
				   argc - startoptind, infn);
		}

		if (fseeko(fid, 0, SEEK_END)) {
			fprintf(stderr, "Couldn't seek to end of input file %s: %s\n", infn, strerror(errno));
			exit(-1);
		}

		map_size = ftello(fid);

		fseeko(fid, 0, SEEK_SET);

		map = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fileno(fid), 0);
		if (map == MAP_FAILED) {
			fprintf(stderr, "Couldn't mmap input file %s: %s\n", infn, strerror(errno));
			exit(-1);
		}
		fclose(fid);

		if (map_size % USNOB_RECORD_SIZE) {
			fprintf(stderr, "Warning, input file %s has size %u which is not divisible into %i-byte records.\n",
					infn, map_size, USNOB_RECORD_SIZE);
		}

		for (i=0; i<map_size; i+=USNOB_RECORD_SIZE) {
			usnob_entry entry;
			int hp;
			FILE* fid;
			unsigned char flags;
			int ob;

			// debug
			off_t offset1;
			off_t offset2;
			
			if (usnob_parse_entry(map + i, &entry)) {
				fprintf(stderr, "Failed to parse USNOB entry: offset %i in file %s.\n",
						i, infn);
				exit(-1);
			}

			hp = radectohealpix_nside(deg2rad(entry.ra), deg2rad(entry.dec), Nside);

			if (!fids[hp]) {
				if (init_fits_file(fids, headers, outfn, hp, table, Nside)) {
					fprintf(stderr, "Failed to initialized FITS file %i.\n", hp);
					exit(-1);
				}
			}

			fid = fids[hp];
			hprecords[hp]++;

			flags =
				(entry.diffraction_spike << 7) |
				(entry.motion_catalog    << 6) |
				(entry.ys4               << 5);

			offset1 = ftello(fid);

			if (fits_add_column_D(fid, entry.ra) ||
				fits_add_column_D(fid, entry.dec) ||
				fits_add_column_X(fid, flags) ||
				fits_add_column_E(fid, entry.sigma_ra) ||
				fits_add_column_E(fid, entry.sigma_dec) ||
				fits_add_column_E(fid, entry.sigma_ra_fit) ||
				fits_add_column_E(fid, entry.sigma_dec_fit) ||
				fits_add_column_E(fid, entry.epoch) ||
				fits_add_column_E(fid, entry.mu_prob) ||
				fits_add_column_E(fid, entry.mu_ra) ||
				fits_add_column_E(fid, entry.mu_dec) ||
				fits_add_column_E(fid, entry.sigma_mu_ra) ||
				fits_add_column_E(fid, entry.sigma_mu_dec) ||
				fits_add_column_B(fid, entry.ndetections)) {
				fprintf(stderr, "Failed to write FITS entry.\n");
				exit(-1);
			}

			for (ob=0; ob<5; ob++) {
				if (fits_add_column_E(fid, entry.obs[ob].mag) ||
					fits_add_column_I(fid, entry.obs[ob].field) ||
					fits_add_column_B(fid, entry.obs[ob].survey) ||
					fits_add_column_B(fid, entry.obs[ob].star_galaxy) ||
					fits_add_column_E(fid, entry.obs[ob].xi_resid) ||
					fits_add_column_E(fid, entry.obs[ob].eta_resid) ||
					fits_add_column_B(fid, entry.obs[ob].calibration) ||
					fits_add_column_J(fid, entry.obs[ob].pmmscan)) {
					fprintf(stderr, "Failed to write FITS entry.\n");
					exit(-1);
				}
			}

			offset2 = ftello(fid);

			assert((offset2 - offset1) == table->tab_w);

			nrecords++;
			nobs += (entry.ndetections == 0 ? 1 : entry.ndetections);
		}

		munmap(map, map_size);

		nfiles++;
		printf(".");
		fflush(stdout);
	}
	printf("\n");

	// close all the files...
	for (i=0; i<HP; i++) {
		qfits_header* header;
		qfits_header* tablehdr;
		char val[256];
		off_t offset;
		FILE* fid;
		int npad;

		fid = fids[i];
		if (!fid) continue;
		header = headers[i];

		offset = ftello(fid);
		fseeko(fid, 0, SEEK_SET);

		sprintf(val, "%u", hprecords[i]);
		qfits_header_mod(header, "NOBJS", val, "Number of objects in this catalog.");
		qfits_header_dump(header, fid);
        qfits_header_destroy(header);

		table->nr = hprecords[i];
		tablehdr = qfits_table_ext_header_default(table);
		qfits_header_dump(tablehdr, fid);
		qfits_header_destroy(tablehdr);

		fseek(fid, offset, SEEK_SET);

		// pad with zeros up to a multiple of 2880 bytes.
		npad = (offset % FITS_BLOCK_SIZE);
		if (npad) {
			char nil = '\0';
			int i;
			npad = FITS_BLOCK_SIZE - npad;
			for (i=0; i<npad; i++)
				fwrite(&nil, 1, 1, fid);
		}

		if (fclose(fid)) {
			fprintf(stderr, "Failed to close file %i: %s\n", i, strerror(errno));
		}
	}

	printf("Read %u files, %u records, %u observations.\n",
		   nfiles, nrecords, nobs);

	qfits_table_close(table);

	free(headers);
	free(fids);
	free(hprecords);
	
	return 0;
}
