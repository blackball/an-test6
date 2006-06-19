#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "xylist.h"

char* OPTIONS = "hf:v:x:y:n:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s \n"
			"   <-f xyls-file-name>\n"
			"   [-n field-number]\n"
			"   [-v variable-name]\n"
			"   [-x x-column-name]\n"
			"   [-y y-column-name]\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
  int argchar;
  char* progname = args[0];
  char* filename = NULL;
  char* varname = "xy";
  char* xname = NULL;
  char* yname = NULL;
  int i;
  xylist* xyls;
  xy* field;
  int fieldnum = 0;

  while ((argchar = getopt (argc, args, OPTIONS)) != -1) {
	  switch (argchar) {
	  case '?':
	  case 'h':
		  printHelp(progname);
		  exit(0);
	  case 'n':
		  fieldnum = atoi(optarg);
		  break;
	  case 'f':
		  filename = optarg;
		  break;
	  case 'v':
		  varname = optarg;
		  break;
	  case 'x':
		  xname = optarg;
		  break;
	  case 'y':
		  yname = optarg;
		  break;
	  }
  }

  if (!filename) {
	  printHelp(progname);
	  exit(-1);
  }

  xyls = xylist_open(filename);
  if (!xyls) {
	  fprintf(stderr, "Couldn't open xylist %s\n", filename);
	  exit(-1);
  }

  if (xname)
	  xyls->xname = xname;
  if (yname)
	  xyls->yname = yname;

  field = xylist_get_field(xyls, fieldnum);
  if (!field) {
	  fprintf(stderr, "Failed to read field %i from file %s\n", fieldnum, filename);
	  exit(-1);
  }

  printf("%s = [", varname);
  for (i=0; i<xy_size(field); i++) {
	  printf("%g,%g;\n", xy_refx(field, i), xy_refy(field, i));
  }
  printf("];\n");
  free_xy(field);

  xylist_close(xyls);
  return 0;
}
