#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>

#include "healpix.h"
#include "starutil.h"
#include "codefile.h"
#include "fileutil.h"
#include "mathutil.h"
#include "quadfile.h"
#include "bl.h"
#include "catalog.h"
#include "tic.h"

#define OPTIONS "hf:u:l:n:o" // r

extern char *optarg;
extern int optind, opterr, optopt;

quadfile* quads = NULL;
codefile* codes = NULL;

catalog* cat;

// bounds of quad scale (in radians^2)
double quad_scale_upper2;
double quad_scale_lower2;

#define IL_BLOCKSIZE 50

int quadnum = 0;

void print_help(char* progname)
{
	printf("\nUsage:\n"
	       "  %s -f <filename-base>\n"
	       "     [-r]            re-bin the unused stars\n"
	       "     [-n <nside>]    healpix nside (default 512)\n"
	       "     [-u <scale>]    upper bound of quad scale (arcmin)\n"
	       "     [-l <scale>]    lower bound of quad scale (arcmin)\n"
	       "\n"
	       , progname);
}


// warning, you must guarantee iA<iB and iC<iD
void drop_quad(il* stars, int iA, int iB, int iC, int iD)
{
	int inA, inB, inC, inD;
	inA = iA;
	inB = iB;
	inC = iC;
	inD = iD;

	assert(iA < iB);
	assert(iC < iD);
	assert(il_size(stars) >= 4);
	assert(il_size(stars) > iA);
	assert(il_size(stars) > iB);
	assert(il_size(stars) > iC);
	assert(il_size(stars) > iD);
	assert(iA >= 0);
	assert(iB >= 0);
	assert(iC >= 0);
	assert(iD >= 0);

	il_remove(stars, iA);

	assert(iB >= 0);
	assert(iB - 1 < il_size(stars));

	il_remove(stars, iB - 1);
	if (inC > inA)
		iC--;
	if (inC > inB)
		iC--;

	assert(iC >= 0);
	assert(iC < il_size(stars));

	il_remove(stars, iC);
	if (inD > inA)
		iD--;
	if (inD > inB)
		iD--;

	assert(iD >= 0);
	assert(iD - 1 < il_size(stars));

	il_remove(stars, iD - 1);
}

int try_quads(int iA, int iB, int* iCs, int* iDs, int ncd,
              char* inbox, int maxind, il* stars,
              int* used_stars)
{
	int i;
	int iC = 0, iD;
	double Ax, Ay, Bx, By, Dx, Dy;
	double ABx, ABy, ADx, ADy;
	double cx = 0.0, cy = 0.0, dx = 0.0, dy = 0.0;
	double costheta, sintheta, scale;
	double *sA, *sB, *sD;
	double midAB[3];
	int ninbox = 0;
	uint staridA, staridB, staridC = 0, staridD;

	staridA = il_get(stars, iA);
	staridB = il_get(stars, iB);
	sA = catalog_get_star(cat, staridA);
	sB = catalog_get_star(cat, staridB);

	star_midpoint(midAB, sA, sB);
	star_coords(sA, midAB, &Ax, &Ay);
	star_coords(sB, midAB, &Bx, &By);

	ABx = Bx - Ax;
	ABy = By - Ay;

	scale = (ABx * ABx) + (ABy * ABy);

	if ((scale < quad_scale_lower2) ||
	        (scale > quad_scale_upper2))
		return 0;

	costheta = (ABx + ABy) / scale;
	sintheta = (ABy - ABx) / scale;

	// check which C, D points are inside the square.
	for (i = 0; i < maxind; i++) {
		if (!inbox[i])
			continue;

		iD = i;
		staridD = il_get(stars, i);
		sD = catalog_get_star(cat, staridD);
		star_coords(sD, midAB, &Dx, &Dy);
		ADx = Dx - Ax;
		ADy = Dy - Ay;
		dx = ADx * costheta + ADy * sintheta;
		dy = -ADx * sintheta + ADy * costheta;

		if ((dx >= 1.0) || (dx <= 0.0) ||
		        (dy >= 1.0) || (dy <= 0.0)) {
			continue;
		}

		ninbox++;
		if (ninbox == 1) {
			cx = dx;
			cy = dy;
			staridC = staridD;
			iC = iD;
		} else {
			codefile_write_code(codes, cx, cy, dx, dy);
            quadfile_write_quad(quads, staridA, staridB, staridC, staridD);
			quadnum++;
			drop_quad(stars, iA, iB, iC, iD);
			if (used_stars) {
				used_stars[0] = staridA;
				used_stars[1] = staridB;
				used_stars[2] = staridC;
				used_stars[3] = staridD;
			}
			return 1;
		}
	}
	return 0;
}

