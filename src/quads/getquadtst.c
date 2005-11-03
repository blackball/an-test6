#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "hf:"
const char HelpString[] =
    "getquadtst -f fname iA iB iC iD\n"
    "  print info about quad ABCD\n";

void check_quad(sidx iA, sidx iB, sidx iC, sidx iD);

extern char *optarg;
extern int optind, opterr, optopt;

char *catfname = NULL;
char buff[100], maxstarWidth;
off_t catposmarker = 0;
FILE *catfid = NULL;


int main(int argc, char *argv[])
{
	int argidx, argchar; //  opterr = 0;
	double ramin, ramax, decmin, decmax;
	dimension DimStars;
	sidx numstars,iA,iB,iC,iD;

	if (argc <= 6 || argc>7) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'f':
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

	iA= strtoul(argv[optind++], NULL, 0);
	iB= strtoul(argv[optind++], NULL, 0);
	iC= strtoul(argv[optind++], NULL, 0);
	iD= strtoul(argv[optind++], NULL, 0);


	if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}


	fopenin(catfname, catfid);
	free_fn(catfname);
	read_objs_header(catfid, &numstars, &DimStars,
						  &ramin, &ramax, &decmin, &decmax);
	catposmarker = ftello(catfid);

   check_quad(iA, iB, iC, iD);

	return (0);
}



void check_quad(sidx iA, sidx iB, sidx iC, sidx iD)
{
	double Ax, Ay, Bx, By, Cx, Cy, Dx, Dy;
	double scale, xxtmp, costheta, sintheta;
	star *sA = mk_star();
	star *sB = mk_star();
	star *sC = mk_star();
	star *sD = mk_star();
	star *midpoint = mk_star();

	//iA=9395286; iB=9400963; iC=9400958;iD=9400964;
   //9395286 9400963 9400958 9400964

	fseekocat(iA, catposmarker, catfid);
	freadstar(sA, catfid);
	fseekocat(iB, catposmarker, catfid);
	freadstar(sB, catfid);
	fseekocat(iC, catposmarker, catfid);
	freadstar(sC, catfid);
	fseekocat(iD, catposmarker, catfid);
	freadstar(sD, catfid);

	star_midpoint(midpoint, sA, sB);
	star_coords(sA,midpoint, &Ax, &Ay);
	star_coords(sB,midpoint, &Bx, &By);
	star_coords(sC,midpoint, &Cx, &Cy); 
	star_coords(sD,midpoint, &Dx, &Dy); 

	fprintf(stdout,"A=[%+.12lf,%+.12lf]\n",Ax,Ay);
	fprintf(stdout,"B=[%+.12lf,%+.12lf]\n",Bx,By);
	fprintf(stdout,"C=[%+.12lf,%+.12lf]\n",Cx,Cy);
	fprintf(stdout,"D=[%+.12lf,%+.12lf]\n",Dx,Dy);

	Bx -= Ax;	By -= Ay;
	Cx -= Ax;	Cy -= Ay;
	Dx -= Ax;	Dy -= Ay;
	scale = Bx * Bx + By * By;
	costheta = (Bx + By) / scale;
	sintheta = (By - Bx) / scale;
	fprintf(stdout,"scale=%lf; cost=%lf; sint=%lf\n",scale,costheta,sintheta);

	xxtmp = Cx;
	Cx = Cx * costheta + Cy * sintheta;
	Cy = -xxtmp * sintheta + Cy * costheta;

	xxtmp = Dx;
	Dx = Dx * costheta + Dy * sintheta;
	Dy = -xxtmp * sintheta + Dy * costheta;

	fprintf(stdout,"Cuv=[%+.12lf,%+.12lf]\n",Cx,Cy);
	fprintf(stdout,"Duv=[%+.12lf,%+.12lf]\n",Dx,Dy);

	free_star(midpoint);
	free_star(sA);	free_star(sB);	free_star(sC);	free_star(sD);

	return;
}



	//fprintf(stderr, "  Reading star KD tree from %s...", treefname);
	//fflush(stderr);
	//fopenin(treefname, treefid);
	//free_fn(treefname);
	//starkd = read_starkd(treefid, &ramin, &ramax, &decmin, &decmax);
	//fclose(treefid);
	//if (starkd == NULL)
	//	return (2);
	//numstars = starkd->root->num_points;

	//fprintf(stderr, "done\n    (%lu stars, %d nodes, depth %d).\n",
	//        numstars, starkd->num_nodes, starkd->max_depth);
	//fprintf(stderr, "    (dim %d) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
	//        kdtree_num_dims(starkd),
	//        rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));




