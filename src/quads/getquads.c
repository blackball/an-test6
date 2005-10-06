#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "habs:q:f:"
const char HelpString[] =
    "getquads -f fname -s scale(arcmin) -q numQuads [-a|-b]\n"
    "  scale is the scale to look for quads, numQuads is the #darts to throw\n"
    "  -a sets ASCII output, -b (default) sets BINARY output\n";


extern char *optarg;
extern int optind, opterr, optopt;

qidx get_quads(FILE *quadfid, FILE *codefid, char ASCII,
               stararray *thestars, kdtree *kd, double index_scale,
               double ramin, double ramax, double decmin, double decmax,
               qidx maxCodes, qidx *numtries);

void accept_quad(sidx iA, sidx iB, sidx iC, sidx iD,
                 double Cx, double Cy, double Dx, double Dy,
                 FILE *codefid, FILE *quadfid, char ASCII);


char *treefname = NULL;
char *quadfname = NULL;
char *codefname = NULL;
char ASCII = 0;
char buff[100], maxstarWidth;


int main(int argc, char *argv[])
{
	int argidx, argchar; //  opterr = 0;
	qidx maxQuads = 0;
	double index_scale = DEFAULT_IDXSCALE;
	double ramin, ramax, decmin, decmax;
	FILE *treefid = NULL, *quadfid = NULL, *codefid = NULL;
	sidx numstars;
	qidx numtries, numfound;
	kdtree *starkd = NULL;
	stararray *thestars = NULL;

	if (argc <= 6) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'a':
			ASCII = 1;
			break;
		case 'b':
			ASCII = 0;
			break;
		case 's':
			index_scale = (double)strtod(optarg, NULL);
			index_scale = arcmin2rad(index_scale);
			break;
		case 'q':
			maxQuads = strtoul(optarg, NULL, 0);
			break;
		case 'f':
			treefname = mk_streefn(optarg);
			quadfname = mk_quad0fn(optarg);
			codefname = mk_code0fn(optarg);
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

#define RANDSEED 888
	am_srand(RANDSEED);

	fprintf(stderr, "getquads: finding %lu quads in %s [RANDSEED=%d]\n",
	        maxQuads, treefname, RANDSEED);

	fprintf(stderr, "  Reading star KD tree from %s...", treefname);
	fflush(stderr);
	fopenin(treefname, treefid);
	free_fn(treefname);
	starkd = read_starkd(treefid, &ramin, &ramax, &decmin, &decmax);
	fclose(treefid);
	if (starkd == NULL)
		return (2);
	numstars = starkd->root->num_points;
	if (ASCII) {
		sprintf(buff, "%lu", numstars - 1);
		maxstarWidth = strlen(buff);
	}
	fprintf(stderr, "done\n    (%lu stars, %d nodes, depth %d).\n",
	        numstars, starkd->num_nodes, starkd->max_depth);
	fprintf(stderr, "    (dim %d) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
	        kdtree_num_dims(starkd),
	        rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));

	thestars = (stararray *)mk_dyv_array_from_kdtree(starkd);

	fprintf(stderr, "  Finding %lu quads at scale %g arcmin...(be patient)\n",
	        maxQuads, rad2arcmin(index_scale));
	fflush(stderr);
	fopenout(quadfname, quadfid);
	free_fn(quadfname);
	fopenout(codefname, codefid);
	free_fn(codefname);
	numfound = get_quads(quadfid, codefid, ASCII, thestars, starkd, index_scale,
	                     ramin, ramax, decmin, decmax, maxQuads, &numtries);
	fclose(quadfid);
	fclose(codefid);
	fprintf(stderr, "  got %lu codes in %lu tries.\n", numfound, numtries);

	free_dyv_array_from_kdtree((dyv_array *)thestars);
	free_kdtree(starkd);

	//basic_am_malloc_report();
	return (0);
}