char find_a_quad(il* stars, int* used_stars)
{
	uint numxy, iA, iB, iC, iD, newpoint;
	int *iCs, *iDs;
	char *iunion;
	int ncd;

	numxy = il_size(stars);

	iCs = alloca(numxy * numxy * sizeof(int));
	iDs = alloca(numxy * numxy * sizeof(int));
	iunion = alloca(numxy * sizeof(char));

	/*
	  Each time through the "for" loop below, we consider a new
	  star ("newpoint").  First, we try building all quads that
	  have the new star on the diagonal (star B).  Then, we try
	  building all quads that have the star not on the diagonal
	  (star D).

	  In each of the loops, we first consider a pair of A, B stars,
	  and gather all the stars that could be C and D stars.  We add
	  the indices of the C and D stars to the arrays "iCs" and "iDs".
	  That is, iCs[0] and iDs[0] contain the first pair of C,D stars.
	  Both arrays can contain duplicates.

	  We also mark "iunion" of these indices to TRUE, so if index "i"
	  appears in "iCs" or "iDs", then "iunion[i]" is true.

	  We then call "try_quads" with stars A,B and the sets of stars
	  C,D.  For each element in "iunion" that is true, "try_quads"
	  checks whether that star is inside the box formed by AB; if not,
	  it marks iunion back to FALSE.  If then iterates through "iCs"
	  and "iDs", skipping any for which the corresponding "iunion"
	  element is FALSE.
	*/

	// We keep the invariants that iA < iB and iC < iD.
	// We try the A<->B and C<->D permutation in try_all_points.
	for (newpoint = 0; newpoint < numxy; newpoint++) {
		// quads with the new star on the diagonal:
		iB = newpoint;
		for (iA = 0; iA < newpoint; iA++) {
			ncd = 0;
			memset(iunion, 0, newpoint);
			for (iC = 0; iC < newpoint; iC++) {
				if ((iC == iA) || (iC == iB))
					continue;
				iunion[iC] = 1;
				for (iD = iC + 1; iD < newpoint; iD++) {
					if ((iD == iA) || (iD == iB))
						continue;
					iCs[ncd] = iC;
					iDs[ncd] = iD;
					ncd++;
				}
			}
			// note: "newpoint" is used as an upper-bound on the largest
			// TRUE element in "iunion".
			if (try_quads(iA, iB, iCs, iDs, ncd, iunion, newpoint, stars, used_stars)) {
				return 1;
			}
		}

		// quads with the new star not on the diagonal:
		iD = newpoint;
		for (iA = 0; iA < newpoint; iA++) {
			for (iB = iA + 1; iB < newpoint; iB++) {
				ncd = 0;
				memset(iunion, 0, newpoint + 1);
				iunion[iD] = 1;
				for (iC = 0; iC < newpoint; iC++) {
					if ((iC == iA) || (iC == iB))
						continue;
					iunion[iC] = 1;
					iCs[ncd] = iC;
					iDs[ncd] = iD;
					ncd++;
				}
				// note: "newpoint+1" is used because "iunion[newpoint]" is TRUE.
				if (try_quads(iA, iB, iCs, iDs, ncd, iunion, newpoint + 1, stars, used_stars)) {
					return 1;
				}
			}
		}
	}
	return 0;
}

