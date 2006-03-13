#include "starutil_am.h"
#include "kdutil.h"
#include "fileutil_amkd.h"

#define OPTIONS "hpn:s:z:f:o:w:x:q:r:d:S:"
const char HelpString[] =
    "genfields -f fname -o fieldname {-n num_rand_fields | -r RA -d DEC}\n"
    "          -s scale(arcmin) [-p] [-w noise] [-x distractors] [-q dropouts] [-S seed]\n\n"
    "    -r RA -d DEC generates a single field centred at RA,DEC\n"
    "    -n N generates N randomly centred fields\n"
    "    -p flips parity, -q (default 0) sets the fraction of real stars removed\n"
    "    -x (default 0) sets the fraction of real stars added as random stars\n"
    "    -w (default 0) sets the fraction of scale by which to jitter positions\n"
    "    -S random seed\n";

extern char *optarg;
extern int optind, opterr, optopt;

qidx gen_pix(FILE *listfid, FILE *pix0fid, FILE *pixfid,
             stararray *thestars, kdtree *kd,
             double aspect, double noise, double distractors, double dropouts,
             double ramin, double ramax, double decmin, double decmax,
             double radscale, qidx numFields);

char *treefname = NULL, *listfname = NULL, *pix0fname = NULL, *pixfname = NULL;
char *rdlsfname = NULL;
FILE *rdlsfid = NULL;
char FlipParity = 0;

int RANDSEED = 777;
/* Number of times a field can fail to be created before bailing */
int FAILURES = 30;

