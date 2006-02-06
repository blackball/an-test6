#include <sys/mman.h>
#include <sys/file.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

//#include "blocklist.h"
#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hf:o:s:w:i:"

void printHelp(char* progname) {
	printf("usage: %s -f <index-prefix> -o <field-prefix> "
		   "-s <state-directory> -w <work-file> -i <index>\n"
		   "  [-h] (help)\n\n", progname);
}

typedef unsigned int uint;

int Nindexes = 12;

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


int depths[] = { 50, 70, 90, 110, 130, 150 };
int Nagree = 5;
int Natonce = 100;

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argidx, argchar; //  opterr = 0;

	char* indexname = NULL;
	char* fieldname = NULL;
	char* statedir = "";
	char* workfn = NULL;
	FILE* workfile;
	int Nfields;
	FILE* fieldfid = NULL;
	off_t offset;
	char*  mmapped_work;
	size_t mmapped_work_size;
	bool initworkfile = FALSE;
	off_t workfilesize;
	xyarray* fields;
	workstatus* work;
	int i;
	int index = -1;
	char parity = 0;
	
    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'n':
			index = atoi(optarg);
			break;
		case 'f':
			indexname = optarg;
			break;
		case 'o':
			fieldname = optarg;
			break;
		case 's':
			statedir = optarg;
			break;
		case 'w':
			workfn = optarg;
			break;
		case 'h':
			printHelp(argv[0]);
			exit(0);
		}

	if (!indexname || !fieldname || !workfn) {
		printHelp(argv[0]);
		exit(-1);
	}

	if (index == -1) {
		printf("You must specify the index number (-i)\n");
		printHelp(argv[0]);
		exit(-1);
	}

    if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			printf ("Non-option argument %s\n", argv[argidx]);
		printHelp(argv[0]);
		exit(-1);
    }

	// read the fields (to find Nfields)
	fopenin(fieldname, fieldfid);
	fields = readxy(fieldfid, parity);
	if (!fields) {
		printf("Couldn't read field file: %s\n", strerror(errno));
		exit(-1);
	}
	Nfields = fields->size;
	free_xy(fields);
	fclose(fieldfid);

	workfile = fopen(workfn, "r+b");
	if (!workfile) {
		printf("Couldn't open work file %s: %s\n",
			   workfn, strerror(errno));
		exit(-1);
	}
	// check if the file existed.
	workfilesize = Nfields * Nindexes * sizeof(workstatus);

	lock_workfile(workfile, workfilesize);

	fseeko(workfile, 0, SEEK_END);
	offset = ftello(workfile);
	if (!offset) {
		printf("Initializing work file...\n");
		initworkfile = TRUE;
	} else if (offset != workfilesize) {
		// file is the wrong size.
		printf("Work file is the wrong size: %i, not %i.\n",
			   (int)offset, (int)workfilesize);
		exit(-1);
	}
	fseeko(workfile, 0, SEEK_SET);

	mmapped_work_size = workfilesize;
	mmapped_work = mmap(0, workfilesize, PROT_READ | PROT_WRITE,
						MAP_SHARED, fileno(workfile), 0);
	if (mmapped_work == MAP_FAILED) {
		printf("Failed to mmap work file: %s\n", strerror(errno));
		goto cleanup;
	}
	work = (workstatus*)mmapped_work;

	if (initworkfile) {
		int i, j;

		// go to the end...
		fseeko(workfile, workfilesize, SEEK_SET);
		// then return to the beginning.
		fseeko(workfile, 0, SEEK_SET);

		for (i=0; i<Nfields; i++) {
			for (j=0; j<Nindexes; j++) {
				workstatus* w;
				w = work + i*Nindexes + j;
				w->start_time = 0;
				w->healpix = j;
				w->field = i;
				w->nobjects = 0;
				w->nagree = 0;
			}
		}
	}

	// make iterative-deepening sweeps through the
	// set of fields.
	for (;;) {
		int d;
		int j;
		bool solved;
		time_t now;
		int inds[Natonce];
		int ninds;

		// at each step, we find a set of fields which
		// have the smallest "nobjects" value, and which
		// have "start_time" = 0, and also have no other
		// index marking it with "nagree" above our
		// agreement threshold.

		// find the smallest "nobjects" value for this
		// field.
		ninds = 0;
		d = 1000000;
		for (i=0; i<Nfields; i++) {
			workstatus* w = work + i*Nindexes;
			if ((w[index].nobjects > d) ||
				(w[index].start_time)) {
				continue;
			}
			solved = FALSE;
			for (j=0; j<Nindexes; j++) {
				if (w[j].nagree > Nagree) {
					solved = TRUE;
					break;
				}
			}
			if (solved) continue;
			if (w[index].nobjects == d) {
			} else {
				d = w[index].nobjects;
				ninds = 0;
			}
			if (ninds < Natonce) {
				inds[ninds] = i;
				ninds++;
			}
		}

		// mark these fields with our start_time, sync the file,
		// drop the lock, and begin working on them.
		now = time(NULL);
		for  (i=0; i<ninds; i++) {
			work[inds[i]*Nindexes + index].start_time = now;
		}

		synchronize_workfile(mmapped_work, mmapped_work_size);

		unlock_workfile(workfile, workfilesize);

		// DO THE WORK...
		for (i=0; i<ninds; i++) {
			workstatus* w = work + inds[i]*Nindexes + index;
			char resumefn[256];
			sprintf(resumefn, "%s%si%i_f%i.suspend",
					statedir, (statedir && strlen(statedir) &&
							   statedir[strlen(statedir)-1] != '/') ? "/":"",
					w->healpix, w->field);
			printf("using suspend/resume file %s\n", resumefn);

			// create xyarray of the fields
			// solve_fields(...)

		}

		// once we're done, we again grab the lock,
		// set the start_time back to zero, and update
		// the 'nobjects' and 'nagree' fields.
		// (note that 'nobjects' may not be the same as
 		// our target because of "early (successful) bailout").
		lock_workfile(workfile, workfilesize);

		for  (i=0; i<ninds; i++) {
			workstatus* w = work + inds[i]*Nindexes + index;
			w->start_time = 0;
			// w->nobjects = ...;
			// w->nagree = ...;
		}
	}


	printf("Unmapping work file...\n");
	munmap(mmapped_work, mmapped_work_size);

 cleanup:
	printf("Unlocking work file...\n");
	unlock_workfile(workfile, workfilesize);

	fclose(workfile);

	return 0;
}
