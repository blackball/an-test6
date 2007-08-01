/*
 This file is part of the Astrometry.net suite.
 Copyright 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <ctype.h>
#include <math.h>

#include "qfits_error.h"
#include "qfits_cache.h"

#include "boilerplate.h"
#include "blind.h"
#include "log.h"
#include "tic.h"

static void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "Usage: %s\n", progname);
}

static int read_parameters(blind_t* bp);

int main(int argc, char *argv[]) {
	blind_t my_bp;
	char* progname = argv[0];
	blind_t* bp = &my_bp;
	solver_t* sp = &(bp->solver);

	log_init(3);

	if (argc == 2 && strcmp(argv[1], "-s") == 0) {
		log_init(0);
		fprintf(stderr, "premptive silence\n");
	}

	if (argc != 1 && !bp->silent) {
		printHelp(progname);
		exit( -1);
	}

	qfits_err_statset(1);

	// Read input settings until "run" is encountered; repeat.
	for (;;) {
		tic();

		blind_init(bp);
		// must be in this order because init_parameters handily zeros out sp
		solver_set_default_values(sp);

		if (read_parameters(bp)) {
			blind_cleanup(bp);
			break;
		}

		if (!blind_parameters_are_sane(bp, sp)) {
			exit(-1);
		}

		if (blind_is_run_obsolete(bp, sp)) {
			blind_cleanup(bp);
			continue;
		}

		// Log this run's parameters
		logmsg("%s params:\n", progname);
		blind_log_run_parameters(bp);

		blind_run(bp);

		if (!bp->silent)
			toc();

		blind_restore_logging(bp);

		if (bp->hit_total_timelimit)
			break;
		if (bp->hit_total_cpulimit)
			break;

		blind_cleanup(bp);
	}

	qfits_cache_purge(); // for valgrind
	return 0;
}

static int read_parameters(blind_t* bp)
{
	solver_t* sp = &(bp->solver);
	for (;;) {
		char buffer[10240];
		char* nextword;
		char* line;
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			return -1;
		}
		line = buffer;
		// strip off newline
		if (line[strlen(line) - 1] == '\n')
			line[strlen(line) - 1] = '\0';

		// skip leading whitespace:
		while (*line && isspace(*line))
			line++;

		logverb("Command: %s\n", line);

		if (line[0] == '#') {
			//logmsg("Skipping comment.\n");
			continue;
		}
		// skip blank lines.
		if (line[0] == '\0') {
			continue;
		}
		if (is_word(line, "help", &nextword)) {
			logmsg("No help soup for you!\n  (use the source, Luke)\n");
		} else if (is_word(line, "idfile", &nextword)) {
			bp->use_idfile = TRUE;
		} else if (is_word(line, "verify ", &nextword)) {
			sl_append(bp->verify_wcsfiles, nextword);
		} else if (is_word(line, "verify_wcs ", &nextword)) {
			tan_t wcs;
			if (sscanf(nextword, "%lg %lg %lg %lg %lg %lg %lg %lg",
			           &(wcs.crval[0]), &(wcs.crval[1]),
			           &(wcs.crpix[0]), &(wcs.crpix[1]),
			           &(wcs.cd[0][0]), &(wcs.cd[0][1]),
			           &(wcs.cd[1][0]), &(wcs.cd[1][1])) != 8) {
				logerr("Failed to parse verify_wcs entry.\n");
				continue;
			}
			bl_append(bp->verify_wcs_list, &wcs);
		} else if (is_word(line, "cpulimit ", &nextword)) {
			bp->cpulimit = atoi(nextword);
		} else if (is_word(line, "timelimit ", &nextword)) {
			bp->timelimit = atoi(nextword);
		} else if (is_word(line, "total_timelimit ", &nextword)) {
			bp->total_timelimit = atoi(nextword);
		} else if (is_word(line, "total_cpulimit ", &nextword)) {
			bp->total_cpulimit = atoi(nextword);
		} else if (is_word(line, "verify_dist ", &nextword)) {
			bp->verify_dist2 = arcsec2distsq(atof(nextword));
		} else if (is_word(line, "verify_pix ", &nextword)) {
			sp->verify_pix = atof(nextword);
		} else if (is_word(line, "nsolves ", &nextword)) {
			bp->nsolves = atoi(nextword);
		} else if (is_word(line, "ratio_tosolve ", &nextword)) {
			bp->logratio_tosolve = log(atof(nextword));
		} else if (is_word(line, "ratio_tokeep ", &nextword)) {
			bp->logratio_tokeep = log(atof(nextword));
		} else if (is_word(line, "ratio_toprint ", &nextword)) {
			bp->logratio_toprint = log(atof(nextword));
		} else if (is_word(line, "ratio_tobail ", &nextword)) {
			sp->logratio_bail_threshold = log(atof(nextword));
		} else if (is_word(line, "match ", &nextword)) {
			free(bp->matchfname);
			bp->matchfname = strdup(nextword);
		} else if (is_word(line, "indexrdls ", &nextword)) {
			free(bp->indexrdlsfname);
			bp->indexrdlsfname = strdup(nextword);
		} else if (is_word(line, "indexrdls_solvedonly", &nextword)) {
			bp->indexrdls_solvedonly = TRUE;
		} else if (is_word(line, "indexrdls_expand ", &nextword)) {
			bp->indexrdls_expand = atof(nextword);
		} else if (is_word(line, "solved ", &nextword)) {
			free(bp->solved_in);
			free(bp->solved_out);
			bp->solved_in = strdup(nextword);
			bp->solved_out = strdup(nextword);
		} else if (is_word(line, "solved_in ", &nextword)) {
			free(bp->solved_in);
			bp->solved_in = strdup(nextword);
		} else if (is_word(line, "solved_out ", &nextword)) {
			free(bp->solved_out);
			bp->solved_out = strdup(nextword);
		} else if (is_word(line, "cancel ", &nextword)) {
			free(bp->cancelfname);
			bp->cancelfname = strdup(nextword);
		} else if (is_word(line, "log ", &nextword)) {
			// Open the log file...
			free(bp->logfname);
			bp->logfname = strdup(nextword);
			blind_setup_logging(bp);
		} else if (is_word(line, "solvedserver ", &nextword)) {
			free(bp->solvedserver);
			bp->solvedserver = strdup(nextword);
		} else if (is_word(line, "silent", &nextword)) {
			bp->silent = TRUE;
		} else if (is_word(line, "quiet", &nextword)) {
			bp->quiet = TRUE;
		} else if (is_word(line, "verbose", &nextword)) {
			bp->verbose = TRUE;
		} else if (is_word(line, "tweak_skipshift", &nextword)) {
			bp->tweak_skipshift = TRUE;
		} else if (is_word(line, "tweak_aborder ", &nextword)) {
			bp->tweak_aborder = atoi(nextword);
		} else if (is_word(line, "tweak_abporder ", &nextword)) {
			bp->tweak_abporder = atoi(nextword);
		} else if (is_word(line, "tweak", &nextword)) {
			bp->do_tweak = TRUE;
		} else if (is_word(line, "wcs ", &nextword)) {
			free(bp->wcs_template);
			bp->wcs_template = strdup(nextword);
		} else if (is_word(line, "fieldid_key ", &nextword)) {
			free(bp->fieldid_key);
			bp->fieldid_key = strdup(nextword);
		} else if (is_word(line, "maxquads ", &nextword)) {
			sp->maxquads = atoi(nextword);
		} else if (is_word(line, "maxmatches ", &nextword)) {
			sp->maxmatches = atoi(nextword);
		} else if (is_word(line, "xcol ", &nextword)) {
			free(bp->xcolname);
			bp->xcolname = strdup(nextword);
		} else if (is_word(line, "ycol ", &nextword)) {
			free(bp->ycolname);
			bp->ycolname = strdup(nextword);
		} else if (is_word(line, "index ", &nextword)) {
			sl_append(bp->indexnames, nextword);
		} else if (is_word(line, "indexes_inparallel", &nextword)) {
			bp->indexes_inparallel = TRUE;
		} else if (is_word(line, "field ", &nextword)) {
			free(bp->fieldfname);
			bp->fieldfname = strdup(nextword);
		} else if (is_word(line, "fieldw ", &nextword)) {
			sp->field_maxx = atof(nextword);
		} else if (is_word(line, "fieldh ", &nextword)) {
			sp->field_maxy = atof(nextword);
		} else if (is_word(line, "distractors ", &nextword)) {
			sp->distractor_ratio = atof(nextword);
		} else if (is_word(line, "fieldid ", &nextword)) {
			bp->fieldid = atoi(nextword);
		} else if (is_word(line, "start ", &nextword)) {
			free(bp->startfname);
			bp->startfname = strdup(nextword);
		} else if (is_word(line, "done ", &nextword)) {
			free(bp->donefname);
			bp->donefname = strdup(nextword);
		} else if (is_word(line, "donescript ", &nextword)) {
			free(bp->donescript);
			bp->donescript = strdup(nextword);
		} else if (is_word(line, "sdepth ", &nextword)) {
			sp->startobj = atoi(nextword);
		} else if (is_word(line, "depth ", &nextword)) {
			sp->endobj = atoi(nextword);
		} else if (is_word(line, "tol ", &nextword)) {
			sp->codetol = atof(nextword);
		} else if (is_word(line, "parity ", &nextword)) {
			sp->parity = atoi(nextword);
		} else if (is_word(line, "fieldunits_lower ", &nextword)) {
			sp->funits_lower = atof(nextword);
		} else if (is_word(line, "fieldunits_upper ", &nextword)) {
			sp->funits_upper = atof(nextword);
		} else if (is_word(line, "firstfield ", &nextword)) {
			bp->firstfield = atoi(nextword);
		} else if (is_word(line, "lastfield ", &nextword)) {
			bp->lastfield = atoi(nextword);
		} else if (is_word(line, "fields ", &nextword)) {
			char* str = nextword;
			char* endp;
			int i, firstfld = -1;
			for (;;) {
				int fld = strtol(str, &endp, 10);
				if (str == endp) {
					// non-numeric value
					logerr("Couldn't parse: %.20s [etc]\n", str);
					break;
				}
				if (firstfld == -1) {
					il_insert_unique_ascending(bp->fieldlist, fld);
				} else {
					if (firstfld > fld) {
						logerr("Ranges must be specified as <start>/<end>: %i/%i\n", firstfld, fld);
					} else {
						for (i = firstfld + 1; i <= fld; i++) {
							il_insert_unique_ascending(bp->fieldlist, i);
						}
					}
					firstfld = -1;
				}
				if (*endp == '/')
					firstfld = fld;
				if (*endp == '\0')
					// end of string
					break;
				str = endp + 1;
			}
		} else if (is_word(line, "run", &nextword)) {
			return 0;
		} else if (is_word(line, "quit", &nextword)) {
			return 1;

		} else if (is_word(line, "nverify ", &nextword)) {
			logmsg("DEPRECATED: \"nverify\" command.\n");
		} else if (is_word(line, "nindex_tosolve ", &nextword)) {
			logmsg("DEPRECATED: \"nindex_tosolve\" command.\n");
		} else if (is_word(line, "nindex_tokeep ", &nextword)) {
			logmsg("DEPRECATED: \"nindex_tokeep\" command.\n");

		} else {
			logmsg("I didn't understand this command:\n  \"%s\"\n", line);
		}
	}
}
