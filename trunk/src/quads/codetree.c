#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "hR:f:B:"
const char HelpString[] =
    "codetree -f fname [-B buffer_length] [-R KD_RMIN]\n"
    "  -B sets the number of stars to read into memory at once\n"
    "  KD_RMIN (default 50) is the max# points per leaf in KD tree\n";

#define MEM_LOAD 1000000000
extern char *optarg;
extern int optind, opterr, optopt;

codearray *readcodes(FILE *fid, qidx *numcodes, dimension *Dim_Codes,
                     double *index_scale, qidx buffsize);

dimension readnextcode(FILE *codefid, code *tmpcode,
                       dimension Dim_Codes);

char *treefname = NULL;
char *codefname = NULL;

int main(int argc, char *argv[])
{
	int argidx, argchar; //  opterr = 0;
	int kd_Rmin = DEFAULT_KDRMIN;
	FILE *codefid = NULL, *treefid = NULL;
	qidx numcodes, ii;
	code *tmpcode;
	dimension Dim_Codes;
	double index_scale;
	qidx buffsize = (qidx)floor(MEM_LOAD / (sizeof(double) + sizeof(int)) * DIM_CODES);
	codearray *thecodes = NULL;
	kdtree *codekd = NULL;

	if (argc <= 2) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'R':
			kd_Rmin = (int)strtoul(optarg, NULL, 0);
			break;
		case 'B':
			buffsize = strtoul(optarg, NULL, 0);
			break;
		case 'f':
			treefname = mk_ctreefn(optarg);
			codefname = mk_codefn(optarg);
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

	fprintf(stderr, "codetree: building KD tree for %s\n", codefname);

	fprintf(stderr, "  Reading codes...");
	fflush(stderr);
	fopenin(codefname, codefid);
	free_fn(codefname);
	thecodes = readcodes(codefid, &numcodes, &Dim_Codes,
								&index_scale, buffsize);
	if (thecodes == NULL)
		return (1);
	fprintf(stderr, "got %d codes (dim %hu).\n", thecodes->size, Dim_Codes);

	fprintf(stderr, "  Building code KD tree...\n");
	fflush(stderr);
	codekd = mk_codekdtree(thecodes, kd_Rmin);
	free_codearray(thecodes);
	if (codekd == NULL)
		return (2);

	if (numcodes > buffsize) {
		fprintf(stderr, "    buffer codes (%d nodes, depth %d).\n",
		        codekd->num_nodes, codekd->max_depth);
		tmpcode = mk_coded(Dim_Codes);
		for (ii = 0;ii < (numcodes - buffsize);ii++) {
			readnextcode(codefid, tmpcode, Dim_Codes);
			add_point_to_kdtree(codekd, (dyv *)tmpcode);
			if (is_power_of_two(ii + 1))
				fprintf(stderr, "    %lu / %lu of rest done\r", ii + 1, numcodes - buffsize);
		}
		free_code(tmpcode);
	}
	fprintf(stderr, "    done (%d nodes, depth %d).\n",
	        codekd->num_nodes, codekd->max_depth);
	fclose(codefid);

	fprintf(stderr, "  Writing code KD tree to %s...", treefname);
	fflush(stderr);
	fopenout(treefname, treefid);
	free_fn(treefname);
	write_codekd(treefid, codekd, index_scale);
	fprintf(stderr, "done.\n");
	fclose(treefid);


	free_kdtree(codekd);

	//basic_am_malloc_report();
	return (0);

}



codearray *readcodes(FILE *fid, qidx *numcodes, dimension *DimCodes,
                     double *index_scale, qidx buffsize)
{
	qidx ii;
	sidx numstars;
	codearray *thecodes = NULL;
	char readStatus;

	readStatus = read_code_header(fid, numcodes, &numstars, 
				      DimCodes, index_scale);
	if (readStatus == READ_FAIL)
		return ((codearray *)NULL);

	if (*numcodes < buffsize)
		buffsize = *numcodes;
	thecodes = mk_codearray(buffsize);

	for (ii = 0;ii < buffsize;ii++) {
		thecodes->array[ii] = mk_coded(*DimCodes);
		if (thecodes->array[ii] == NULL) {
			fprintf(stderr, "ERROR (codetree) -- out of memory at code %lu\n", ii);
			free_codearray(thecodes);
			return (codearray *)NULL;
		}
		// assert *DimCodes==DIM_CODES
		freadcode(thecodes->array[ii], fid);
	}
	return thecodes;
}


dimension readnextcode(FILE *codefid, code *tmpcode, dimension Dim_Codes)
{
	dimension rez = 0;
	//assert Dim_Codes==DIM_CODES
	rez = (dimension)freadcode(tmpcode, codefid);

	if (rez != Dim_Codes)
		fprintf(stderr, "ERROR (codetree) -- can't read next code\n");

	return rez;
}






