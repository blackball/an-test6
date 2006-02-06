/**
 *   Solve fields (using Keir's kdtrees)
 *
 * Inputs: .ckdt2 .objs .quad
 * Output: .hits
 */

#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"

#include "blocklist.h"

#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"

#include "solver2.h"
#include "hitsfile.h"
#include "suspend.h"

#define OPTIONS "hpef:o:t:m:n:x:d:r:R:D:H:F:T:vS:a:b:"
const char HelpString[] =
"solvexy2 -f fname -o fieldname [-m agree_tol(arcsec)] [-t code_tol] [-p]\n"
"   [-n matches_needed_to_agree] [-x max_matches_needed]\n"
"   [-F maximum-number-of-field-objects-to-process]\n"
"   [-T maximum-number-of-field-quads-to-try]\n"
"   [-H hits-file-name.hits]\n"
"   [-r minimum-ra-for-debug-output]\n"
"   [-R maximum-ra-for-debug-output]\n"
"   [-d minimum-dec-for-debug-output]\n"
"   [-D maximum-dec-for-debug-output]\n"
"   [-S first-field]\n"
"   [-a resume (from) file]\n"
"   [-b suspend (to) file]\n"
"   -p flips parity\n"
"   code tol is the RADIUS (not diameter or radius^2) in 4d codespace\n";

#define DEFAULT_MIN_MATCHES_TO_AGREE 3
#define DEFAULT_MAX_MATCHES_NEEDED 8
#define DEFAULT_AGREE_TOL 7.0
#define DEFAULT_CODE_TOL .002
#define DEFAULT_PARITY_FLIP 0

qidx solve_fields(xyarray *thefields, int maxfieldobjs, int maxtries,
				  kdtree_t *codekd, double codetol);

blocklist* get_best_hits(blocklist* hitlist);

void debugging_gather_hits(blocklist* hitlist, blocklist* outputlist);

int find_correspondences(blocklist* hits, sidx* starids, sidx* fieldids,
						 int* p_ok);

void clear_hitlist(blocklist* hitlist);

void getquadids(qidx thisquad, sidx *iA, sidx *iB, sidx *iC, sidx *iD);
void getstarcoords(star *sA, star *sB, star *sC, star *sD,
                   sidx iA, sidx iB, sidx iC, sidx iD);


char *fieldfname = NULL, *treefname = NULL, *hitfname = NULL;
char *quadfname = NULL, *catfname = NULL;
FILE *hitfid = NULL, *quadfid = NULL, *catfid = NULL;
off_t qposmarker, cposmarker;

char *resumefname = NULL;
FILE* resumefid;
char *suspendfname = NULL;
FILE* suspendfid;

double* catalogue;
sidx*   quadindex;

bool use_mmap = TRUE;

// the largest star and quad available in the corresponding files.
sidx maxstar;
qidx maxquad;

char buff[100], maxstarWidth, oneobjWidth;

double AgreeArcSec = DEFAULT_AGREE_TOL;
double AgreeTol;
int mostAgree;
char ParityFlip = DEFAULT_PARITY_FLIP;
unsigned int min_matches_to_agree = DEFAULT_MIN_MATCHES_TO_AGREE;
unsigned int max_matches_needed = DEFAULT_MAX_MATCHES_NEEDED;

double DebuggingRAMin;
double DebuggingRAMax;
double DebuggingDecMin;
double DebuggingDecMax;
int Debugging = 0;

bool verbose = FALSE;

int nctrlcs = 0;
bool quitNow = FALSE;

int firstfield = 0;

extern char *optarg;
extern int optind, opterr, optopt;

void signal_handler(int sig);

