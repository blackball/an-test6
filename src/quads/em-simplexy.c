#include <sys/param.h>
#include <math.h>
#include <stdio.h>

#include "qfits_image.h"

#include "xylist.h"

static const char* OPTIONS = "h";

void printHelp() {
	fprintf(stderr,
			"Usage: em-simplexy [options] <in.xy.fits> <in.image.fits> <out.xy.fits>\n"
			"\n"
			"\n");
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int argchar;
    char* infn;
    char* imgfn;
    char* outfn;
    xylist* inxy;
    xylist* outxy;
    double* xy;
    int i, N;
    double psfw;
    qfitsloader qimg;
    int W, H;

    qimg.filename = imgfn;
    qimg.xtnum = 0;
    // FIXME - ugh!
    qimg.pnum = 0;
    qimg.ptype = PTYPE_DOUBLE;
    qimg.map = 1;
    
    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case '?':
		case 'h':
			printHelp();
			exit(0);
        }

    if (optind != argc - 3) {
		printHelp();
		exit(-1);
    }

    infn  = args[optind];
    imgfn = args[optind + 1];
    outfn = args[optind + 2];

    if (qfitsloader_init(&qimg)) {
        fprintf(stderr, "Failed to read header of FITS image %s.\n", imgfn);
        exit(-1);
    }

    W = qimg.lx;
    H = qimg.ly;

    if (qimg.np != 1) {
        printf("Warning, image has %i planes but this program only looks at the first one.\n", qimg.np);
    }

    inxy = xylist_open(infn);
    if (!inxy) {
        fprintf(stderr, "Failed to open input file.\n");
        exit(-1);
    }
    outxy = xylist_open_for_writing(outfn);
    if (!outxy) {
        fprintf(stderr, "Failed to open output file.\n");
        exit(-1);
    }

    if (qfits_loadpix(&qimg)) {
        fprintf(stderr, "Failed to read pixels from FITS image %s.\n", imgfn);
        exit(-1);
    }

    N = xylist_n_entries(inxy, 1);
    if (N == -1) {
        fprintf(stderr, "Couldn't find number of entries in field 1.\n");
        exit(-1);
    }
    xy = malloc(N * 2 * sizeof(double));
    if (xylist_read_entries(inxy, 1, 0, N, xy)) {
        fprintf(stderr, "Failed to read entries.\n");
        exit(-1);
    }

    psfw = qfits_header_getdouble(inxy->header, "DPSF", 1.0);



    // EM
    for (i=0; i<N; i++) {
        // crazy FITS indexing :)
        double x = xy[2*i+0] - 1;
        double y = xy[2*i+1] - 1;
        int ix, iy;

        ix = round(x);
        iy = round(y);
        if (ix < 0 || ix >= W) {
            printf("Error, source %i has x=%g but image width is %i.\n", i, x, W);
            continue;
        }
        if (iy < 0 || iy >= H) {
            printf("Error, source %i has y=%g but image height is %i.\n", i, y, H);
            continue;
        }

        
    }











    free(xy);
    xylist_close(inxy);
    if (xylist_close(outxy)) {
        fprintf(stderr, "Failed to close output xylist.\n");
        exit(-1);
    }
    qfitsloader_free_buffers(&qimg);

    return 0;
}