qidx get_quads(FILE *quadfid, FILE *codefid, char ASCII,
               stararray *thestars, kdtree *kd, double index_scale,
               double ramin, double ramax, double decmin, double decmax,
               qidx maxCodes, qidx *numtries)
{
	char still_not_done;
	sidx count;
	qidx ii, numfound;
	sidx iA, iB, iC, iD, jj, kk, numS;
	double Ax, Ay, Bx, By, Cx, Cy, Dx, Dy;
	double scale, thisx, thisy, xxtmp, costheta, sintheta;
	star *midpoint = mk_star();
	kquery *kq = mk_kquery("rangesearch", "", KD_UNDEF, index_scale, kd->rmin);

	write_code_header(codefid, ASCII, maxCodes, (sidx)thestars->size,
	                  DIM_CODES, index_scale);
	write_quad_header(quadfid, ASCII, maxCodes, (sidx)thestars->size,
	                  DIM_QUADS, index_scale);

	numfound = 0;
	(*numtries) = 0;
	for (ii = 0;ii < maxCodes;ii++) {
		still_not_done = 1;
		while (still_not_done && ((*numtries) < DEFAULT_OVERSAMPLE*maxCodes)) {
			star *randstar = make_rand_star(ramin, ramax, decmin, decmax);
			// better to actively select...
			kresult *krez = mk_kresult_from_kquery(kq, kd, randstar);
			numS = (sidx)krez->count;
			//fprintf(stderr,"random location: %d within index_scale.\n",numS);
			if (numS >= DIM_QUADS) {
				ivec *candidates = mk_ivec(numS - 2);
				dyv *candidatesX = mk_dyv(numS - 2);
				dyv *candidatesY = mk_dyv(numS - 2);
				iA = (sidx)krez->pindexes->iarr[0];
				star_coords(thestars->array[iA], thestars->array[iA], &Ax, &Ay);
				// Ax,Ay will be almost exactly zero for now
				for (jj = (numS - 1);jj > 0 && still_not_done;jj--) { // try all iB's
					iB = (sidx)krez->pindexes->iarr[jj];
					star_coords(thestars->array[iB], thestars->array[iA], &Bx, &By);
					Bx -= Ax;
					By -= Ay; // probably don't need this
					scale = Bx * Bx + By * By;
					if (4.0*scale > (index_scale*index_scale)) { // ?? is this right?
						star_midpoint(midpoint, thestars->array[iA], thestars->array[iB]);
						star_coords(thestars->array[iA], midpoint, &Ax, &Ay);
						star_coords(thestars->array[iB], midpoint, &Bx, &By);
						Bx -= Ax;
						By -= Ay;
						scale = Bx * Bx + By * By;
						costheta = (Bx + By) / scale;
						sintheta = (By - Bx) / scale;
						count = 0;
						for (kk = 1;kk < numS;kk++) {
							if (kk != jj) {
								star_coords(thestars->array[krez->pindexes->iarr[kk]],
								            midpoint, &thisx, &thisy);
								thisx -= Ax;
								thisy -= Ay;
								xxtmp = thisx;
								thisx = thisx * costheta + thisy * sintheta;
								thisy = -xxtmp * sintheta + thisy * costheta;
								if ((thisx < 1.0) && (thisx > 0.0) && (thisy < 1.0) && (thisy > 0.0)) {
									ivec_set(candidates, count, krez->pindexes->iarr[kk]);
									dyv_set(candidatesX, count, thisx);
									dyv_set(candidatesY, count, thisy);
									count++;
								}
							}
						}
						if (count >= 2) {
							sidx i1 = (sidx)int_random(count);
							sidx i2 = (sidx)int_random(count - 1);
							if (i2 >= i1)
								i2++;
							//if(ivec_ref(candidates,i2)<ivec_ref(candidates,i1))
							//  {sidx itmp=i1; i1=i2; i2=itmp;}
							iC = (sidx)ivec_ref(candidates, i1);
							iD = (sidx)ivec_ref(candidates, i2);
							Cx = dyv_ref(candidatesX, i1);
							Cy = dyv_ref(candidatesY, i1);
							Dx = dyv_ref(candidatesX, i2);
							Dy = dyv_ref(candidatesY, i2);

							accept_quad(iA, iB, iC, iD, Cx, Cy, Dx, Dy, codefid, quadfid, ASCII);
							still_not_done = 0;
							numfound++;

						} // if count>=2
					}
				} // if scale and for jj
				free_ivec(candidates);
				free_dyv(candidatesX);
				free_dyv(candidatesY);
			} //if numS>=DIM_QUADS
			free_star(randstar);
			free_kresult(krez);
			(*numtries)++;
		} // while(still_not_done)
		if (is_power_of_two(numfound)) {
			fprintf(stderr, "  got %lu codes in %lu tries\r", numfound, *numtries);
			fflush(stderr);
		}
	}

	free_kquery(kq);
	free_star(midpoint);

	return numfound;
}



void accept_quad(sidx iA, sidx iB, sidx iC, sidx iD,
                 double Cx, double Cy, double Dx, double Dy,
                 FILE *codefid, FILE *quadfid, char ASCII)
{
	sidx itmp;
	double tmp;
	if (iC > iD) { // swap C and D if iC>iD, involves swapping Cxy/Dxy
		itmp = iC;
		iC = iD;
		iD = itmp;
		tmp = Cx;
		Cx = Dx;
		Dx = tmp;
		tmp = Cy;
		Cy = Dy;
		Dy = tmp;
	}
	if (iA > iB) { //swap A,B if iA>iB, involves C/Dxy->1-C/Dxy (??HOPE THIS IS OK)
		itmp = iA;
		iA = iB;
		iB = itmp;
		Cx = 1.0 - Cx;
		Cy = 1.0 - Cy;
		Dx = 1.0 - Dx;
		Dy = 1.0 - Dy;
	}

	if (ASCII) {
		fprintf(codefid, "%lf,%lf,%lf,%lf\n", Cx, Cy, Dx, Dy);
		fprintf(quadfid, "%2$*1$lu,%3$*1$lu,%4$*1$lu,%5$*1$lu\n",
		        maxstarWidth, iA, iB, iC, iD);
	} else {
		writeonecode(codefid, Cx, Cy, Dx, Dy);
		writeonequad(quadfid, iA, iB, iC, iD);
	}

	return ;
}


