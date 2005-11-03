#include <errno.h>
#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "hf:ixrRt:k:"
const char HelpString[] =
    "findstar -f fname [-t dist | -k kNN] {-i idx | -x x y z | -r RA DEC}  OR\n"
    "findstar -f fname [-t dist | -k kNN] {-i | -x | -r | -R} then read stdin\n";

extern char *optarg;
extern int optind, opterr, optopt;

void output_star(FILE *fid, sidx i, stararray *sa);
void output_results(FILE *fid, kquery *kq, kdtree *starkd, star *thequery, stararray *thestars);

char *treefname = NULL, *catfname = NULL;
FILE *treefid = NULL, *catfid = NULL;
off_t catposmarker = 0;
char buff[100], oneobjWidth;

int main(int argc, char *argv[])
{
	int argidx, argchar; //  opterr = 0;
	char whichset = 0, xyzset = 0, radecset = 0, kset = 1, dtolset = 0, read_dtol = 0,readStatus;
	sidx K = 0;
	double dtol = 0.0;
	sidx numstars, whichstar;
	double xx, yy, zz, ra, dec, ramin, ramax, decmin, decmax;
	kquery *kq = NULL;
	star *thequery = NULL;
	kdtree *starkd = NULL;
	stararray *thestars = NULL;

	if (argc <= 3) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'k':
			K = strtoul(optarg, NULL, 0);
			if (K < 0)
				K = 0;
			kset = 1;
			dtolset = 0;
			break;
		case 't':
			dtol = strtod(optarg, NULL);
			if (dtol < 0.0)
				dtol = 0.0;
			dtolset = 1;
			kset = 0;
			break;
		case 'i':
			whichset = 1;
			xyzset = 0;
			radecset = 0;
			break;
		case 'x':
			xyzset = 1;
			whichset = 0;
			radecset = 0;
			break;
		case 'r':
			radecset = 1;
			whichset = 0;
			xyzset = 0;
			break;
		case 'R':
			radecset = 1;
			whichset = 0;
			xyzset = 0;
			read_dtol = 1;
			break;
		case 'f':
			treefname = mk_streefn(optarg);
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

	if (!xyzset && !radecset && !whichset) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	if (whichset && kset && (K == 0)) {
		dimension DimStars;
		fprintf(stderr, "# findstar: getting stars from %s\n", catfname);
		fopenin(catfname, catfid);
		free_fn(catfname);
		readStatus = read_objs_header(catfid, &numstars, &DimStars,
		                          &ramin, &ramax, &decmin, &decmax);
		if (readStatus == READ_FAIL)
			return (1);
		fprintf(stderr, "    (%lu stars) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
		        numstars, rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));
		catposmarker = ftello(catfid);
	} else {
		fprintf(stderr, "findstar: getting stars from %s\n", treefname);
		fprintf(stderr, "  Reading star KD tree...");
		fflush(stderr);
		fopenin(treefname, treefid);
		free_fn(treefname);
		starkd = read_starkd(treefid, &ramin, &ramax, &decmin, &decmax);
		fclose(treefid);
		if (starkd == NULL)
			return (2);
		numstars = starkd->root->num_points;
		fprintf(stderr, "done\n    (%lu stars, %d nodes, depth %d).\n",
		        numstars, starkd->num_nodes, starkd->max_depth);
		fprintf(stderr, "    (dim %d) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
		        kdtree_num_dims(starkd),
		        rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));

		thestars = (stararray *)mk_dyv_array_from_kdtree(starkd);

		thequery = mk_star();
	}

	if (!whichset && kset && (K <= 0))
		K = 1;
	if (kset && K > 0) {
		if (whichset && kset)
			kq = mk_kquery("knn", "", K + 1, KD_UNDEF, starkd->rmin);
		else
			kq = mk_kquery("knn", "", K, KD_UNDEF, starkd->rmin);
	} else if (dtolset)
		kq = mk_kquery("rangesearch", "", KD_UNDEF, dtol, starkd->rmin);

	//fprintf(stderr,"optind=%d,argc=%d\n",optind,argc);

	if (optind < argc) {
		argidx = optind;
		while (argidx < argc) {
			//fprintf(stderr,"argv[%d]=%s\n",argidx,argv[argidx]);
			//argidx++;
			if (whichset) {
				whichstar = strtoul(argv[argidx++], NULL, 0);
				//fprintf(stderr,"got idx:%lu\n",whichstar);
				if (whichstar < 0 || whichstar >= numstars)
					whichstar = 0;
				if (kset && K == 0) {
					//fprintf(stderr,"  getting star %lu\n",whichstar);
					output_star(stdout, whichstar, NULL);
				} else {
					if (kset)
						fprintf(stderr, "  getting %lu stars nearest %lu\n", K, whichstar);
					else if (dtolset)
						fprintf(stderr, "  getting stars within %lf of %lu\n", dtol, whichstar);
					thequery = thestars->array[whichstar];
					output_results(stdout, kq, starkd, thequery, thestars);
				}
			} else if (radecset) {
				if (argidx < (argc - 1)) {
					ra = strtod(argv[argidx++], NULL);
					dec = strtod(argv[argidx++], NULL);
					//fprintf(stderr,"got ra=%lf,dec=%lf\n",ra,dec);
					star_set(thequery, 0, radec2x(deg2rad(ra), deg2rad(dec)));
					star_set(thequery, 1, radec2y(deg2rad(ra), deg2rad(dec)));
					star_set(thequery, 2, radec2z(deg2rad(ra), deg2rad(dec)));
					if (kset)
						fprintf(stderr, "  getting %lu stars closest to ra=%lf,dec=%lf\n",
						        K, ra, dec);
					else if (dtolset)
						fprintf(stderr, "  getting stars within %lf of ra=%lf,dec=%lf\n",
						        dtol, ra, dec);
					output_results(stdout, kq, starkd, thequery, thestars);
				}
			} else { //xyzset
				if (argidx < (argc - 2)) {
					xx = strtod(argv[argidx++], NULL);
					yy = strtod(argv[argidx++], NULL);
					zz = strtod(argv[argidx++], NULL);
					//fprintf(stderr,"got x=%lf,y=%lf,z=%lf\n",xx,yy,zz);
					star_set(thequery, 0, xx);
					star_set(thequery, 1, yy);
					star_set(thequery, 2, zz);
					if (kset)
						fprintf(stderr, "  getting %lu stars closest to x=%lf,y=%lf,z=%lf\n",
						        K, xx, yy, zz);
					else if (dtolset)
						fprintf(stderr, " stars within %lf of x=%lf,y=%lf,z=%lf\n",
						        dtol, xx, yy, zz);

					output_results(stdout, kq, starkd, thequery, thestars);
				}
			}
		}
	} else {
		char scanrez = 1;
		while (!feof(stdin) && scanrez) {
			if (whichset) {
				scanrez = fscanf(stdin, "%lu", &whichstar);
				if (scanrez == 1) {
					//fprintf(stderr,"read idx:%lu\n",whichstar);
					if (whichstar < 0 || whichstar >= numstars)
						whichstar = 0;
					if (kset && K == 0) {
						//fprintf(stderr,"  getting star %lu\n",whichstar);
						output_star(stdout, whichstar, NULL);
					} else {
						if (kset)
							fprintf(stderr, "  getting %lu stars nearest %lu\n", K, whichstar);
						else if (dtolset)
							fprintf(stderr, "  getting stars within %lf of %lu\n", dtol, whichstar);
						thequery = thestars->array[whichstar];
						output_results(stdout, kq, starkd, thequery, thestars);
					}
				}
			} else if (radecset) {
				if (read_dtol) {
					scanrez = fscanf(stdin, "%lf %lf %lf", &ra, &dec, &dtol);

					if (kq)
						free_kquery(kq);
					kq = mk_kquery("rangesearch", "", KD_UNDEF, dtol, starkd->rmin);
				}
				else
					scanrez = fscanf(stdin, "%lf %lf", &ra, &dec);
				if ((scanrez == 2 && !read_dtol) || (scanrez ==
							3 && read_dtol)) {
					//fprintf(stderr,"read ra=%lf,dec=%lf\n",ra,dec);
					star_set(thequery, 0, radec2x(deg2rad(ra), deg2rad(dec)));
					star_set(thequery, 1, radec2y(deg2rad(ra), deg2rad(dec)));
					star_set(thequery, 2, radec2z(deg2rad(ra), deg2rad(dec)));
					if (kset)
						fprintf(stderr, "  getting %lu stars closest to ra=%lf,dec=%lf\n",
						        K, ra, dec);
					else if (dtolset)
						fprintf(stderr, "  getting stars within %lf of ra=%lf,dec=%lf\n",
						        dtol, ra, dec);
					output_results(stdout, kq, starkd, thequery, thestars);
					fflush(stdout);
				}
			} else { //xyzset
				scanrez = fscanf(stdin, "%lf %lf %lf", &xx, &yy, &zz);
				if (scanrez == 3) {
					//fprintf(stderr,"read x=%lf,y=%lf,z=%lf\n",xx,yy,zz);
					star_set(thequery, 0, xx);
					star_set(thequery, 1, yy);
					star_set(thequery, 2, zz);
					if (kset)
						fprintf(stderr, "  getting %lu stars closest to x=%lf,y=%lf,z=%lf\n",
						        K, xx, yy, zz);
					else if (dtolset)
						fprintf(stderr, " stars within %lf of x=%lf,y=%lf,z=%lf\n",
						        dtol, xx, yy, zz);

					output_results(stdout, kq, starkd, thequery, thestars);
				}
			}
		}
	}

	if (kq)
		free_kquery(kq);
	if (thequery)
		free_star(thequery);
	if (!whichset) {
		free_dyv_array_from_kdtree((dyv_array *)thestars);
		free_kdtree(starkd);
	}
	if (catfid)
		fclose(catfid);

	//basic_am_malloc_report();
	return (0);
}

