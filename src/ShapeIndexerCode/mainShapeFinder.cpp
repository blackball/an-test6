/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <fstream.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "StarShapeSys.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * Outputs the shapes that a particular star is involved in inside a particular index
 * Usage: <index_file> <star_num>
 * <index_file>       : index file to search
 * <star_num>         : star number to find shapes for
 */
int main(int argc, char *argv[])
{
  StarShapeSys ShapeSys;
  char *index_file;
  DWORD star_num;
  BOOL32 iGood;

  // output usage
  if (argc < 3)
  {
    printf("\n");
    printf("Usage: <index_file> <star_num>\n");
    printf("<index_file>       : index file to search\n");
    printf("<star_num>         : star number to find shapes for\n");
    printf("\n");
    return -1;
  }
  // grab parameters
  iGood = TRUE32;
  index_file  = argv[1];
  if (sscanf(argv[2], "%u", &star_num) < 1)
  {
    iGood = FALSE32;
  }
  // check validity of parameters
  if (iGood == FALSE32)
  {
    printf("Error parsing cammand params\n");
    exit(1);
  }

  // open shape index
  if (ShapeSys.OpenIndex(index_file) == FALSE32)
  {
    printf("Error opening index file: %s\n", index_file);
    return -1;
  }

  // find shapes
  ShapeSys.FindShapes(star_num);

  return 0;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