void shifted_healpix_bin_stars(int numstars, il* starindices,
                               il* pixels, int dx, int dy,
                               int Nside)
{
	int i;
	int HEALPIXES = 12 * Nside * Nside;
	int offx, offy;

	printf("Binning stars into %i healpixes...\n", HEALPIXES);
	fflush(stdout);

	// the idea is that we look at a healpix grid that is three times
	// finer in both directions, and in each pass we choose of the nine
	// sub-pixels to be the center and we dump stars from a 3x3 grid around
	// the center into its coarser pixel.

	// (offx, offy) are the offsets you have to add to the sub-pixel
	// positions to determine which pixel they belong to.
	offx = (1 - dx);
	offy = (1 - dy);

	for (i = 0; i < numstars; i++) {
		double* starxyz;
		int hp;
		int ind;
		uint bighp, x, y;
		if (!(i % 100000)) {
			fprintf(stderr, ".");
			fflush(stderr);
		}
		if (!starindices)
			ind = i;
		else
			ind = il_get(starindices, i);

		starxyz = catalog_get_star(cat, ind);
		// note the Nside*3; this is the sub-pixel.
		hp = xyztohealpix_nside(starxyz[0], starxyz[1], starxyz[2], Nside*3);
		healpix_decompose(hp, &bighp, &x, &y, Nside*3);
		// now we compute which pixel this sub-pixel belongs to.
		if ((x == 0) || (x == (Nside*3-1)) ||
			(y == 0) || (y == (Nside*3-1))) {
			// this sub-pixel is on the border of its big healpix.
			// this happens rarely, so do a relatively expensive check:
			// just find its neighbours and take the first one that has the
			// right "dx" and "dy" values (ie, is the center sub-pixel in
			// this pass).
			uint nn, neigh[8];
			int j;
			nn = healpix_get_neighbours_nside(hp, neigh, Nside*3);
			for (j=0; j<nn; j++) {
				uint nx, ny, nbighp;
				healpix_decompose(neigh[j], &nbighp, &nx, &ny, Nside*3);
				if (((nx % 3) == dx) && ((ny % 3) == dy)) {
					// found the center pixel!
					// compute its corresponding normal (not sub-) pixel.
					hp = healpix_compose(nbighp, nx/3, ny/3, Nside*3);
					break;
				}
			}
		} else {
			x = (x + offx) / 3;
			y = (y + offy) / 3;
			// note Nside: this is a normal pixel, not a sub-pixel.
			hp = healpix_compose(bighp, x, y, Nside);
		}
		// append this star to the appropriate normal pixel list.
		il_append(pixels + hp, ind);
	}
	fprintf(stderr, "\n");
	fflush(stderr);
}

