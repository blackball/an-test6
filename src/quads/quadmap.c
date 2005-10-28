#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hf:"
const char HelpString[] =
    "quadmap -f fname\n";

extern char *optarg;
extern int optind, opterr, optopt;

char *quadfname = NULL;
char *catfname = NULL;
char *qmapfname = NULL;

int main(int argc, char *argv[])
{
	int argidx, argchar; //  opterr = 0;
	FILE *quadfid = NULL, *qmapfid = NULL, *catfid = NULL;
	qidx ii,iA,iB,iC,iD,numquads;
	sidx numstars;
	dimension DimStars,DimQuads;
	double index_scale,ramin,ramax,decmin,decmax;
	off_t catposmarker = 0;
	char qASCII,cASCII;
	star *tmps = NULL;

	if (argc <= 3) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'f':
			quadfname = mk_quadfn(optarg);
			qmapfname = mk_qmapfn(optarg);
			catfname = mk_catfn(optarg);
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

	fprintf(stderr, "quadmap: mapping quads in %s to %s\n", 
		quadfname,qmapfname);

	fopenin(quadfname, quadfid);
	qASCII = read_quad_header(quadfid,&numquads, &numstars, 
				  &DimQuads, &index_scale);
	if(qASCII == READ_FAIL) {
	  fprintf(stderr,"quadmap: read error on %s\n",quadfname);
	  return (1);
	}
	if(qASCII==1) {
	  fprintf(stderr,"quadmap: ascii not supported for %s\n",quadfname);
	  return (1);
	}
	fprintf(stderr, "    (%lu quads, %lu total stars, scale=%f arcmin)\n",
		numquads, numstars, rad2arcmin(index_scale));
	free_fn(quadfname);


	fopenin(catfname, catfid);
	cASCII = read_objs_header(catfid, &numstars, &DimStars,
				  &ramin, &ramax, &decmin, &decmax);
	if(cASCII == READ_FAIL) {
	  fprintf(stderr,"quadmap: read error on %s\n",catfname);
	  return (1);
	}
	if(cASCII==1) {
	  fprintf(stderr,"quadmap: ascii not supported for %s\n",catfname);
	  return (1);
	}
	fprintf(stderr, "  (%lu stars) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
   numstars, rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));
	catposmarker = ftello(catfid);
	free_fn(catfname);


	fopenout(qmapfname, qmapfid);
	free_fn(qmapfname);

	write_objs_header(qmapfid, 0, DIM_QUADS*numquads,DimStars, 
			  ramin,ramax,decmin,decmax);


	tmps = mk_star();
	for(ii=0;ii<numquads;ii++) {
	  readonequad(quadfid,&iA,&iB,&iC,&iD);
	  fseekocat(iA, catposmarker, catfid);
	  freadstar(tmps, catfid); fwritestar(tmps, qmapfid);
	  fseekocat(iB, catposmarker, catfid);
	  freadstar(tmps, catfid); fwritestar(tmps, qmapfid);
	  fseekocat(iC, catposmarker, catfid);
	  freadstar(tmps, catfid); fwritestar(tmps, qmapfid);
	  fseekocat(iD, catposmarker, catfid);
	  freadstar(tmps, catfid); fwritestar(tmps, qmapfid);
	}
	free_star(tmps);


	fclose(quadfid);
	fclose(qmapfid);
	fclose(catfid);
	//basic_am_malloc_report();
	return (0);
}