int main(int argc, char *argv[]) {
    int argidx, argchar; //  opterr = 0;
    double codetol = DEFAULT_CODE_TOL;
    FILE *fieldfid = NULL, *treefid = NULL;
    qidx numfields, numquads, numsolved;
    sidx numstars;
    char readStatus;
    double index_scale, ramin, ramax, decmin, decmax;
    dimension Dim_Quads, Dim_Stars;
    xyarray *thefields = NULL;
    kdtree_t *codekd = NULL;
    int fieldobjs = 0;
    int fieldtries = 0;

    void*  mmapped_tree = NULL;
    size_t mmapped_tree_size = 0;
	void*  mmap_cat = NULL;
	size_t mmap_cat_size = 0;
	void*  mmap_quad = NULL;
	size_t mmap_quad_size = 0;

	off_t endoffset;
	hits_header hitshdr;

    if (argc <= 4) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'a':
			resumefname = optarg;
			break;
		case 'b':
			suspendfname = optarg;
			break;
		case 'S':
			firstfield = atoi(optarg);
			break;
		case 'v':
			verbose = TRUE;
			break;
		case 'd':
			Debugging++;
			DebuggingDecMin = strtod(optarg, NULL);
			break;
		case 'D':
			Debugging++;
			DebuggingDecMax = strtod(optarg, NULL);
			break;
		case 'r':
			Debugging++;
			DebuggingRAMin = strtod(optarg, NULL);
			break;
		case 'R':
			Debugging++;
			DebuggingRAMax = strtod(optarg, NULL);
			break;
		case 'F':
			fieldobjs = atoi(optarg);
			if (!fieldobjs) {
				fprintf(stderr, "Couldn't parse -F argument: %s\n",
						optarg);
				exit(-1);
			}
			break;
		case 'T':
			fieldtries = atoi(optarg);
			if (!fieldtries) {
				fprintf(stderr, "Couldn't parse -T argument: %s\n",
						optarg);
				exit(-1);
			}
			break;
		case 'H':
			if (hitfname) free_fn(hitfname);
			hitfname = strdup(optarg);
			break;
		case 'f':
			treefname = mk_ctree2fn(optarg);
			quadfname = mk_quadfn(optarg);
			catfname = mk_catfn(optarg);
			break;
		case 'o':
			fieldfname = mk_fieldfn(optarg);
			if (!hitfname)
				hitfname = mk_hitfn(optarg);
			break;
		case 't':
			codetol = strtod(optarg, NULL);
			break;
		case 'm':
			AgreeArcSec = strtod(optarg, NULL);
			break;
		case 'p':
			ParityFlip = 1 - ParityFlip;
			break;
		case 'n':
			min_matches_to_agree = (unsigned int) strtol(optarg, NULL, 0);
			break;
		case 'x':
			max_matches_needed = (unsigned int) strtol(optarg, NULL, 0);
			break;
		case '?':
			fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		case 'h':
			fprintf(stderr, HelpString);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}

    if (Debugging != 0 && Debugging != 4) {
		fprintf(stderr, "When debugging need all of -d -D -r -R\n");
		return (OPT_ERR);
    }

    if (treefname == NULL || fieldfname == NULL || codetol < 0) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
    }

    if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		fprintf(stderr, HelpString);
		return (OPT_ERR);
    }
	if (resumefname && suspendfname && (strcmp(resumefname, suspendfname) == 0)) {
		fprintf(stderr, "Suspend and resume file can't be the same.\n");
		exit(-1);
	}


	/*
	  fprintf(stderr, "sizeof(int)    = %i\n", sizeof(int));
	  fprintf(stderr, "sizeof(long)   = %i\n", sizeof(long));
	  fprintf(stderr, "sizeof(int*)   = %i\n", sizeof(int*));
	  fprintf(stderr, "sizeof(sidx)   = %i\n", sizeof(sidx));
	  fprintf(stderr, "sizeof(qidx)   = %i\n", sizeof(qidx));
	  fprintf(stderr, "sizeof(double) = %i\n", sizeof(double));
	*/

    fprintf(stderr, "solvexy2: solving fields in %s using %s\n",
			fieldfname, treefname);

	// Read .xyls file...
    fprintf(stderr, "  Reading fields...");
    fflush(stderr);
    fopenin(fieldfname, fieldfid);
    thefields = readxy(fieldfid, ParityFlip);
    fclose(fieldfid);
    if (thefields == NULL)
		return (1);
    numfields = (qidx)thefields->size;
    fprintf(stderr, "got %lu fields.\n", numfields);
    if (ParityFlip)
		fprintf(stderr, "  Flipping parity (swapping row/col image coordinates).\n");


	// Read .ckdt2 file...
    fprintf(stderr, "  Reading code KD tree from %s...", treefname);
    fflush(stderr);
    fopenin(treefname, treefid);
    codekd = kdtree_read(treefid, use_mmap, &mmapped_tree, &mmapped_tree_size);
    if (!codekd)
		return (2);
    fclose(treefid);
    fprintf(stderr, "done\n    (%d quads, %d nodes, dim %d).\n",
			codekd->ndata, codekd->nnodes, codekd->ndim);


	// Read .quad file...
    fopenin(quadfname, quadfid);
    free_fn(quadfname);
    readStatus = read_quad_header(quadfid, &numquads, &numstars, 
								  &Dim_Quads, &index_scale);
    if (readStatus == READ_FAIL)
		return (3);
    qposmarker = ftello(quadfid);
	// check that the quads file is the right size.
	fseeko(quadfid, 0, SEEK_END);
	endoffset = ftello(quadfid) - qposmarker;
	maxquad = endoffset / (DIM_QUADS * sizeof(sidx));
	if (maxquad != numquads) {
		fprintf(stderr, "Error: numquads=%li (specified in .quad file header) does "
				"not match maxquad=%li (computed from size of .quad file).\n",
				numquads, maxquad);
		exit(-1);
	}
	if (use_mmap) {
		mmap_quad_size = endoffset;
		mmap_quad = mmap(0, mmap_quad_size, PROT_READ, MAP_SHARED,
						 fileno(quadfid), 0);
		if (mmap_quad == MAP_FAILED) {
			fprintf(stderr, "Failed to mmap quad file: %s\n", strerror(errno));
			exit(-1);
		}
		fclose(quadfid);
		quadindex = (sidx*)(((char*)(mmap_quad)) + qposmarker);
	}


	// Read .objs file...
    fopenin(catfname, catfid);
    free_fn(catfname);
    readStatus = read_objs_header(catfid, &numstars, &Dim_Stars,
								  &ramin, &ramax, &decmin, &decmax);
    if (readStatus == READ_FAIL) {
		exit(-1);
	}
    cposmarker = ftello(catfid);
	// check that the catalogue file is the right size.
	fseeko(catfid, 0, SEEK_END);
	endoffset = ftello(catfid) - cposmarker;
	maxstar = endoffset / (DIM_STARS * sizeof(double));
	if (maxstar != numstars) {
		fprintf(stderr, "Error: numstars=%li (specified in .objs file header) does "
				"not match maxstars=%li (computed from size of .objs file).\n",
				numstars, maxstar);
		exit(-1);
	}
	if (use_mmap) {
		mmap_cat_size = endoffset;
		mmap_cat = mmap(0, mmap_cat_size, PROT_READ, MAP_SHARED, fileno(catfid), 0);
		if (mmap_cat == MAP_FAILED) {
			fprintf(stderr, "Failed to mmap catalogue file: %s\n", strerror(errno));
			exit(-1);
		}
		fclose(catfid);
		catalogue = (double*)(((char*)(mmap_cat)) + cposmarker);
	}

    AgreeTol = sqrt(2.0) * radscale2xyzscale(arcsec2rad(AgreeArcSec));
    fprintf(stderr, "  Solving %lu fields (code_match_tol=%lg,agreement_tol=%lg arcsec)...\n",
			numfields, codetol, AgreeArcSec);


	if (resumefname) {
		// read resume file.
		double index_scale;
		char oldfieldname[256];
		char oldtreename[256];
		uint nfields;
		fopenin(resumefname, resumefid);
		if (suspend_read_header(resumefid, &index_scale, oldfieldname, oldtreename, &nfields)) {
			fprintf(stderr, "Couldn't read resume file %s: %s\n", resumefname, strerror(errno));
			exit(-1);
		}
	}
	if (suspendfname) {
		fopenout(suspendfname, suspendfid);
		suspend_write_header(suspendfid, index_scale, fieldfname,
							 treefname, dyv_array_size(thefields));
	}


	// write HITS header.
    fprintf(stderr, "Using HITS output file %s\n", hitfname);
    fopenout(hitfname, hitfid);
    free_fn(hitfname);
	hits_header_init(&hitshdr);
	hitshdr.field_file_name = fieldfname;
	hitshdr.tree_file_name = treefname;
	hitshdr.nfields = numfields;
	hitshdr.ncodes = codekd->ndata;
	hitshdr.nstars = numstars;
	hitshdr.codetol = codetol;
	hitshdr.agreetol = AgreeArcSec;
	hitshdr.parity = ParityFlip;
	hitshdr.min_matches_to_agree = min_matches_to_agree;
	hitshdr.max_matches_needed = max_matches_needed;
	hits_write_header(hitfid, &hitshdr);
    free_fn(fieldfname);
    free_fn(treefname);


    signal(SIGINT, signal_handler);


    numsolved = solve_fields(thefields, fieldobjs, fieldtries,
							 codekd, codetol);
    fprintf(stderr, "\nDone (solved %lu).\n", numsolved);


	// finish up HITS file...
	hits_write_tailer(hitfid);
    fclose(hitfid);

	// clean up...
	if (resumefid)
		fclose(resumefid);
	if (suspendfid)
		fclose(suspendfid);
    free_xyarray(thefields);
    if (use_mmap) {
		munmap(mmapped_tree, mmapped_tree_size);
		free(codekd);
		munmap(mmap_quad, mmap_quad_size);
		munmap(mmap_cat, mmap_cat_size);
    } else {
		kdtree_free(codekd);
		fclose(quadfid);
		fclose(catfid);
    }

    return 0;
}