int main(int argc, char *argv[])
{
	int argidx, argchar; //  opterr = 0;

	qidx numFields = 0;
	double radscale = 10.0, aspect = 1.0, distractors = 0.0, dropouts = 0.0, noise = 0.0;
	double centre_ra = 0.0, centre_dec = 0.0;
	FILE *treefid = NULL, *listfid = NULL, *pix0fid = NULL, *pixfid = NULL;
	qidx numtries;
	sidx numstars;
	double ramin, ramax, decmin, decmax;
	kdtree *starkd = NULL;
	stararray *thestars = NULL;

	if (argc <= 8) {
		fprintf(stderr, HelpString);
		return (HELP_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'S':
			RANDSEED = atoi(optarg);
			break;
		case 'n':
			numFields = strtoul(optarg, NULL, 0);
			break;
		case 'p':
			FlipParity = 1;
			break;
		case 's':
			radscale = (double)strtod(optarg, NULL);
			radscale = arcmin2rad(radscale);
			break;
		case 'z':
			aspect = strtod(optarg, NULL);
			break;
		case 'w':
			noise = strtod(optarg, NULL);
			break;
		case 'x':
			distractors = strtod(optarg, NULL);
			break;
		case 'q':
			dropouts = strtod(optarg, NULL);
			break;
		case 'r':
			centre_ra = deg2rad(strtod(optarg, NULL));
			break;
		case 'd':
			centre_dec = deg2rad(strtod(optarg, NULL));
			break;
		case 'f':
			treefname = mk_streefn(optarg);
			break;
		case 'o':
			listfname = mk_idlistfn(optarg);
			pix0fname = mk_field0fn(optarg);
			pixfname = mk_fieldfn(optarg);
			rdlsfname = mk_rdlsfn(optarg);
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

	am_srand(RANDSEED);

	if (numFields)
		fprintf(stderr, "genfields: making %lu random fields from %s [RANDSEED=%d]\n",
		        numFields, treefname, RANDSEED);
	else {
		fprintf(stderr, "genfields: making fields from %s around ra=%lf,dec=%lf\n",
		        treefname, centre_ra, centre_dec);
		numFields = 1;
	}

	fprintf(stderr, "  Reading star KD tree from %s...", treefname);
	fflush(stderr);
	fopenin(treefname, treefid);
	free_fn(treefname);
	starkd = read_starkd(treefid, &ramin, &ramax, &decmin, &decmax);
	fclose(treefid);
	if (starkd == NULL)
		return (1);
	numstars = starkd->root->num_points;
	fprintf(stderr, "done\n    (%lu stars, %d nodes, depth %d).\n",
	        numstars, starkd->num_nodes, starkd->max_depth);
	fprintf(stderr, "    (dim %d) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
	        kdtree_num_dims(starkd),
	        rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));

	thestars = (stararray *)mk_dyv_array_from_kdtree(starkd);

	if (numFields > 1)
		fprintf(stderr, "  Generating %lu fields at scale %g arcmin...\n",
		        numFields, rad2arcmin(radscale));
	fflush(stderr);
	fopenout(listfname, listfid);
	free_fn(listfname);
	fopenout(pix0fname, pix0fid);
	free_fn(pix0fname);
	fopenout(pixfname, pixfid);
	free_fn(pixfname);
	fopenout(rdlsfname, rdlsfid);
	free_fn(rdlsfname);
	if (numFields > 1)
		numtries = gen_pix(listfid, pix0fid, pixfid, thestars, starkd, aspect,
		                   noise, distractors, dropouts,
		                   ramin, ramax, decmin, decmax, radscale, numFields);
	else
		numtries = gen_pix(listfid, pix0fid, pixfid, thestars, starkd, aspect,
		                   noise, distractors, dropouts,
		                   centre_ra, centre_ra, centre_dec, centre_dec,
		                   radscale, numFields);
	fclose(listfid);
	fclose(pix0fid);
	fclose(pixfid);
	fclose(rdlsfid);
	if (numFields > 1)
		fprintf(stderr, "  made %lu nonempty fields in %lu tries.\n",
		        numFields, numtries);

	free_dyv_array_from_kdtree((dyv_array *)thestars);
	free_kdtree(starkd);

	//basic_am_malloc_report();
	return (0);
}



qidx gen_pix(FILE *listfid, FILE *pix0fid, FILE *pixfid,
             stararray *thestars, kdtree *kd,
             double aspect, double noise, double distractors, double dropouts,
             double ramin, double ramax, double decmin, double decmax,
             double radscale, qidx numFields)
{
	sidx jj, numS, numX;
	qidx numtries = 0, ii;
	double xx, yy;
	star *randstar;
	double scale = radscale2xyzscale(radscale);
	double pixxmin = 0, pixymin = 0, pixxmax = 0, pixymax = 0;
	kquery *kq = mk_kquery("rangesearch", "", KD_UNDEF, scale, kd->rmin);

	fprintf(pix0fid, "NumFields=%lu\n", numFields);
	fprintf(pixfid, "NumFields=%lu\n", numFields);
	fprintf(listfid, "NumFields=%lu\n", numFields);
	fprintf(rdlsfid, "NumFields=%lu\n", numFields);

	randstar = mk_star();

	for (ii = 0;ii < numFields;ii++) {
		kresult *krez = NULL;
		numS = 0;
		while (!numS) {
			double tmpstar[3];
			make_rand_star(tmpstar, ramin, ramax, decmin, decmax);
			star_set(randstar, 0, tmpstar[0]);
			star_set(randstar, 0, tmpstar[1]);
			star_set(randstar, 0, tmpstar[2]);
			krez = mk_kresult_from_kquery(kq, kd, randstar);

			numS = krez->count;
			//fprintf(stderr,"random location: %lu within scale.\n",numS);

			if (numS) {
				fprintf(pix0fid, "centre xyz=(%lf,%lf,%lf) radec=(%lf,%lf)\n",
						star_ref(randstar, 0), star_ref(randstar, 1), 
						  star_ref(randstar, 2),
						rad2deg(xy2ra(star_ref(randstar, 0), star_ref(randstar, 1))),
						rad2deg(z2dec(star_ref(randstar, 2))));

				numX = floor(numS * distractors);
				fprintf(pixfid, "%lu", numS + numX);
				fprintf(pix0fid, "%lu", numS);
				fprintf(listfid, "%lu", numS);
				fprintf(rdlsfid, "%lu", numS);
				for (jj = 0;jj < numS;jj++) {
					fprintf(listfid, ",%d", krez->pindexes->iarr[jj]);
					star_coords(thestars->array[(krez->pindexes->iarr[jj])],
					            randstar, &xx, &yy);

					{
						double x, y, z, ra, dec;
						x = star_ref(thestars->array[(krez->pindexes->iarr[jj])], 0);
						y = star_ref(thestars->array[(krez->pindexes->iarr[jj])], 1);
						z = star_ref(thestars->array[(krez->pindexes->iarr[jj])], 2);
						ra = rad2deg(xy2ra(x,y));
						dec = rad2deg(z2dec(z));
						fprintf(rdlsfid, ",%lf,%lf", ra, dec);
					}

					// should add random rotation here ???
					if (FlipParity) {
						double swaptmp = xx;
						xx = yy;
						yy = swaptmp;
					}
					if (jj == 0) {
						pixxmin = pixxmax = xx;
						pixymin = pixymax = yy;
					}
					if (xx > pixxmax)
						pixxmax = xx;
					if (xx < pixxmin)
						pixxmin = xx;
					if (yy > pixymax)
						pixymax = yy;
					if (yy < pixymin)
						pixymin = yy;
					fprintf(pix0fid, ",%lf,%lf", xx, yy);
					if (noise)
						fprintf(pixfid, ",%lf,%lf",
						        xx + noise*scale*gen_gauss(),
						        yy + noise*scale*gen_gauss() );
					else
						fprintf(pixfid, ",%lf,%lf", xx, yy);
				}
				for (jj = 0;jj < numX;jj++)
					fprintf(pixfid, ",%lf,%lf",
					        range_random(pixxmin, pixxmax),
					        range_random(pixymin, pixymax));

				fprintf(pixfid, "\n");
				fprintf(listfid, "\n");
				fprintf(pix0fid, "\n");
			}
			free_star(randstar);
			free_kresult(krez);
			numtries++;

			if (numtries >= FAILURES) {
				/* We've failed too many times; something is wrong. Bail
				 * gracefully. */
				fprintf(stderr, "  ERROR: Too many failures: %lu fails\n",
					numtries);
				exit(1);
			}
		}
		//if(is_power_of_two(ii))
		//fprintf(stderr,"  made %lu pix in %lu tries\r",ii,numtries);fflush(stderr);
	}

	free_star(randstar);

	free_kquery(kq);

	return numtries;
}
