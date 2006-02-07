#include <sys/mman.h>
#include <sys/file.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "blocklist.h"
#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "fileutil.h"
#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"
#include "suspend.h"
#include "solver2.h"

#define OPTIONS "hpf:o:t:m:n:x:F:s:w:i:"

void printHelp(char* progname) {
	printf("usage: %s -f <index-prefix> -o <field-prefix> "
		   "-s <state-directory> -w <work-file> -i <index-number>\n"
		   "  [-h] (help)\n\n", progname);
}

typedef unsigned int uint;

int Nhealpix = 12;

struct workstatus {
	// if non-zero, some worker is working on this.
	time_t start_time;
	// the healpix index
	uint healpix;
	// the field number
	uint field;
	// the number of field objects we've examined
	uint nobjects;
	// the number of agreeing matches found
	uint nagree;
};
typedef struct workstatus workstatus;


void lock_workfile(FILE* workfile, size_t workfilesize) {
	printf("Trying to lock work file...\n");
	if (lockf(fileno(workfile), F_LOCK, workfilesize)) {
		printf("Couldn't lock work file: %s\n", strerror(errno));
		exit(-1);
	}
}

void synchronize_workfile(void* mmapped_work, size_t mmapped_work_size) {
	if (msync(mmapped_work, mmapped_work_size,
			  MS_SYNC | MS_INVALIDATE)) {
		printf("Couldn't msync work file: %s\n", strerror(errno));
		exit(-1);
	}
}

void unlock_workfile(FILE* workfile, size_t workfilesize) {
	printf("Unlocking work file...\n");
	lockf(fileno(workfile), F_ULOCK, workfilesize);
}

void clear_hitlist(blocklist* hitlist);

void getquadids(qidx thisquad, sidx *iA, sidx *iB, sidx *iC, sidx *iD);
void getstarcoords(star *sA, star *sB, star *sC, star *sD,
                   sidx iA, sidx iB, sidx iC, sidx iD);


int depths[] = { 20, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150 };
int Ndepths = sizeof(depths) / sizeof(int);
int Nagree = 5;
int Natonce = 100;

double* catalogue;
sidx*   quadindex;

// the largest star and quad available in the corresponding files.
sidx maxstar;
qidx maxquad;

FILE *hitfid = NULL, *quadfid = NULL, *catfid = NULL;
off_t qposmarker, cposmarker;