void signal_handler(int sig) {
    if (sig != SIGINT) return;
	nctrlcs++;
	switch (nctrlcs) {
	case 1:
		fprintf(stderr, "Received signal SIGINT - stopping ASAP...\n");
		break;
	case 2:
		fprintf(stderr, "Hit ctrl-C again to hard-quit - warning, hits file won't be written.\n");
		break;
	default:
		exit(-1);
	}
    quitNow = TRUE;
}

int numtries, nummatches;

/*
  void print_abcdpix(xy *abcd) {
  fprintf(stderr, "ABCDpix=[%8g, %8g, %8g, %8g, %8g, %8g, %8g, %8g]\n",
  abcd->farr[0], abcd->farr[1], abcd->farr[2], abcd->farr[3],
  abcd->farr[4], abcd->farr[5], abcd->farr[6], abcd->farr[7]);
  }
*/

qidx solve_fields(xyarray *thefields, int maxfieldobjs, int maxtries,
				  kdtree_t *codekd, double codetol) {
	uint resume_fieldnum;
	uint resume_nobjs;
	uint resume_ntried;
	blocklist* resume_hits = NULL;

    qidx numsolved, ii;
    sidx numxy;
    xy *thisfield;
    xy *cornerpix = mk_xy(2);
	blocklist* hitlist = blocklist_pointer_new(256);

    numsolved = 0;
    for (ii=firstfield; ii< dyv_array_size(thefields); ii++) {
		hits_field fieldhdr;
		blocklist* bestlist;
		blocklist* allocated_list = NULL;
		sidx* starids;
		sidx* fieldids;
		int correspond_ok = 1;
		int Ncorrespond;
		int i, bestnum;
		sidx startobject;
		int objsused;

		numtries = 0;
		nummatches = 0;
		mostAgree = 0;
		thisfield = xya_ref(thefields, ii);
		numxy = xy_size(thisfield);
		startobject = 3;

		if (resumefid && !resume_hits) {
			blocklist* slist = blocklist_pointer_new(256);
			// read the next suspended record from the file...
			if (suspend_read_field(resumefid, &resume_fieldnum, &resume_nobjs, &resume_ntried, slist)) {
				fprintf(stderr, "Couldn't read a suspended field: %s\n", strerror(errno));
				blocklist_pointer_free(slist);
				fclose(resumefid);
				resumefid = NULL;
			} else {
				resume_hits = slist;
			}
		}

		if (resume_hits && (ii == resume_fieldnum)) {
			int i;
			int nhits;
			// Resume where we left off...
			numtries = resume_ntried;
			startobject = resume_nobjs;
			for (i=0; i<blocklist_count(resume_hits); i++) {
				MatchObj* mo = (MatchObj*)blocklist_pointer_access(resume_hits, i);
				solver_add_hit(hitlist, mo, AgreeTol);
			}
			nhits = blocklist_count(resume_hits);
			blocklist_pointer_free(resume_hits);
			resume_hits = NULL;
			fprintf(stderr, "Resuming at field object %i (%i quads tried, %i hits found so far)\n",
					(int)startobject, (int)numtries, nhits);
		}

		if (!thisfield) {
			clear_hitlist(hitlist);
			continue;
		}

		solve_field(thisfield, startobject, maxfieldobjs, maxtries,
					max_matches_needed, codekd, codetol, hitlist,
					&quitNow, AgreeTol, &numtries, &nummatches,
					&mostAgree, cornerpix, &objsused);

		if (suspendfid) {
			int i, j, M, N;
			blocklist* hits = blocklist_pointer_new(256);
			N = blocklist_count(hitlist);
			for (i=0; i<N; i++) {
				blocklist* lst = (blocklist*)blocklist_pointer_access(hitlist, i);
				M = blocklist_count(lst);
				for (j=0; j<M; j++) {
					MatchObj* mo = (MatchObj*)blocklist_pointer_access(lst, j);
					blocklist_pointer_append(hits, mo);
				}
			}
			suspend_write_field(suspendfid, (uint)ii, objsused, (uint)numtries, hits);
			blocklist_free(hits);
		}


		bestlist = get_best_hits(hitlist);

		if (!bestlist)
			bestnum = 0;
		else
			bestnum = blocklist_count(bestlist);

		if (bestnum < min_matches_to_agree) {
			if (Debugging) {
				bestlist = blocklist_pointer_new(256);
				allocated_list = bestlist;
				debugging_gather_hits(hitlist, bestlist);
			}
		} else {
			numsolved++;
		}

		if (bestlist)
			bestnum = blocklist_count(bestlist);

		hits_field_init(&fieldhdr);
		fieldhdr.user_quit = quitNow;
		fieldhdr.field = ii;
		fieldhdr.objects_in_field = numxy;
		fieldhdr.objects_examined = objsused;
		fieldhdr.field_corners = cornerpix;
		fieldhdr.ntries = numtries;
		fieldhdr.nmatches = nummatches;
		fieldhdr.nagree = bestnum;
		fieldhdr.parity = ParityFlip;
		hits_write_field_header(hitfid, &fieldhdr);

		fprintf(stderr, "\n    field %lu: tried %i quads, matched %i codes, "
				"%i agree\n", ii, numtries, nummatches, bestnum);

		if (bestnum < min_matches_to_agree) {
			hits_write_hit(hitfid, NULL);
			hits_write_field_tailer(hitfid);
			fflush(hitfid);
			continue;
		}

		for (i=0; i<bestnum; i++) {
			MatchObj* mo = (MatchObj*)blocklist_pointer_access(bestlist, i);
			hits_write_hit(hitfid, mo);
		}

		starids  = (sidx*)malloc(bestnum * 4 * sizeof(sidx));
		fieldids = (sidx*)malloc(bestnum * 4 * sizeof(sidx));
		Ncorrespond = find_correspondences(bestlist, starids, fieldids, &correspond_ok);

		hits_write_correspondences(hitfid, starids, fieldids, Ncorrespond, correspond_ok);

		hits_write_field_tailer(hitfid);
		fflush(hitfid);

		free(starids);
		free(fieldids);
		if (allocated_list)
			blocklist_pointer_free(allocated_list);

		clear_hitlist(hitlist);
		//quitNow = false;
    }

    free_xy(cornerpix);
	blocklist_free(hitlist);

    return numsolved;
}