void create_quads_in_pixels(int numstars, il* starindices,
                            il* pixels, int Nside,
                            int dx, int dy)
{
	int i;
	int HEALPIXES = 12 * Nside * Nside;
	int Ninteresting = HEALPIXES;
	char* interesting;
	unsigned char* quadsmade;
	int passes = 0;
	int nused = 0;

	shifted_healpix_bin_stars(numstars, starindices, pixels, dx, dy, Nside);

	quadsmade = malloc(HEALPIXES * sizeof(unsigned char));
	memset(quadsmade, 0, HEALPIXES * sizeof(unsigned char));

	interesting = malloc(HEALPIXES * sizeof(char));
	memset(interesting, 0, HEALPIXES * sizeof(char));

	Ninteresting = 0;
	for (i=0; i<HEALPIXES; i++) {
		if (il_size(pixels + i)) {
			interesting[i] = 1;
			Ninteresting++;
		}
	}

	while (Ninteresting) {
		int nusedthispass;
		int grass = 0;
		int hp;
		nusedthispass = 0;
		for (hp=0; hp<HEALPIXES; hp++) {
			int foundone;
			int used_stars[4];
			if (!interesting[hp])
				continue;
			if (((grass++) % 100000) == 0) {
				fprintf(stderr, "+");
				fflush(stderr);
			}

			foundone = find_a_quad(pixels + hp, used_stars);
			if (foundone) {
				int s;
				// remove the used stars.
				for (s = 0; s < 4; s++)
					il_remove_value(pixels + hp, used_stars[s]);

				if (!il_size(pixels + hp)) {
					interesting[hp] = 0;
					Ninteresting--;
					continue;
				}

				quadsmade[hp]++;
				if (!quadsmade[hp]) {
					fprintf(stderr, "Warning, \"quadsmade\" counter overflowed for healpix %i.  Some of the stats may be wrong.\n", hp);
				}
				nused += 4;
				nusedthispass += 4;
			} else {
				interesting[hp] = 0;
				Ninteresting--;
			}
		}
		passes++;
		fprintf(stderr, "\nEnd of pass %i: ninteresting=%i, nused=%i this pass, %i total, of %i stars\n",
		        passes, Ninteresting, nusedthispass, nused, (int)numstars);
	}
	fprintf(stderr, "\n");
	fflush(stderr);

	fprintf(stderr, "Took %i passes.\n", passes);
	fprintf(stderr, "Made %i quads.\n", quadnum);
	fprintf(stderr, "Used %i stars.\n", nused);
	fprintf(stderr, "Didn't use %i stars.\n", (int)numstars - nused);

	/*
	  {
	  int maxmade = 0;
	  int* nmadehist;
	  for (i = 0; i < HEALPIXES; i++) {
	  if (quadsmade[i] > maxmade)
	  maxmade = quadsmade[i];
	  }
	  nmadehist = malloc((maxmade + 1) * sizeof(int));
	  memset(nmadehist, 0, (maxmade + 1)*sizeof(int));
	  for (i = 0; i < HEALPIXES; i++)
	  nmadehist[quadsmade[i]]++;
	  fprintf(stderr, "nmade=[");
	  for (i = 0; i <= maxmade; i++)
	  fprintf(stderr, "%i,", nmadehist[i]);
	  fprintf(stderr, "];\n");
	  free(nmadehist);
	  }
	  {
	  int maxmade = 0;
	  int* nmadehist;
	  for (i = 0; i < HEALPIXES; i++) {
	  if (il_size(pixels + i) > maxmade)
	  maxmade = il_size(pixels + i);
	  }
	  nmadehist = malloc((maxmade + 1) * sizeof(int));
	  memset(nmadehist, 0, (maxmade + 1)*sizeof(int));
	  for (i = 0; i < HEALPIXES; i++)
	  nmadehist[il_size(pixels + i)]++;
	  fprintf(stderr, "nleft=[");
	  for (i = 0; i <= maxmade; i++)
	  fprintf(stderr, "%i,", nmadehist[i]);
	  fprintf(stderr, "];\n");
	  free(nmadehist);
	  }
	*/

	free(interesting);
	free(quadsmade);
}