bool use_mmap = TRUE;

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argidx, argchar; //  opterr = 0;

	char *fieldfname = NULL, *treefname = NULL, *hitfname = NULL;
	char *quadfname = NULL, *catfname = NULL;

	//char* indexname = NULL;
	char* statedir = "";
	char* workfname = NULL;
	FILE* workfile;
	int Nfields;
	FILE* fieldfid = NULL;
	off_t offset;
	char*  mmapped_work;
	size_t mmapped_work_size;
	bool initworkfile = FALSE;
	off_t workfilesize;
	xyarray* fields = NULL;
	workstatus* work;
	int i;
	int healpix = -1;
	char parity = 0;
	//xyarray* subfields;
	xy* cornerpix = NULL;
	blocklist* resume_hits = NULL;
	blocklist* hitlist = NULL;
	kdtree_t* codekd = NULL;
	double codetol = 0.002;
	double agreetol = 7.0;
    void*  mmapped_tree = NULL;
    size_t mmapped_tree_size = 0;
	void*  mmap_cat = NULL;
	size_t mmap_cat_size = 0;
	void*  mmap_quad = NULL;
	size_t mmap_quad_size = 0;
	uint min_matches_to_agree;
	uint max_matches_needed;
	uint maxobjs;
	char readStatus;
	qidx numstars, numquads;
	dimension Dim_Stars, Dim_Quads;
	double index_scale;
	double ramin, ramax, decmin, decmax;
	off_t endoffset;
    FILE* treefid = NULL;


    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'i':
			healpix = atoi(optarg);
			break;
		case 'f':
			//indexname = optarg;
			treefname = mk_ctree2fn(optarg);
			quadfname = mk_quadfn(optarg);
			catfname = mk_catfn(optarg);
			break;
		case 'o':
			fieldfname = mk_fieldfn(optarg);
			if (!hitfname)
				hitfname = mk_hitfn(optarg);
			break;
		case 's':
			statedir = optarg;
			break;
		case 't':
			codetol = strtod(optarg, NULL);
			break;
		case 'm':
			agreetol = strtod(optarg, NULL);
			break;
		case 'p':
			parity = !parity;
			break;
		case 'n':
			min_matches_to_agree = (unsigned int) strtol(optarg, NULL, 0);
			break;
		case 'x':
			max_matches_needed = (unsigned int) strtol(optarg, NULL, 0);
			break;
		case 'w':
			workfname = optarg;
			break;
		case 'F':
			maxobjs = atoi(optarg);
			if (!maxobjs) {
				fprintf(stderr, "Couldn't parse -F argument: %s\n", optarg);
				exit(-1);
			}
			break;
		case 'h':
			printHelp(argv[0]);
			exit(0);
		}

    if (!treefname || !fieldfname || !workfname || (codetol < 0.0)) {
		printHelp(argv[0]);
		exit(-1);
    }

	if (healpix == -1) {
		printf("You must specify the index (healpix) number (-i)\n");
		printHelp(argv[0]);
		exit(-1);
	}

    if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			printf ("Non-option argument %s\n", argv[argidx]);
		printHelp(argv[0]);
		exit(-1);
    }

    agreetol = sqrt(2.0) * radscale2xyzscale(arcsec2rad(agreetol));

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

	// read the fields (to find Nfields)
	fopenin(fieldfname, fieldfid);
	fields = readxy(fieldfid, parity);
	if (!fields) {
		fprintf(stderr, "Couldn't read field file: %s\n", strerror(errno));
		exit(-1);
	}
	Nfields = fields->size;
	//free_xy(fields);
	fclose(fieldfid);

	workfile = fopen(workfname, "w+b");
	if (!workfile) {
		fprintf(stderr, "Couldn't open work file %s: %s\n",
			   workfname, strerror(errno));
		exit(-1);
	}
	// check if the file existed.
	workfilesize = Nfields * Nhealpix * sizeof(workstatus);

	lock_workfile(workfile, workfilesize);

	fseeko(workfile, 0, SEEK_END);
	offset = ftello(workfile);
	fprintf(stderr, "Workfile is %i bytes long.\n", (int)offset);
	if (!offset) {
		char blank[sizeof(workstatus)];

		fprintf(stderr, "Initializing work file...\n");

		fseeko(workfile, 0, SEEK_SET);
		memset(blank, 0, sizeof(workstatus));
		for (i=0; i<(Nfields * Nhealpix); i++) {
			fwrite(blank, 1, sizeof(workstatus), workfile);
		}
		/*
		  ;
		  // go to the end...
		  fseeko(workfile, workfilesize, SEEK_SET);
		*/
		fsync(fileno(workfile));
		//fclose(workfile);
		initworkfile = TRUE;
	} else if (offset != workfilesize) {
		// file is the wrong size.
		fprintf(stderr, "Work file is the wrong size: %i, not %i.\n",
			   (int)offset, (int)workfilesize);
		exit(-1);
	}
	fseeko(workfile, 0, SEEK_SET);

	mmapped_work_size = workfilesize;
	mmapped_work = mmap(0, workfilesize, PROT_READ | PROT_WRITE,
						MAP_SHARED, fileno(workfile), 0);
	//fclose(workfile);
	if (mmapped_work == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap work file: %s\n", strerror(errno));
		exit(-1);
	}
	work = (workstatus*)mmapped_work;

	if (initworkfile) {
		int i, j;

		// return to the beginning...
		//fseeko(workfile, 0, SEEK_SET);

		for (i=0; i<Nfields; i++) {
			for (j=0; j<Nhealpix; j++) {
				workstatus* w;
				w = work + i*Nhealpix + j;
				w->start_time = 0;
				w->healpix = j;
				w->field = i;
				w->nobjects = 0;
				w->nagree = 0;
			}
		}
	}

	/*
	  subfields = mk_xyarray(Nfields);
	  for (i=0; i<Nfields; i++) {
	  xya_set(subfields, i, NULL);
	  }
	  free_xyarray(subfields);
	*/

	resume_hits = blocklist_pointer_new(256);
	hitlist = blocklist_pointer_new(256);
	cornerpix = mk_xy(2);

	// make iterative-deepening sweeps through the
	// set of fields.
	for (;;) {
		int d;
		int j;
		bool solved;
		time_t now;
		int inds[Natonce];
		int ninds;
		int nextdepth;

		int all_objsused[Natonce];
		int all_mostagree[Natonce];

		// at each step, we find a set of fields which
		// have the smallest "nobjects" value, and which
		// have "start_time" = 0, and also have no other
		// index marking it with "nagree" above our
		// agreement threshold.

		fprintf(stderr, "Starting a sweep...\n");

		// find the smallest "nobjects" value for this
		// field.
		ninds = 0;
		d = 1000000;
		for (i=0; i<Nfields; i++) {
			workstatus* w = work + i*Nhealpix;
			if ((w[healpix].nobjects > d) ||
				(w[healpix].start_time)) {
				continue;
			}
			solved = FALSE;
			for (j=0; j<Nhealpix; j++) {
				if (w[j].nagree > Nagree) {
					solved = TRUE;
					break;
				}
			}
			if (solved) continue;
			if (w[healpix].nobjects == d) {
			} else {
				d = w[healpix].nobjects;
				ninds = 0;
			}
			if (ninds < Natonce) {
				inds[ninds] = i;
				ninds++;
			}
		}

		fprintf(stderr, "Smallest depth: %i.  Ninds %i\n", d, ninds);

		/*
		  if (d < depths[0]) {
		  nextdepth = 0;
		  } else {
		*/
		// find the next-highest depth.
		for (nextdepth=0; nextdepth<Ndepths; nextdepth++) {
			if (d < depths[nextdepth])
				break;
		}
		//}
		fprintf(stderr, "Next depth %i\n", nextdepth);
		if (nextdepth == Ndepths) {
			break;
		}
		nextdepth = depths[nextdepth];
		fprintf(stderr, "Next depth %i\n", nextdepth);

		// mark these fields with our start_time, sync the file,
		// drop the lock, and begin working on them.
		now = time(NULL);
		for  (i=0; i<ninds; i++) {
			work[inds[i]*Nhealpix + healpix].start_time = now;
		}

		synchronize_workfile(mmapped_work, mmapped_work_size);

		unlock_workfile(workfile, workfilesize);

		// DO THE WORK...
		for (i=0; i<ninds; i++) {
			uint startobj;
			int endobj;
			int maxtries, max_matches_needed;
			uint numtries, fieldnum, nhits;
			int nummatches, mostagree, objsused;
			bool quitNow = FALSE;
			int ntries;

			workstatus* w = work + inds[i]*Nhealpix + healpix;
			char resumefn[256];
			char suspendfn[256];

			startobj = 3;
			endobj = nextdepth;
			maxtries = 0;
			max_matches_needed = 8;
			numtries = 0;
			nummatches = 0;
			mostagree = 0;

			sprintf(resumefn, "%s%si%02i_f%04i.suspend",
					statedir, (statedir && strlen(statedir) &&
							   statedir[strlen(statedir)-1] != '/') ? "/":"",
					w->healpix, w->field);
			sprintf(suspendfn, "%s%si%02i_f%04i.suspend.new",
					statedir, (statedir && strlen(statedir) &&
							   statedir[strlen(statedir)-1] != '/') ? "/":"",
					w->healpix, w->field);
			fprintf(stderr, "using resume file %s and suspend file %s\n", resumefn, suspendfn);

			// read resume file.
			do {
				off_t offset;
				FILE* resumefid;
				double index_scale;
				char oldfieldname[256];
				char oldtreename[256];
				uint nfields;

				resumefid = fopen(resumefn, "r");
				if (!resumefid && (errno == ENOENT)) {
					fprintf(stderr, "Resume file %s does not exist; starting from the beginning.\n", resumefn);
					break;
				} else if (!resumefid) {
					fprintf(stderr, "Couldn't open resume file %s: %s\n", resumefn, strerror(errno));
					break;
				}
				fseeko(resumefid, 0, SEEK_END);
				offset = ftello(resumefid);
				if (!offset) {
					fprintf(stderr, "Resume file %s is empty; starting from the beginning.\n", resumefn);
					fclose(resumefid);
					break;
				}
				fseeko(resumefid, 0, SEEK_SET);
				if (suspend_read_header(resumefid, &index_scale, oldfieldname, oldtreename, &nfields)) {
					fprintf(stderr, "Couldn't read resume file %s: %s\n", resumefn, strerror(errno));
					fclose(resumefid);
					break;
				}
				if (suspend_read_field(resumefid, &fieldnum, &startobj, &numtries, hitlist)) {
					fprintf(stderr, "Couldn't read record from resume file %s: %s\n", resumefn, strerror(errno));
					fclose(resumefid);
					break;
				}
				if (fieldnum != inds[i]) {
					fprintf(stderr, "Resume file %s: contains field %i, not %i.\n", resumefn, fieldnum, w->field);
					fclose(resumefid);
					break;
				}
				for (i=0; i<blocklist_count(resume_hits); i++) {
					MatchObj* mo = (MatchObj*)blocklist_pointer_access(resume_hits, i);
					solver_add_hit(hitlist, mo, agreetol);
				}
				nhits = blocklist_count(resume_hits);
				blocklist_remove_all_but_first(resume_hits);

				fprintf(stderr, "Resuming field %i at field object %i (%i quads tried, %i hits found so far)\n",
						w->field, (int)startobj, (int)numtries, nhits);
				fclose(resumefid);
			} while (0);

			ntries = (int)numtries;
			
			solve_field(xya_ref(fields, w->field),
						startobj, endobj,
						maxtries, max_matches_needed,
						codekd, codetol, hitlist,
						&quitNow, agreetol,
						&ntries, &nummatches, &mostagree,
						cornerpix, &objsused);
			numtries = ntries;

			fprintf(stderr, "\n");

			all_objsused[i] = objsused;
			all_mostagree[i] = mostagree;

			do {
				// write the suspend file.
				FILE* suspendfid;
				blocklist* tmplist;
				int i, N, j, M;

				fprintf(stderr, "Writing suspend file %s...\n", suspendfn);
				fopenout(suspendfn, suspendfid);
				suspend_write_header(suspendfid, index_scale, fieldfname,
									 treefname, 1);
				tmplist = blocklist_pointer_new(256);
				N = blocklist_count(hitlist);
				for (i=0; i<N; i++) {
					blocklist* lst = (blocklist*)blocklist_pointer_access(hitlist, i);
					if (!lst) continue;
					M = blocklist_count(lst);
					for (j=0; j<M; j++) {
						MatchObj* mo = (MatchObj*)blocklist_pointer_access(lst, j);
						blocklist_pointer_append(tmplist, mo);
					}
				}

				suspend_write_field(suspendfid, w->field, objsused,
									numtries, tmplist);

				blocklist_pointer_free(tmplist);

				fclose(suspendfid);

				// rename the new suspend file to the resume file name.
				fprintf(stderr, "Renaming suspend file %s to %s ...\n", suspendfn, resumefn);
				if (rename(suspendfn, resumefn)) {
					fprintf(stderr, "Couldn't rename suspend file %s to resume file %s: %s\n",
							suspendfn, resumefn, strerror(errno));
				}
				fprintf(stderr, "Done.\n");
			} while (0);

			/*
			  {
			  blocklist* bestlist = get_best_hits(hitlist);
			  if (!bestlist) {
			  mostagree = 0;
			*/

			/*
			  if (mostagree > min_matches_to_agree) {
			  // write hits entry
			  }
			*/

			clear_hitlist(hitlist);
		}


		/*
		  for (i=0; i<Nfields; i++) {
		  xya_set(subfields, i, NULL);
		  }
		  for (i=0; i<ninds; i++) {
		  xya_set(subfields, inds[i], xya_ref(thefields, inds[i]));
		  }
		  {
		  char resumefn[256];
		  char suspendfn[256];
		  sprintf(resumefn, "%s%si%i_f%i.suspend",
		  statedir, (statedir && strlen(statedir) &&
		  statedir[strlen(statedir)-1] != '/') ? "/":"",
		  w->healpix, w->field);
		  sprintf(suspendfn, "%s%si%i_f%i.suspend.new",
		  statedir, (statedir && strlen(statedir) &&
		  statedir[strlen(statedir)-1] != '/') ? "/":"",
		  w->healpix, w->field);
		  printf("using resume file %s and suspend file %s\n", resumefn, suspendfn);
		  }
		*/

		// once we're done, we again grab the lock,
		// set the start_time back to zero, and update
		// the 'nobjects' and 'nagree' fields.
		// (note that 'nobjects' may not be the same as
 		// our target because of "early (successful) bailout").
		lock_workfile(workfile, workfilesize);

		for  (i=0; i<ninds; i++) {
			workstatus* w = work + inds[i]*Nhealpix + healpix;
			w->start_time = 0;
			w->nobjects = all_objsused[i];
			w->nagree = all_mostagree[i];
		}
	}

	blocklist_pointer_free(resume_hits);
	blocklist_pointer_free(hitlist);
	free_xy(cornerpix);

	fprintf(stderr, "Unmapping work file...\n");
	munmap(mmapped_work, mmapped_work_size);

	// clean up...
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

	if (fields)
		free_xyarray(fields);

	unlock_workfile(workfile, workfilesize);

	fclose(workfile);

	return 0;
}

blocklist* get_best_hits(blocklist* hitlist) {
    int i, N;
    int bestnum;
	blocklist* bestlist;
	bestnum = 0;
	bestlist = NULL;
    N = blocklist_count(hitlist);
    for (i=0; i<N; i++) {
		int M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hitlist, i);
		M = blocklist_count(hits);
		if (M > bestnum) {
			bestnum = M;
			bestlist = hits;
		}
	}
	return bestlist;
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

