/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "matchobj.h"
#include "matchfile.h"
#include "rdlist.h"
#include "histogram.h"
#include "solvedfile.h"

char* OPTIONS = "hR:A:B:n:t:f:C:T:F:i:I:m:M:";

void printHelp(char* progname) {
    fprintf(stderr, "Usage: %s\n"
            "   -R rdls-file-template\n"
            "   [-i <first-file>]\n"
            "   [-I <last-file>]\n"
            "   [-A <first-field>]  (default 0)\n"
            "   [-B <last-field>]   (default the largest field encountered)\n"
            "   [-n <negative-fields-rdls>]"
            "   [-f <false-positive-fields-rdls>]\n"
            "   [-t <true-positive-fields-rdls>]\n"
            "   [-C <number-of-RDLS-stars-to-compute-center>]\n"
            "   [-T <true-positive-solvedfile>]\n"
            "   [-F <false-positive-solvedfile>]  (note, both of these are per-MATCH, not per-FIELD)\n"
            "   [-m <true matches output file>]\n"
            "   [-M <false matches output file>]\n"
            "\n"
            "   <input-match-file> ...\n"
            "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
    char* progname = argv[0];
    char** inputfiles = NULL;
    int ninputfiles = 0;
    char* rdlsfname = NULL;
    rdlist* rdls = NULL;
    int i;
    int correct, incorrect;
    int firstfield = 0;
    int lastfield = -1;

    int nfields;

    char* truefn = NULL;
    char* fpfn = NULL;
    char* negfn = NULL;

    int Ncenter = 0;

    char* fpsolved = NULL;
    char* tpsolved = NULL;
    int nfields_total;

    int firstfileid = 0;
    int lastfileid = 0;
    int fileid;

    matchfile** matchfiles;
    int* mfcursors;

    char* truematchfn = NULL;
    char* falsematchfn = NULL;
    matchfile* truematch = NULL;
    matchfile* falsematch = NULL;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
        switch (argchar) {
        case 'h':
            printHelp(progname);
            return (HELP_ERR);
        case 'm':
            truematchfn = optarg;
            break;
        case 'M':
            falsematchfn = optarg;
            break;
        case 'A':
            firstfield = atoi(optarg);
            break;
        case 'B':
            lastfield = atoi(optarg);
            break;
        case 'C':
            Ncenter = atoi(optarg);
            break;
        case 'R':
            rdlsfname = optarg;
            break;
        case 't':
            truefn = optarg;
            break;
        case 'f':
            fpfn = optarg;
            break;
        case 'n':
            negfn = optarg;
            break;
        case 'T':
            tpsolved = optarg;
            break;
        case 'F':
            fpsolved = optarg;
            break;
        case 'i':
            firstfileid = atoi(optarg);
            break;
        case 'I':
            lastfileid = atoi(optarg);
            break;
        default:
            return (OPT_ERR);
        }
    }
    if (optind < argc) {
        ninputfiles = argc - optind;
        inputfiles = argv + optind;
    } else {
        printHelp(progname);
        exit(-1);
    }
    if (!rdlsfname) {
        fprintf(stderr, "You must specify an RDLS file!\n");
        printHelp(progname);
        exit(-1);
    }

    if (truematchfn) {
        truematch = matchfile_open_for_writing(truematchfn);
        if (!truematch) {
            fprintf(stderr, "Failed to open file %s for writing matches.\n", truematchfn);
            exit(-1);
        }
        if (matchfile_write_header(truematch)) {
            fprintf(stderr, "Failed to write header for %s\n", truematchfn);
            exit(-1);
        }
    }
    if (falsematchfn) {
        falsematch = matchfile_open_for_writing(falsematchfn);
        if (!falsematch) {
            fprintf(stderr, "Failed to open file %s for writing matches.\n", falsematchfn);
            exit(-1);
        }
        if (matchfile_write_header(falsematch)) {
            fprintf(stderr, "Failed to write header for %s\n", falsematchfn);
            exit(-1);
        }
    }

    matchfiles = malloc(ninputfiles * sizeof(matchfile*));
    mfcursors = calloc(ninputfiles, sizeof(int));

    for (i=0; i<ninputfiles; i++) {
        char* fname = inputfiles[i];
        printf("Opening matchfile %s...\n", fname);
        matchfiles[i] = matchfile_open(fname);
        if (!matchfiles[i]) {
            fprintf(stderr, "Failed to open matchfile %s.\n", fname);
            exit(-1);
        }
    }

    correct = incorrect = 0;
    nfields_total = 0;

    for (i=0; i<ninputfiles; i++) {
        matchfile* mf;
        MatchObj* mo;
        mf = matchfiles[i];

        for (fileid=firstfileid; fileid<=lastfileid; fileid++) {
            char fn[1024];
            int nread = 0;
            sprintf(fn, rdlsfname, fileid);
            //printf("Reading rdls file \"%s\"...\n", fn);
            fflush(stdout);
            rdls = rdlist_open(fn);
            if (!rdls) {
                fprintf(stderr, "Couldn't read rdls file.\n");
                exit(-1);
            }
            rdls->xname = "RA";
            rdls->yname = "DEC";

            nfields = rdls->nfields;
            //printf("Read %i fields from rdls file.\n", nfields);
            if ((lastfield != -1) && (nfields > lastfield)) {
                nfields = lastfield + 1;
            } else {
                lastfield = nfields - 1;
            }

            for (; mfcursors[i]<mf->nrows; mfcursors[i]++) {
                int filenum;
                int fieldnum;
                double xyzc[3];
                double rac, decc;
                double r2;
                double arc;
                int nrd;
                double* rd;
                int k;
                bool err = FALSE;

                mo = matchfile_buffered_read_match(mf);
                filenum = mo->fieldfile;
                if (filenum < fileid)
                    continue;
                if (filenum > fileid) {
                    matchfile_buffered_read_pushback(mf);
                    break;
                }
                fieldnum = mo->fieldnum;
                if (fieldnum < firstfield)
                    continue;
                if (fieldnum > lastfield)
                    continue;

                nread++;

                star_midpoint(xyzc, mo->sMin, mo->sMax);
                r2 = distsq(xyzc, mo->sMin, 3);
                arc = distsq2arcsec(r2)/60.0;
                rac = rad2deg(xy2ra(xyzc[0], xyzc[1]));
                decc = rad2deg(z2dec(xyzc[2]));

                // read the RDLS entries for this field and make sure they're all
                // within "radius" of the center.
                nrd = rdlist_n_entries(rdls, fieldnum);
                if (Ncenter && Ncenter < nrd)
                    nrd = Ncenter;
                rd = malloc(nrd * 2 * sizeof(double));
                if (rdlist_read_entries(rdls, fieldnum, 0, nrd, rd)) {
                    fprintf(stderr, "Failed to read RDLS entries for field %i.\n", fieldnum);
                    exit(-1);
                }
                for (k=0; k<nrd; k++) {
                    double xyz[3];
                    radecdeg2xyzarr(rd[k*2], rd[k*2+1], xyz);
                    if (distsq_exceeds(xyz, xyzc, 3, r2 * 1.2)) {
                        printf("\nError: Field %i: match says center (%g, %g), scale %g arcmin, but\n",
                               fieldnum, rac, decc, arc);
                        printf("rdls %i is (%g, %g).\n", k, rd[k*2], rd[k*2+1]);
                        printf("Logprob %g (%g).\n", mo->logodds, exp(mo->logodds));
                        err = TRUE;
                        break;
                    }
                }
                free(rd);

                if (err) {
                    incorrect++;
                    if (falsematch) {
                        if (matchfile_write_match(falsematch, mo)) {
                            fprintf(stderr, "Failed to write match to %s\n", falsematchfn);
                            exit(-1);
                        }
                    }
                } else {
                    printf("Field %5i: correct hit: (%8.3f, %8.3f), scale %6.3f arcmin, logodds %g (%g)\n",
                           fieldnum, rac, decc, arc, mo->logodds, exp(mo->logodds));
                    correct++;
                    if (truematch) {
                        if (matchfile_write_match(truematch, mo)) {
                            fprintf(stderr, "Failed to write match to %s\n", truematchfn);
                            exit(-1);
                        }
                    }
                }
                fflush(stdout);

                if (tpsolved && !err)
                    solvedfile_set(tpsolved, nfields_total);
                if (fpsolved && err)
                    solvedfile_set(fpsolved, nfields_total);

                nfields_total++;
            }

            rdlist_close(rdls);

            printf("Read %i from %s for fileid %i\n", nread, inputfiles[i], fileid);
        }
    }

    printf("\n");
    printf("Read a total of %i correct and %i incorrect matches.\n", correct, incorrect);

    for (i=0; i<ninputfiles; i++) {
        matchfile_close(matchfiles[i]);
    }
    free(matchfiles);
    free(mfcursors);

    if (tpsolved)
        solvedfile_setsize(tpsolved, nfields_total);
    if (fpsolved)
        solvedfile_setsize(fpsolved, nfields_total);

    if (truematch) {
        if (matchfile_fix_header(truematch) ||
            matchfile_close(truematch)) {
            fprintf(stderr, "Failed to fix header for %s\n", truematchfn);
            exit(-1);
        }
    }
    if (falsematch) {
        if (matchfile_fix_header(falsematch) ||
            matchfile_close(falsematch)) {
            fprintf(stderr, "Failed to fix header for %s\n", falsematchfn);
            exit(-1);
        }
    }

    return 0;
}

    
