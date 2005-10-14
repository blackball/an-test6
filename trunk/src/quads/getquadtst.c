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
char ASCII = 0;
char buff[100], maxstarWidth;


int main(int argc, char *argv[])
{
	int argidx, argchar; //  opterr = 0;
	qidx maxQuads = 0;
	double index_scale = DEFAULT_IDXSCALE;
	double ramin, ramax, decmin, decmax;
	FILE *treefid = NULL;
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
	numfound = get_quads(NULL,NULL, ASCII, thestars, starkd, index_scale,
	                     ramin, ramax, decmin, decmax, maxQuads, &numtries);
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
	sidx count;
	qidx ii, numfound;
	sidx iA, iB, iC, iD, jj, kk, numS;
	double Ax, Ay, Bx, By, Cx, Cy, Dx, Dy;
	double scale, thisx, thisy, xxtmp, costheta, sintheta;
	star *midpoint = mk_star();

	numfound = 0;
	(*numtries) = 0;
	for (ii = 0;ii<1;ii++) {
		{
			if (1) {
				iA = 9395286;
				star_coords(thestars->array[iA], thestars->array[iA], &Ax, &Ay);
				// Ax,Ay will be almost exactly zero for now
				{
					iB = 9400963;
					star_coords(thestars->array[iB], thestars->array[iA], &Bx, &By);
					Bx -= Ax;
					By -= Ay; // probably don't need this
					scale = Bx * Bx + By * By;
					if (1) {
						star_midpoint(midpoint, thestars->array[iA], thestars->array[iB]);
						star_coords(thestars->array[iA], midpoint, &Ax, &Ay);
						star_coords(thestars->array[iB], midpoint, &Bx, &By);
						Bx -= Ax;
						By -= Ay;
						scale = Bx * Bx + By * By;
						costheta = (Bx + By) / scale;
						sintheta = (By - Bx) / scale;
						fprintf(stdout,"scale=%lf\n cos=%lf \n sin=%lf\n",scale,costheta,sintheta);
						count = 0;
						{
							{
							  //iC=9400958;
							  iC=9400964;
							  // also try iD=9400964
							  star_coords(thestars->array[iC],midpoint, &thisx, &thisy); 
								thisx -= Ax;
								thisy -= Ay;

								fprintf(stdout,"A=[%.12lf,%.12lf]\n",Ax,Ay);
								fprintf(stdout,"Bbar=[%.12lf,%.12lf]\n",Bx,By);
								fprintf(stdout,"Cbar=[%.12lf,%.12lf]\n",thisx,thisy);

								xxtmp = thisx;
								thisx = thisx * costheta + thisy * sintheta;
								thisy = -xxtmp * sintheta + thisy * costheta;
								fprintf(stdout,"Just computed u=%lf,v=%lf\n",thisx,thisy);
								if ((thisx < 1.0) && (thisx > 0.0) && (thisy < 1.0) && (thisy > 0.0)) {
									count++;
								}
							}
						}
						/*if (0) {
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
							numfound++;

							} */
					}
				} // if scale and for jj
			} 
			(*numtries)++;
		} 
		if (is_power_of_two(numfound)) {
			fprintf(stderr, "  got %lu codes in %lu tries\r", numfound, *numtries);
			fflush(stderr);
		}
	}

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


