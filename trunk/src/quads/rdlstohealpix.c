#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "healpix.h"
#include "starutil.h"
#include "rdlist.h"
#include "bl.h"

char* OPTIONS = "hqf:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options]\n"
			"   -f <rdls-file>\n"
			"   [-h] print help msg\n"
			"   [-q] quiet mode\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
  char* filename = NULL;
  int npoints;
  int i, j;
  int healpixes[12];
  int argchar;
  char* progname = args[0];
  il* lists[12];
  bool quiet = FALSE;
  rdlist* rdls;

  while ((argchar = getopt (argc, args, OPTIONS)) != -1)
	  switch (argchar) {
	  case 'f':
		  filename = optarg;
		  break;
	  case 'h':
		  printHelp(progname);
		  exit(0);
	  case 'q':
		  quiet = TRUE;
		  break;
	  case '?':
		  fprintf(stderr, "Unknown option `-%c'.\n", optopt);
	  default:
		  exit(-1);
	  }

  if (!filename) {
	  printHelp(progname);
	  exit(-1);
  }

  fprintf(stderr, "Opening RDLS file %s...\n", filename);
  rdls = rdlist_open(filename);
  if (!rdls) {
	  fprintf(stderr, "Failed to open RDLS file.\n");
	  exit(-1);
  }

  for (i=0; i<12; i++) {
	  lists[i] = il_new(256);
  }

  for (j=0; j<rdls->nfields; j++) {
	  rd* points;

	  points = rdlist_get_field(rdls, j);
	  if (!points) {
		  fprintf(stderr, "error reading field %i\n", j);
		  break;
	  }

	  for (i=0; i<12; i++) {
		  healpixes[i] = 0;
	  }

	  npoints = rd_size(points);

	  for (i=0; i<npoints; i++) {
		  double ra, dec;
		  int hp;

		  ra  = rd_refra (points, i);
		  dec = rd_refdec(points, i);

		  ra  *= M_PI / 180.0;
		  dec *= M_PI / 180.0;

		  hp = radectohealpix(ra, dec);
		  if ((hp < 0) || (hp >= 12)) {
			  printf("hp=%i\n", hp);
			  continue;
		  }
		  healpixes[hp] = 1;
	  }
	  if (!quiet) {
		  printf("Field %i: healpixes  ", j);
		  for (i=0; i<12; i++) {
			  if (healpixes[i])
				  printf("%i  ", i);
		  }
		  printf("\n");
	  }

	  for (i=0; i<12; i++)
		  if (healpixes[i])
			  il_append(lists[i], j);

	  free_rd(points);
  }

  for (i=0; i<12; i++) {
	  int N = il_size(lists[i]);
	  printf("HP %i: ", i);
	  for (j=0; j<N; j++)
		  printf("%i ", il_get(lists[i], j));
	  printf("\n");
	  il_free(lists[i]);
  }

  rdlist_close(rdls);
  return 0;
}
