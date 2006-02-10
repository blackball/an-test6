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
#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "blocklist.h"
#include "solver2.h"
#include "solver2_callbacks.h"
#include "hitsfile.h"
#include "suspend.h"
#include "hitlist.h"

char* OPTIONS = "hpef:o:t:n:x:d:r:R:D:H:F:T:vS:L:a:b:IB:";
// M:

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s -f fname -o fieldname\n"
			"   [-t code_tol]\n"
			"   [-p] (flip field parity)\n"
			"   [-n matches_needed_to_agree]\n"
			"   [-x max_matches_needed]\n"
			"   [-F maximum-number-of-field-objects-to-process]\n"
			"   [-T maximum-number-of-field-quads-to-try]\n"
			"   [-H hits-file-name.hits]\n"
			"   [-r minimum-ra-for-debug-output]\n"
			"   [-R maximum-ra-for-debug-output]\n"
			"   [-d minimum-dec-for-debug-output]\n"
			"   [-D maximum-dec-for-debug-output]\n"
			"   [-S first-field]\n"
			"   [-L last-field]\n"
			"   [-a resume-from-file]\n"
			"   [-b suspend-to-file]\n"
			"   [-I] (interactive mode - probably only useful from Python)\n"
			//"   [-M match-file-name]\n"
			"   [-B batch-file-name]\n"
			"%s"
			"     code tol is the RADIUS (not diameter or radius^2) in 4d codespace\n",
			progname, hitlist_get_parameter_help());
}

#define DEFAULT_MIN_MATCHES_TO_AGREE 3
#define DEFAULT_MAX_MATCHES_NEEDED 8
#define DEFAULT_CODE_TOL .002
#define DEFAULT_PARITY_FLIP 0

qidx solve_fields(xyarray *thefields, int maxfieldobjs, int maxtries,
				  kdtree_t *codekd, double codetol, hitlist* hits);

void debugging_gather_hits(hitlist* hits, blocklist* outputlist);

int find_correspondences(blocklist* hits, sidx* starids, sidx* fieldids,
						 int* p_ok);

int get_next_assignment();

char *fieldfname = NULL, *treefname = NULL, *hitfname = NULL;
char *quadfname = NULL, *catfname = NULL;
FILE *hitfid = NULL, *quadfid = NULL, *catfid = NULL;
//FILE* matchfid = NULL;
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

char ParityFlip = DEFAULT_PARITY_FLIP;
unsigned int min_matches_to_agree = DEFAULT_MIN_MATCHES_TO_AGREE;
unsigned int max_matches_needed = DEFAULT_MAX_MATCHES_NEEDED;

double DebuggingRAMin;
double DebuggingRAMax;
double DebuggingDecMin;
double DebuggingDecMax;
int Debugging = 0;
bool verbose = FALSE;
bool interactive = FALSE;
bool rename_suspend;
bool batchmode = FALSE;
int maxfieldobjs = 0;
int firstfield = 0;
int lastfield = -1;
int* list_of_fields = NULL;

