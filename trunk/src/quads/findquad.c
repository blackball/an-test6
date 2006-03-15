#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hf:"
const char HelpString[] =
    "findquad -f fname \n";

extern char *optarg;
extern int optind, opterr, optopt;

char *qidxfname = NULL;
uint thestar;
uint thequad;
char buff[100], maxstarWidth;

signed int compare_sidx(const void *x, const void *y)
{
	uint xval, yval;
	xval = *(uint *)x;
	yval = *(uint *)y;
	if (xval > yval)
		return (1);
	else if (xval < yval)
		return ( -1);
	else
		return (0);
}

int main(int argc, char *argv[])
{
	int argidx, argchar; 
        int jj;
	uint ii, numstars;
	FILE *qidxfid = NULL; 
	uint *starlist, *matchstar;
	uint *starnumq;
	uint **starquads;
        char scanrez = 1;

	if (argc <= 3) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'f':
			qidxfname = mk_qidxfn(optarg);
			break;
		case '?':
			fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		case 'h':
			fprintf(stderr, HelpString);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}

	if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	fprintf(stderr, "findquad: looking up quads in %s\n", qidxfname);

        fprintf(stderr, "  Reading quad index...");
        fflush(stderr);
        fopenin(qidxfname, qidxfid);
        numstars = read_quadidx(qidxfid, &starlist, &starnumq, &starquads);
        if (numstars == 0) {
                fprintf(stderr, "ERROR (findquad) -- out of memory\n");
                return 2;
        }
        fprintf(stderr, "  (%u unique stars used in quads)\n", numstars);
        fclose(qidxfid);

        while (!feof(stdin) && scanrez) {
                scanrez = fscanf(stdin, "%u", &thestar);
                matchstar = (uint *)bsearch(&thestar, starlist, numstars,
                                            sizeof(uint *), compare_sidx);
                if (matchstar == NULL)
                        fprintf(stdout, "[]\n");
                else {
                        ii = (uint)(matchstar - starlist);
                        fprintf(stdout, "[");
                        for (jj = 0;jj < starnumq[ii];jj++)
                                fprintf(stdout, "%u, ", *(starquads[ii] + jj));
                        fprintf(stdout, "]\n");
                }
                fflush(stdout);
                for (ii = 0;ii < numstars; ii++)
                        free(starquads[ii]);
        }

        free(starquads);
        free(starnumq);
        free(starlist);
	free_fn(qidxfname);
	return 0;
}





