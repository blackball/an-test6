/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <MathExt.h>
#include <MyTypes.h>
#include "StarList.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/**
 * Builds a catalog of uniform random stars, saving the results in a file
 * readable by the StarList class.
 * Usage: <catalog_name> <star_count>
 * <cat_name>      : output catalog name
 * <star_count>    : number of stars to generate
 */
int main(int argc, char *argv[])
{
  char *cat_name;
  DWORD starCount;
  BOOL32 iGood;
  StarList cList;
  double ra, dec;
  DWORD i;

  // output usage as needed
  if (argc < 3)
  {
    printf("\n");
    printf("Usage: <cat_name> <star_count>\n");
    printf("<cat_name>      : output catalog name\n");
    printf("<star_count>    : number of stars to generate\n");
    printf("\n");
    return 0;
  }

  // set the input parameters
  iGood = TRUE32;
  cat_name = argv[1];
  if (sscanf(argv[2], "%u", &starCount) < 1)
  {
    iGood = FALSE32;
  }

  // check parameters
  if (iGood == FALSE32)
  {
    printf("Error parsing command line\n");
    return -1;
  }

  // init the randomization
  srand(clock());

  // init catalog
  cList.Init(starCount);
  cList.Bounds[0][0] = 0;
  cList.Bounds[0][1] = 360;
  cList.Bounds[1][0] = 0;
  cList.Bounds[1][1] = 180;

  // generate the random values
  for (i = 0; i < starCount; i++)
  {
    randSphereD(&ra, &dec);

    cList.List[0][i] = ra  * DEG_CONV;
    cList.List[1][i] = dec * DEG_CONV;
    cList.List[2][i] = 0;
  }

  // save the catalog
  if (cList.Save(cat_name) == FALSE32)
  {
    printf("Error: unable to save catalog: %s\n", cat_name);
    return -1;
  }

  // finished
  printf("Done\n");

  return 0;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
