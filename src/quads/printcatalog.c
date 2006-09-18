/**
   Prints the headers of a catalogue (.objs file)

   Input: .objs
*/

#include <errno.h>
#include <math.h>
#include "starutil.h"
#include "fileutil.h"
#include "catalog.h"

#define OPTIONS "hf:m"

extern char *optarg;
extern int optind, opterr, optopt;

void get_star_radec(FILE *fid, off_t marker, 
					uint i, double* pra, double* pdec);

void print_help(char* progname) {
    printf("\nUsage: %s -f <input-file> [-m]\n\n"
		   "  <input-file>  a catalog filename, specified without its .objs suffix.\n"
		   "  [-m]          print Matlab literals describing the RA,DEC of each object.\n\n",
		   progname);
}

int main(int argc, char *argv[]) {
	int argchar;
	char* basefname = NULL;
	char* catfname;
	catalog* cat = NULL;
	bool printout = FALSE;

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'm':
			printout = TRUE;
			break;
        case 'f':
            basefname = optarg;
            break;
		case 'h':
			print_help(argv[0]);
			exit(0);
        default:
            return (OPT_ERR);
        }
    
	if (!basefname) {
		print_help(argv[0]);
		exit(-1);
    }

	catfname = mk_catfn(basefname);
	cat = catalog_open(catfname, 0);
	if (!cat) {
		fprintf(stderr, "Couldn't read catalog.\n");
		exit(-1);
	}
	free_fn(catfname);

    fprintf(stderr, "Got %u stars.\n"
			"Limits RA=[%g, %g] DEC=[%g, %g] degrees.\n",
            cat->numstars,
			rad2deg(cat->ramin), rad2deg(cat->ramax),
			rad2deg(cat->decmin), rad2deg(cat->decmax));

	if (printout) {
		int i;

		printf("radec=zeros([%u,2]);\n", cat->numstars);
		// read a star at a time...
		for (i=0; i<cat->numstars; i++) {
			double* xyz;
			double ra, dec;
			xyz = catalog_get_star(cat, i);
			ra = xy2ra(xyz[0], xyz[1]);
			dec = z2dec(xyz[2]);
			printf("radec(%i,:)=[%g,%g];\n", i+1, ra, dec);
		}
		printf("ra=radec(:,1);\n");
		printf("dec=radec(:,2);\n");
	}
 
	catalog_close(cat);

    return 0;
}