int inrange(double ra, double ralow, double rahigh) {
    if (ralow < rahigh) {
		if (ra > ralow && ra < rahigh)
            return 1;
        return 0;
    }

    /* handle wraparound properly */
    if (ra < ralow && ra > rahigh)
        return 1;
    return 0;
}

void debugging_gather_hits(blocklist* hitlist, blocklist* outputlist) {
	int i, N;

	N = blocklist_count(hitlist);
	// We're debugging.  Gather all hits within our RA/DEC range.
	for (i=0; i<N; i++) {
		int j, M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hitlist, i);
		M = blocklist_count(hits);
		for (j=0; j<M; j++) {
			double minra, maxra, mindec, maxdec;
			double x, y, z;
			MatchObj* mo = (MatchObj*)blocklist_pointer_access(hits, j);
			x = dyv_ref(mo->sMin, 0);
			y = dyv_ref(mo->sMin, 1);
			z = dyv_ref(mo->sMin, 2);
			minra = xy2ra(x, y);
			mindec = z2dec(z);
			x = dyv_ref(mo->sMax, 0);
			y = dyv_ref(mo->sMax, 1);
			z = dyv_ref(mo->sMax, 2);
			maxra = xy2ra(x, y);
			maxdec = z2dec(z);

			// convert to degrees - Debugging{RA,Dec}{Min,Max} are specified in degrees.
			minra  *= 180/M_PI;
			mindec *= 180/M_PI;
			maxra  *= 180/M_PI;
			maxdec *= 180/M_PI;

			// If any of the corners are in the range, go for it
			if ( (inrange(mindec, DebuggingDecMin, DebuggingDecMax) &&
				  inrange(minra, DebuggingRAMin, DebuggingRAMax)) ||
				 (inrange(maxdec, DebuggingDecMin, DebuggingDecMax) &&
				  inrange(maxra, DebuggingRAMin, DebuggingRAMax)) ||
				 (inrange(mindec, DebuggingDecMin, DebuggingDecMax) &&
				  inrange(maxra, DebuggingRAMin, DebuggingRAMax)) ||
				 (inrange(maxdec, DebuggingDecMin, DebuggingDecMax) &&
				  inrange(minra, DebuggingRAMin, DebuggingRAMax))) {

				blocklist_pointer_append(outputlist, mo);
			}
		}
	}
}


