/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <stdio.h>
#include <MyTypes.h>
#include "StarShapeSys.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * Outputs the distance ratios and angles of all the shapes in an index into 
 * text files. This is useful for checking the distribution of values.
 *
 * Usage: <index_file> <ratio_file> <angle_file>
 * <index_file>    : index file to open
 * <ratio_file>    : file to output ratios into
 * <angle_file>    : file to output angles into
 */
int main(int argc, char *argv[])
{
  StarShapeSys sys;
  char *index_name;
  char *ratio_name;
  char *angle_name;

  // output usage
  if (argc < 4)
  {
    printf("\n");
    printf("Usage: <index_file> <ratio_file> <angle_file>\n");
    printf("<index_file>    : index file to open\n");
    printf("<ratio_file>    : file to output ratios into\n");
    printf("<angle_file>    : file to output angles into\n");
    printf("\n");
    return -1;
  }
  // grab params
  index_name = argv[1];
  ratio_name = argv[2];
  angle_name = argv[3];

  // open the index
  printf("Opening index...\n");
  fflush(stdout);
  if (sys.OpenIndex(index_name) == FALSE32)
  {
    printf("Error opening index file: %s\n", index_name);
    return -1;
  }
  printf("Index %s opened\n", index_name);
  fflush(stdout);

  // save the ratios
  printf("Saving Ratios...\n");
  fflush(stdout);
  if (sys.SaveRatiosAsText(ratio_name) == FALSE32)
  {
    printf("Error creating ratio file: %s\n", ratio_name);
    return -1;
  }

  // save the angles
  printf("Saving Angles...\n");
  fflush(stdout);
  if (sys.SaveAnglesAsText(angle_name) == FALSE32)
  {
    printf("Error creating angle file: %s\n", angle_name);
    return -1;
  }

  printf("Fin.\n");
  fflush(stdout);

  return 0;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