int main(int argc, char** argv)
{
	int argchar;
	char *quadfname = NULL;
	char *codefname = NULL;
	char* catfname;
	il* pixels;
	int Nside = 501;
	int HEALPIXES;
	int i;
	//int rebin = 0;
	char* basefname = NULL;
	uint pass, npasses;

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
			/*
			  case 'r':
			  rebin = 1;
			  break;
			*/
		case 'n':
			Nside = atoi(optarg);
			break;
		case 'h':
			print_help(argv[0]);
			exit(0);
		case 'f':
			basefname = optarg;
			break;
		case 'u':
			quad_scale_upper2 = atof(optarg);
			if (quad_scale_upper2 == 0.0) {
				printf("Couldn't parse desired scale \"%s\"\n", optarg);
				exit( -1);
			}
			quad_scale_upper2 = square(arcmin2rad(quad_scale_upper2));
			break;
		case 'l':
			quad_scale_lower2 = atof(optarg);
			quad_scale_lower2 = square(arcmin2rad(quad_scale_lower2));
			break;
		default:
			return -1;
		}

	if (!basefname) {
		printf("specify a catalogue file, bonehead.\n");
		print_help(argv[0]);
		exit( -1);
	}

	if (Nside % 3) {
		printf("Warning: to be really correct you should make Nside "
			   " (-n option) a multiple of three.");
	}

	HEALPIXES = 12 * Nside * Nside;
	{
		double hparea = 4.0 * M_PI * square(180.0 * 60.0 / M_PI) / (double)HEALPIXES;
		fprintf(stderr, "Nside=%i.  Number of healpixes=%i.  Healpix area = %g arcmin^2, length ~ %g arcmin.\n",
		        Nside, HEALPIXES, hparea, sqrt(hparea));
	}

	tic();

	pixels = malloc(HEALPIXES * sizeof(il));
	for (i = 0; i < HEALPIXES; i++) {
		il_new_existing(pixels + i, IL_BLOCKSIZE);
	}

	catfname = mk_catfn(basefname);
	printf("Reading catalog file %s ...\n", catfname);
	cat = catalog_open(catfname, 0);
	if (!cat) {
		fprintf(stderr, "Failed to open catalog file %s\n", catfname);
		exit( -1);
	}
	free_fn(catfname);
	printf("Catalog contains %i objects.\n", cat->numstars);

	quadfname = mk_quadfn(basefname);
	codefname = mk_codefn(basefname);

	printf("Writing quad file %s and code file %s\n", quadfname, codefname);

    quads = quadfile_open_for_writing(quadfname);
	if (!quads) {
		fprintf(stderr, "Couldn't open file %s to write quads.\n", quadfname);
		exit(-1);
	}
    if (quadfile_write_header(quads)) {
        fprintf(stderr, "Couldn't write headers to quads file %s\n", quadfname);
        exit(-1);
    }

    codes = codefile_open_for_writing(codefname);
	if (!codes) {
		fprintf(stderr, "Couldn't open file %s to write codes.\n", quadfname);
		exit(-1);
	}
    if (codefile_write_header(codes)) {
        fprintf(stderr, "Couldn't write headers to code file %s\n", codefname);
        exit(-1);
    }

	free_fn(quadfname);
	free_fn(codefname);
	quadfname = codefname = NULL;

    codes->numstars = cat->numstars;
    codes->index_scale       = sqrt(quad_scale_upper2);
    codes->index_scale_lower = sqrt(quad_scale_lower2);

    quads->numstars = cat->numstars;
    quads->index_scale       = sqrt(quad_scale_upper2);
    quads->index_scale_lower = sqrt(quad_scale_lower2);

	npasses = 9;
	for (pass = 0; pass < npasses; pass++) {
		int dx, dy;
		dx = pass % 3;
		dy = (pass / 3);

		fprintf(stderr, "Doing pass %i of %i: dx=%i, dy=%i.\n", pass, npasses, dx, dy);

		create_quads_in_pixels(cat->numstars, NULL, pixels, Nside, dx, dy);

		/*
		  Who knows if this works? 

		  if (rebin) {
		  // Gather up the leftover stars and re-bin.
		  il* leftovers = il_new(IL_BLOCKSIZE);
		  for (i = 0; i < HEALPIXES; i++) {
		  il_merge_lists(leftovers, pixels + i);
		  }
		  fprintf(stderr, "Rebinning with Nside=%i\n", Nside / 2);
		  create_quads_in_pixels(il_size(leftovers), leftovers, pixels, Nside / 2, dx, dy, );
		  il_free(leftovers);
		  }
		*/

		// empty blocklists.
		for (i = 0; i < HEALPIXES; i++)
			il_remove_all(pixels + i);
	}

	catalog_close(cat);

	// fix output file headers.
    quadfile_fix_header(quads);
	if (quadfile_close(quads)) {
		fprintf(stderr, "Couldn't write quad output file: %s\n", strerror(errno));
		exit( -1);
	}

	codefile_fix_header(codes);
    if (codefile_close(codes)) {
		fprintf(stderr, "Couldn't write code output file: %s\n", strerror(errno));
		exit( -1);
	}

	toc();

	fprintf(stderr, "Done.\n");
	fflush(stderr);

	free(pixels);

	return 0;
}