blocklist* get_best_hits(blocklist* hitlist) {
    int i, N;
    int bestnum;
	blocklist* bestlist;

	bestnum = 0;
	bestlist = NULL;
    N = blocklist_count(hitlist);
	//fprintf(stderr, "radecs=[");
    for (i=0; i<N; i++) {
		int M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hitlist, i);
		M = blocklist_count(hits);
		if (M > bestnum) {
			bestnum = M;
			bestlist = hits;
		}
		/*
		  if ((M >= min_matches_to_agree) ||
		  0) {
		  double ra1, dec1;
		  double ra2, dec2;
		  int lim = 1, j;
		  //if (verbose)
		  lim=M;
		  for (j=0; j<lim; j++) {
		  MatchObj* mo = (MatchObj*)blocklist_pointer_access(hits, j);
		  ra1  = xy2ra(mo->vector[0], mo->vector[1]);
		  dec1 = z2dec(mo->vector[2]);
		  ra2  = xy2ra(mo->vector[3], mo->vector[4]);
		  dec2 = z2dec(mo->vector[5]);
		  ra1  *= 180.0/M_PI;
		  dec1 *= 180.0/M_PI;
		  ra2  *= 180.0/M_PI;
		  dec2 *= 180.0/M_PI;
		  //fprintf(stderr, "%.12g,%.12g,%.12g,%.12g;", ra1, dec1, ra2, dec2);
		  //fprintf(stderr, "Match list %i: %i hits: ra,dec (%g, %g)\n", i, M, ra1, dec1);
		  //fprintf(stderr, "Match list %i: %i hits: ra,dec (%g, %g)\n", i, M, ra1, dec1);
		  }
		  }
		*/
	}
	//fprintf(stderr, "];\n");
	return bestlist;
}

