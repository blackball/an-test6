/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <MyTypes.h>
#include <BlockMemBank.hpp>
#include "StarCamera.hpp"
#include "StarList.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * Converts a text catalog where each line is of the form:
 * <id> <x> <y> <z>
 * into a binary file readable by the StarList class.
 * Usage: <text_cat_name> <bin_cat_name>
 * <text_cat_name>   : input text catalog name
 * <bin_cat_name>    : output binary catalog name
 */
int main(int argc, char *argv[])
{
  char *in_name;
  char *out_name;
  FILE *in;
  DWORD starCount, k;
  StarList cList;
  double *dir, tspace[3], ra, dec;
  DWORD i;
  BlockMemBank tbank;
  Enumeration *tenum;

  // output usage
  if (argc < 3)
  {
    printf("\n");
    printf("Usage: <text_cat_name> <bin_cat_name>\n");
    printf("<text_cat_name>   : input text catalog name\n");
    printf("<bin_cat_name>    : output binary catalog name\n");
    printf("\n");
    return 0;
  }

  // grab parameters
  in_name = argv[1];
  out_name = argv[2];

  // init a temporary databank
  tbank.Init(sizeof(double)*3, 10000);
  
  // open the file
  in = fopen(in_name, "rb");
  if (in == NULL)
  {
    printf("Error opening file: %s\n", in_name);
    return -1;
  }
  printf("Reading text file\n");
  fflush(stdout);
  starCount = 0;
  // read all the lines in the file
  k = 0;
  while (!feof(in))
  {
    if (fscanf(in, "%u %lf %lf %lf", &i, &tspace[0], &tspace[1], &tspace[2]) < 4)
    {
      break;
    }
    tbank.AddNewData((void*)tspace);
    starCount++;
    if ((starCount % 100) == 0)
    {
      printf(".");
      k++;
      if ((k%60) == 0)
      {
        printf("  %u\n", starCount);
      }
      fflush(stdout);
    }
  }
  fclose(in);

  // init the binary catalog
  starCount = tbank.GetCount();
  cList.Init(starCount);
  cList.Bounds[0][0] = 0;
  cList.Bounds[0][1] = 360;
  cList.Bounds[1][0] = 0;
  cList.Bounds[1][1] = 180;
  tenum = tbank.NewEnum(FORWARD);
  // copy all the data into the catalog
  for (i = 0; i < starCount; i++)
  {
    dir = (double*)tenum->GetEnumNext();

    ra  = atan2(dir[1],dir[0])*DEG_CONV;
    dec = acos(dir[2])*DEG_CONV;

    cList.List[0][i] = ra;
    cList.List[1][i] = dec;
    cList.List[2][i] = 0;
  }

  // save the results
  if (cList.Save(out_name) == FALSE32)
  {
    printf("Error: unable to save catalog: %s\n", out_name);
    return -1;
  }

  // finished
  printf("Done\n");

  return 0;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
