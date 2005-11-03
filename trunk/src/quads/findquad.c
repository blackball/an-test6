#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hf:q:i::"
const char HelpString[] =
    "findquad -f fname {-q quad# | -i starID}\n";

extern char *optarg;
extern int optind, opterr, optopt;

char *qidxfname = NULL;
char *quadfname = NULL;
char *codefname = NULL;
sidx thestar;
qidx thequad;
char starset = FALSE, quadset = FALSE, readstaridx = FALSE;
char buff[100], maxstarWidth;

int main(int argc, char *argv[])
{
	int argidx, argchar; //  opterr = 0;
	sidx ii, numstars, numstars2;
	qidx numquads, iA, iB, iC, iD, jj;
	dimension DimQuads;
	double index_scale, Cx, Cy, Dx, Dy;
	FILE *qidxfid = NULL, *quadfid = NULL, *codefid = NULL;
	sidx *starlist, *matchstar;
	qidx *starnumq;
	qidx **starquads;
	char readStatus;

	if (argc <= 3) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'f':
			qidxfname = mk_qidxfn(optarg);
			quadfname = mk_quadfn(optarg);
			codefname = mk_codefn(optarg);
			break;
		case 'i':
			if (optarg)
				thestar = strtoul(optarg, NULL, 0);
			else
				readstaridx = TRUE;
			starset = TRUE;
			break;
		case 'q':
			thequad = strtoul(optarg, NULL, 0);
			quadset = TRUE;
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

	if (starset == FALSE) {
		fprintf(stderr, "  Reading code/quad files...\n");
		fflush(stderr);
		fopenin(quadfname, quadfid);
		readStatus = read_quad_header(quadfid, &numquads, &numstars, 
					      &DimQuads, &index_scale);
		if (readStatus == READ_FAIL)
			return (1);
		fprintf(stderr, "    (%lu quads, %lu total stars, scale=%f arcmin)\n",
		        numquads, numstars, rad2arcmin(index_scale));
		if (quadset == TRUE) {
		  fseeko(quadfid, ftello(quadfid) + thequad*
					(DimQuads*sizeof(iA)), SEEK_SET);
		  readonequad(quadfid, &iA, &iB, &iC, &iD);
			fprintf(stderr, "quad %lu : A=%lu,B=%lu,C=%lu,D=%lu\n",
			        thequad, iA, iB, iC, iD);

			fopenin(codefname, codefid);
			readStatus = read_code_header(codefid, &numquads, 
						      &numstars, &DimQuads, 
						      &index_scale);
			if (readStatus == READ_FAIL)
				return (1);
			fseeko(codefid, ftello(codefid) + thequad*
					 (DIM_CODES*sizeof(Cx)), SEEK_SET);
			readonecode(codefid, &Cx, &Cy, &Dx, &Dy);
			fprintf(stderr, "     code = %lf,%lf,%lf,%lf\n", Cx, Cy, Dx, Dy);

		}
	}
	if (quadset == FALSE) {
		fprintf(stderr, "  Reading quad index...");
		fflush(stderr);
		fopenin(qidxfname, qidxfid);
		numstars2 = readquadidx(qidxfid, &starlist, &starnumq, &starquads);
		if (numstars2 == 0) {
			fprintf(stderr, "ERROR (findquad) -- out of memory\n");
			return (2);
		}
		fprintf(stderr, "  (%lu unique stars used in quads)\n", numstars2);
		fclose(qidxfid);
		if (starset == TRUE) {

			if (readstaridx) {
				char scanrez = 1;
				while (!feof(stdin) && scanrez) {
					scanrez = fscanf(stdin, "%lu", &thestar);
					matchstar = (sidx *)bsearch(&thestar, starlist, numstars2,
								    sizeof(sidx *), compare_sidx);
					if (matchstar == NULL)
						fprintf(stdout, "[]\n", thestar);
					else {
						ii = (sidx)(matchstar - starlist);
						fprintf(stdout, "[");
						for (jj = 0;jj < starnumq[ii];jj++)
							fprintf(stdout, "%lu, ", *(starquads[ii] + jj));
						fprintf(stdout, "]\n");
					}
					fflush(stdout);
				}
			} else {
				matchstar = (sidx *)bsearch(&thestar, starlist, numstars2,
							    sizeof(sidx *), compare_sidx);
				if (matchstar == NULL)
					fprintf(stderr, "no match for star %lu\n", thestar);
				else {
					ii = (sidx)(matchstar - starlist);
					fprintf(stderr, "star %lu appears in %lu quads:\n",
						starlist[ii], starnumq[ii]);
					for (jj = 0;jj < starnumq[ii];jj++)
						fprintf(stderr, "%lu ", *(starquads[ii] + jj));
					fprintf(stderr, "\n");
				}
			}

		}

		for (ii = 0;ii < numstars2;ii++)
			free(starquads[ii]);
		free(starquads);
		free(starnumq);
		free(starlist);
	}

	free_fn(codefname);
	free_fn(quadfname);
	free_fn(qidxfname);
	//basic_am_malloc_report();
	return (0);
}