inline void add_correspondence(sidx* starids, sidx* fieldids,
							   sidx starid, sidx fieldid,
							   int* p_nids, int* p_ok) {
	int i;
	int ok = 1;
	for (i=0; i<(*p_nids); i++) {
		if ((starids[i] == starid) &&
			(fieldids[i] == fieldid)) {
			return;
		} else if ((starids[i] == starid) ||
				   (fieldids[i] == fieldid)) {
			ok = 0;
		}
	}
	starids[*p_nids] = starid;
	fieldids[*p_nids] = fieldid;
	(*p_nids)++;
	if (p_ok && !ok) *p_ok = 0;
}

int find_correspondences(blocklist* hits, sidx* starids, sidx* fieldids,
						 int* p_ok) {
	int i, N;
	int M;
	int ok = 1;
	MatchObj* mo;
	if (!hits) return 0;
	N = blocklist_count(hits);
	M = 0;
	for (i=0; i<N; i++) {
		mo = (MatchObj*)blocklist_pointer_access(hits, i);
		add_correspondence(starids, fieldids, mo->fA, mo->iA, &M, &ok);
		add_correspondence(starids, fieldids, mo->fB, mo->iB, &M, &ok);
		add_correspondence(starids, fieldids, mo->fC, mo->iC, &M, &ok);
		add_correspondence(starids, fieldids, mo->fD, mo->iD, &M, &ok);
	}
	if (p_ok && !ok) *p_ok = 0;
	return M;
}


