#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "fitsio.h"
#include "dimage.h"
#include "ver.h"
#include <assert.h>

void printHelp(char* progname)
{
	fprintf(stderr, "%s usage:\n"
	        "   -i <input file>      file to tweak\n"
	        "   -m actually modify file\n"
	        , progname);
}

int main(int argc, char *argv[])
{
	fitsfile *fptr;         /* FITS file pointer, defined in fitsio.h */
	//char card[FLEN_CARD];   /* Standard string lengths defined in fitsio.h */
	int status = 0; // FIXME should have ostatus too
	int naxis;
	long naxisn[100];
	int fix = 0;
	int kk;
	char* infile = NULL;
	char argchar;

	while ((argchar = getopt(argc, argv, "hmi:")) != -1)
		switch (argchar) {
		case 'h':
			printHelp(argv[0]);
			exit(0);
		case 'i':
			infile = optarg;
			break;
		case 'm':
			fix = 1;
			break;
		}

	if (!infile) {
		printHelp(argv[0]);
		exit( -1);
	}
	if (fits_open_file(&fptr, infile, fix ? READWRITE : READONLY, &status)) {
		fprintf(stderr, "Error reading file %s\n", infile);
		fits_report_error(stderr, status);
		exit(-1);
	}

	// Are there multiple HDU's?
	int nhdus;
	fits_get_num_hdus(fptr, &nhdus, &status);
	fprintf(stderr, "nhdus=%d\n", nhdus);

	/* Parameters for simplexy; save for debugging */
	if (fix) {
		//fits_write_key(fptr, TINT, "SVNVER", &maxnpeaks, "Max num of peaks total", &status);

		fits_write_history(fptr, 
			"Edited by astrometry.net's fixaxis",
			&status);
		fits_write_history(fptr, "SVN: " SVNREV, &status);
		assert(!status);
		fits_write_history(fptr, "SVN: " SVNURL, &status);
		assert(!status);
		fits_write_history(fptr, 
			"Visit us on the web at http://astrometry.net/",
			&status);
		assert(!status);
	}

	int hdutype;
//	int nimgs = 0;
	int broken = 0; // Correct until proven otherwise!

	// Check axis on each one
	for (kk=1; kk <= nhdus; kk++) {
		fits_movabs_hdu(fptr, kk, &hdutype, &status);
		fits_get_hdu_type(fptr, &hdutype, &status);

		if (hdutype != IMAGE_HDU) 
			continue;

		fits_get_img_dim(fptr, &naxis, &status);
		if (status) {
			fits_report_error(stderr, status);
			exit( -1);
		}

		// Assume that if naxis is not zero, then it is correct.
		// FIXME prove this is true; but by XP dogma, will not fix
		// until we find an image where naxis !=0 and naxis is not
		// correct. Trac #234
		if (naxis != 0)
			continue;


		// Now find if and how many NAXISN keywords there are
		int naxis_actual = 0;
		char keyname[10];
		while(1) {
			snprintf(keyname, 10, "NAXIS%d", naxis_actual+1);
			fits_read_key(fptr, TINT, keyname, naxisn+naxis_actual,
					NULL, &status);
			if (status) {
				// No more.
				status = 0;
				break;
			}
			naxis_actual++;
		}
		if (naxis_actual == 0) {
			// Ok, it's real.
			continue;
		}

		broken++;
		fprintf(stderr,"broken: actual naxis %d \n", naxis_actual);

		if (fix) {
//			char comment[80];
			//fits_read_key(fptr, TINT, "NAXIS", &naxis, comment, &status);
			//fprintf(stderr, "%s\n", comment);
			//assert(!status);
			fits_update_key(fptr, TINT, "NAXIS", &naxis_actual,
					"set by fixaxis", &status);
			assert(!status);
		}
	}

	if (status == END_OF_FILE)
		status = 0; /* Reset after normal error */

	fits_close_file(fptr, &status);

	if (status)
		fits_report_error(stderr, status); /* print any error message */
	return (status);
}