int nctrlcs = 0;
bool *p_quitnow = NULL;

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
    int fieldtries = 0;

    void*  mmapped_tree = NULL;
    size_t mmapped_tree_size = 0;
	void*  mmap_cat = NULL;
	size_t mmap_cat_size = 0;
	void*  mmap_quad = NULL;
	size_t mmap_quad_size = 0;

	off_t endoffset;
	hits_header hitshdr;
	hitlist* hitlist;
	char* progname = argv[0];
	char alloptions[256];
	char* hitlist_options;

	//char* matchfname = NULL;
	char* batchfname = NULL;

    if (argc <= 4) {
		printHelp(progname);
		return (OPT_ERR);
    }

	hitlist_set_default_parameters();
	hitlist_options = hitlist_get_parameter_options();
	sprintf(alloptions, "%s%s", OPTIONS, hitlist_options);

    while ((argchar = getopt (argc, argv, alloptions)) != -1) {
		char* ind = index(hitlist_options, argchar);
		if (ind) {
			if (hitlist_process_parameter(argchar, optarg)) {
				printHelp(progname);
				exit(-1);
			}
			continue;
		}
		switch (argchar) {
			/*
			  case 'M':
			  matchfname = optarg;
			  break;
			*/
		case 'B':
			batchfname = optarg;
			batchmode = TRUE;
			break;
		case 'I':
			interactive = TRUE;
			break;
		case 'a':
			resumefname = strdup(optarg);
			break;
		case 'b':
			suspendfname = strdup(optarg);
			break;
		case 'S':
			firstfield = atoi(optarg);
			break;
		case 'L':
			lastfield = atoi(optarg);
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
			maxfieldobjs = atoi(optarg);
			if (!maxfieldobjs) {
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
			printHelp(progname);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}
	}
	if (Debugging != 0 && Debugging != 4) {
		fprintf(stderr, "When debugging need all of -d -D -r -R\n");
		return (OPT_ERR);
	}

	if (treefname == NULL || fieldfname == NULL || codetol < 0) {
		printHelp(progname);
		return (OPT_ERR);
	}

	if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		printHelp(progname);
		return (OPT_ERR);
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

	/*
	  if (matchfname) {
	  fprintf("Writing matches to file %s...\n", matchfname);
	  fopenout(matchfname, matchfid);
	  }
	*/

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

	/*
	  fprintf(stderr, "  Solving %lu fields (code_match_tol=%lg,agreement_tol=%lg arcsec)...\n",
	  numfields, codetol, AgreeArcSec);
	*/

	hitlist = hitlist_new();

	do {

		if (interactive) {
			if (get_next_assignment())
				break;
			fprintf(stderr, "Running!\n");
		}

		if (resumefname && suspendfname &&
			(strcmp(resumefname, suspendfname) == 0)) {
			char buffer[1024];
			rename_suspend = TRUE;
			free(suspendfname);
			snprintf(buffer, sizeof(buffer), "%s.tmp", resumefname);
			suspendfname = strdup(buffer);
		} else {
			rename_suspend = FALSE;
		}

		if (resumefname) {
			// read resume file.
			double index_scale;
			char oldfieldname[256];
			char oldtreename[256];
			uint nfields;
			resumefid = fopen(resumefname, "rb");
			if (!resumefid) {
				fprintf(stderr, "Couldn't open resume file %s: %s\n",
						resumefname, strerror(errno));
				fprintf(stderr, "Starting from scratch.\n");
			} else {
				if (suspend_read_header(resumefid, &index_scale, oldfieldname, oldtreename, &nfields)) {
					fprintf(stderr, "Couldn't read resume file %s: %s\n", resumefname, strerror(errno));
					fprintf(stderr, "Starting from scratch.\n");
					fclose(resumefid);
					resumefid = NULL;
				}
			}
		}
		if (suspendfname) {
			fopenout(suspendfname, suspendfid);
			suspend_write_header(suspendfid, index_scale, fieldfname,
								 treefname, dyv_array_size(thefields));
		}

		// write HITS header.
		if (interactive) {
			fprintf(stderr, "Writing HITS output to stdout.\n");
			hitfid = stdout;
		} else {
			fprintf(stderr, "Using HITS output file %s\n", hitfname);
			fopenout(hitfname, hitfid);
		}
		hits_header_init(&hitshdr);
		hitshdr.field_file_name = fieldfname;
		hitshdr.tree_file_name = treefname;
		hitshdr.nfields = numfields;
		hitshdr.ncodes = codekd->ndata;
		hitshdr.nstars = numstars;
		hitshdr.codetol = codetol;
		//hitshdr.agreetol = AgreeArcSec;
		hitshdr.parity = ParityFlip;
		hitshdr.min_matches_to_agree = min_matches_to_agree;
		hitshdr.max_matches_needed = max_matches_needed;
		hits_write_header(hitfid, &hitshdr);

		signal(SIGINT, signal_handler);

		numsolved = solve_fields(thefields, maxfieldobjs, fieldtries,
								 codekd, codetol, hitlist);
		fprintf(stderr, "\nDone (solved %lu).\n", numsolved);
		fprintf(stderr, "Done solving.\n");
		fflush(stderr);

		if (list_of_fields) {
			free(list_of_fields);
			list_of_fields = NULL;
		}

		// finish up HITS file...
		hits_write_tailer(hitfid);
		if (interactive)
			fflush(hitfid);
		else
			fclose(hitfid);

		// clean up...
		if (resumefid)
			fclose(resumefid);
		if (suspendfid)
			fclose(suspendfid);

		signal(SIGINT, SIG_DFL);

		if (rename_suspend) {
			if (rename(suspendfname, resumefname)) {
				fprintf(stderr, "Couldn't rename suspend file from %s to %s: %s\n",
						suspendfname, resumefname, strerror(errno));
			}
		}

	} while (interactive);

	if (batchfname) {
		FILE* batchfid;
		fprintf(stderr, "Writing marker file %s...\n", batchfname);
		fopenout(batchfname, batchfid);
		fclose(batchfid);
	}
	/*
	  if (matchfid)
	  fclose(matchfid);
	*/

	hitlist_free(hitlist);

	free_fn(hitfname);
	free_fn(fieldfname);
	free_fn(treefname);

	free(suspendfname);
	free(resumefname);

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

int get_next_assignment() {
	for (;;) {
		char buffer[10240];
		fprintf(stderr, "\nAwaiting your command:\n");
		fflush(stderr);
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			return -1;
		}
		// strip off newline
		if (buffer[strlen(buffer) - 1] == '\n')
			buffer[strlen(buffer) - 1] = '\0';
		fprintf(stderr, "Command: %s\n", buffer);
		fflush(stderr);

		if (strncmp(buffer, "help", 4) == 0) {
			fprintf(stderr, "Commands:\n"
					"    suspend <suspend-file-name>\n"
					"    resume  <resume-file-name>\n"
					"    field <field-number>\n"
					"    fields <field-number> <field-number> ...]\n"
					"    depth <maximum-field-object>\n"
					"    run\n"
					"    quit\n"
					"    help\n");
			fflush(stderr);
		} else if (strncmp(buffer, "suspend ", 8) == 0) {
			free(suspendfname);
			suspendfname = strdup(buffer + 8);
			fprintf(stderr, "Set suspend file to \"%s\".\n", suspendfname);
			if (strlen(suspendfname) == 0) {
				free(suspendfname);
				suspendfname = NULL;
			}
		} else if (strncmp(buffer, "resume ", 7) == 0) {
			free(resumefname);
			resumefname = strdup(buffer + 7);
			fprintf(stderr, "Set resume file to \"%s\".\n", resumefname);
			if (strlen(resumefname) == 0) {
				free(resumefname);
				resumefname = NULL;
			}
		} else if (strncmp(buffer, "field ", 6) == 0) {
			int fld = atoi(buffer + 6);
			firstfield = fld;
			lastfield = fld + 1;
			fprintf(stderr, "Set field to %i\n", fld);
			fflush(stderr);
		} else if (strncmp(buffer, "fields ", 7) == 0) {
			char* str = buffer + 7;
			char* endp;
			int nflds;
			blocklist* flds = blocklist_int_new(256);
			fprintf(stderr, "Addings fields: ");
			fflush(stderr);
			for (;;) {
				int fld = strtol(str, &endp, 10);
				if (str == endp) {
					// non-numeric value
					fprintf(stderr, "\nCouldn't parse: %.20s [etc]\n", str);
					break;
				}
				blocklist_int_append(flds, fld);
				fprintf(stderr, "%i ", fld);
				fflush(stderr);
				if (*endp == '\0')
					// end of string
					break;
				str = endp + 1;
			}
			nflds = blocklist_count(flds);
			list_of_fields = (int*)malloc(nflds * sizeof(int));
			blocklist_int_copy(flds, 0, nflds, list_of_fields);
			firstfield = 0;
			lastfield = nflds;
			fprintf(stderr, "\nAdded %i fields.\n", nflds);
			fflush(stderr);
		} else if (strncmp(buffer, "depth ", 6) == 0) {
			int d = atoi(buffer + 6);
			maxfieldobjs = d;
			fprintf(stderr, "Set depth to %i\n", d);
			fflush(stderr);
		} else if (strncmp(buffer, "run", 3) == 0) {
			return 0;
		} else if (strncmp(buffer, "quit", 4) == 0) {
			return 1;
		} else {
			fprintf(stderr, "I didn't understand that command.\n");
			fflush(stderr);
		}
	}
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
	if (p_quitnow)
		*p_quitnow = TRUE;
}