void output_results(FILE* fid, kquery *kq, kdtree *starkd, star *thequery, stararray *thestars)
{
	int ii;
	kresult *krez = NULL;
	krez = mk_kresult_from_kquery(kq, starkd, thequery);
	fprintf(fid, "{\n");
	if (krez != NULL) {
		for (ii = 0;ii < krez->count;ii++)
			output_star(fid, krez->pindexes->iarr[ii], thestars);
		free_kresult(krez);
	}
	fprintf(fid, "}\n");
	fflush(stdin);
}


void output_star(FILE *fid, sidx i, stararray *sa)
{
	star *tmps = NULL;

	if (sa == NULL) {
		tmps = mk_star();
		if (tmps == NULL)
			return ;
		fseekocat(i, catposmarker, catfid);
		freadstar(tmps, catfid);
	} else
		tmps = sa->array[i];

	fprintf(fid, "%lu: ((%lf, %lf, %lf), (%lf, %lf)),\n",
			i, star_ref(tmps, 0), star_ref(tmps, 1), star_ref(tmps, 2),
			rad2deg(xy2ra(star_ref(tmps, 0), star_ref(tmps, 1))),
			rad2deg(z2dec(star_ref(tmps, 2))));

	if (sa == NULL)
		free_star(tmps);

	return ;
}