void clear_hitlist(blocklist* hitlist) {
    int i, N;
    N = blocklist_count(hitlist);
    for (i=0; i<N; i++) {
		int j, M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hitlist, i);
		M = blocklist_count(hits);
		for (j=0; j<M; j++) {
			MatchObj* mo = (MatchObj*)blocklist_pointer_access(hits, j);

			free_star(mo->sMin);
			free_star(mo->sMax);
			free_MatchObj(mo);
		}
		blocklist_pointer_free(hits);
    }
    blocklist_remove_all(hitlist);
}

void getquadids(qidx thisquad, sidx *iA, sidx *iB, sidx *iC, sidx *iD) {
	if (thisquad >= maxquad) {
		fprintf(stderr, "thisquad %lu >= maxquad %lu\n", thisquad, maxquad);
	}
	if (use_mmap) {

		*iA = quadindex[thisquad*DIM_QUADS + 0];
		*iB = quadindex[thisquad*DIM_QUADS + 1];
		*iC = quadindex[thisquad*DIM_QUADS + 2];
		*iD = quadindex[thisquad*DIM_QUADS + 3];

	} else {
		fseeko(quadfid, qposmarker + thisquad * DIM_QUADS * sizeof(sidx), SEEK_SET);
		readonequad(quadfid, iA, iB, iC, iD);
	}
}

void getstarcoords(star *sA, star *sB, star *sC, star *sD,
                   sidx iA, sidx iB, sidx iC, sidx iD)
{
	if (iA >= maxstar) {
		fprintf(stderr, "iA %lu > maxstar %lu\n", iA, maxstar);
	}
	if (iB >= maxstar) {
		fprintf(stderr, "iB %lu > maxstar %lu\n", iB, maxstar);
	}
	if (iC >= maxstar) {
		fprintf(stderr, "iC %lu > maxstar %lu\n", iC, maxstar);
	}
	if (iD >= maxstar) {
		fprintf(stderr, "iD %lu > maxstar %lu\n", iD, maxstar);
	}

	if (use_mmap) {

		memcpy(sA->farr, catalogue + iA * DIM_STARS, DIM_STARS * sizeof(double));
		memcpy(sB->farr, catalogue + iB * DIM_STARS, DIM_STARS * sizeof(double));
		memcpy(sC->farr, catalogue + iC * DIM_STARS, DIM_STARS * sizeof(double));
		memcpy(sD->farr, catalogue + iD * DIM_STARS, DIM_STARS * sizeof(double));

	} else {
		fseekocat(iA, cposmarker, catfid);
		freadstar(sA, catfid);
		fseekocat(iB, cposmarker, catfid);
		freadstar(sB, catfid);
		fseekocat(iC, cposmarker, catfid);
		freadstar(sC, catfid);
		fseekocat(iD, cposmarker, catfid);
		freadstar(sD, catfid);
	}
}