qidx solve_fields(xyarray *thefields, int maxfieldobjs, int maxtries,
				  kdtree_t *codekd, double codetol, hitlist* hits) {
	uint resume_fieldnum;
	uint resume_nobjs;
	uint resume_ntried;
	blocklist* resume_hits = NULL;
	qidx numsolved, ii;
	sidx numxy;
	solver_params params;
	int last;

	params.codekd = codekd;
	params.endobj = maxfieldobjs;
	params.maxtries = maxtries;
	params.max_matches_needed = max_matches_needed;
	//params.agreetol = AgreeTol;
	params.codetol = codetol;
	params.cornerpix = mk_xy(2);
	params.hits = hits;
	params.quitNow = FALSE;
	p_quitnow = &params.quitNow;

	last = dyv_array_size(thefields);
	if ((lastfield != -1) && (last > lastfield)) {
		last = lastfield;
	}

	numsolved = 0;
	for (ii=firstfield; ii<last; ii++) {
		hits_field fieldhdr;
		blocklist* bestlist;
		blocklist* allocated_list = NULL;
		sidx* starids;
		sidx* fieldids;
		int correspond_ok = 1;
		int Ncorrespond;
		int i, bestnum;
		xy *thisfield;
		int fieldnum;

		params.numtries = 0;
		params.nummatches = 0;
		params.mostagree = 0;
		params.startobj = 3;

		if (list_of_fields)
			fieldnum = list_of_fields[ii];
		else
			fieldnum = ii;

		thisfield = xya_ref(thefields, fieldnum);
		numxy = xy_size(thisfield);
		params.field = thisfield;

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

		if (resume_hits && (fieldnum == resume_fieldnum)) {
			blocklist* best;
			// Resume where we left off...
			params.numtries = resume_ntried;
			params.startobj = resume_nobjs;
			params.nummatches = blocklist_count(resume_hits);

			hitlist_add_hits(hits, resume_hits);

			best = hitlist_get_best(hits);
			params.mostagree = (best ? blocklist_count(best) : 0);
			blocklist_pointer_free(resume_hits);
			resume_hits = NULL;
			fprintf(stderr, "Resuming at field object %i (%i quads tried, %i hits found so far)\n",
					params.startobj, params.numtries, params.nummatches);
		}

		if (thisfield) {
			solve_field(&params);
		}

		/*
		  if (suspendfid || matchfid) {
		  blocklist* all = hitlist_get_all(hits);
		  if (suspendfid)
		  suspend_write_field(suspendfid, (uint)fieldnum, params.objsused, params.numtries, all);
		  if (matchfid)
		  suspend_write_field(matchfid, (uint)fieldnum, params.objsused, params.numtries, all);
		  blocklist_free(all);
		  }
		*/
		if (suspendfid) {
			blocklist* all = hitlist_get_all(hits);
			suspend_write_field(suspendfid, (uint)fieldnum, params.objsused, params.numtries, all);
			blocklist_free(all);
		}

		bestlist = hitlist_get_best(hits);

		if (!bestlist)
			bestnum = 0;
		else
			bestnum = blocklist_count(bestlist);

		if (bestnum < min_matches_to_agree) {
			if (Debugging) {
				bestlist = blocklist_pointer_new(256);
				allocated_list = bestlist;
				debugging_gather_hits(hits, bestlist);
			}
		} else {
			numsolved++;
		}

		if (bestlist)
			bestnum = blocklist_count(bestlist);

		hits_field_init(&fieldhdr);
		fieldhdr.user_quit = params.quitNow;
		fieldhdr.field = fieldnum;
		fieldhdr.objects_in_field = numxy;
		fieldhdr.objects_examined = params.objsused;
		fieldhdr.field_corners = params.cornerpix;
		fieldhdr.ntries = params.numtries;
		fieldhdr.nmatches = params.nummatches;
		fieldhdr.nagree = bestnum;
		fieldhdr.parity = ParityFlip;
		hits_write_field_header(hitfid, &fieldhdr);

		fprintf(stderr, "\n    field %i: tried %i quads, matched %i codes, "
				"%i agree\n", fieldnum, params.numtries, params.nummatches, bestnum);

		hits_start_hits_list(hitfid);
		if (bestnum < min_matches_to_agree) {
			hits_write_hit(hitfid, NULL);
			hits_end_hits_list(hitfid);
			hits_write_field_tailer(hitfid);
			fflush(hitfid);
			continue;
		}

		for (i=0; i<bestnum; i++) {
			MatchObj* mo = (MatchObj*)blocklist_pointer_access(bestlist, i);
			hits_write_hit(hitfid, mo);
		}
		hits_end_hits_list(hitfid);

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

		hitlist_clear(hits);
		//quitNow = false;
	}
	p_quitnow = NULL;
	free_xy(params.cornerpix);
	return numsolved;
}


void debugging_gather_hits(hitlist* hits, blocklist* outputlist) {
	int j, M;
	blocklist* all = hitlist_get_all(hits);
	M = blocklist_count(all);
	for (j=0; j<M; j++) {
		double minra, maxra, mindec, maxdec;
		double x, y, z;
		MatchObj* mo = (MatchObj*)blocklist_pointer_access(all, j);
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

	
