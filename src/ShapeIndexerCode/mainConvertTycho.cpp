/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <stdio.h>
#include "Tycho2Reader.hpp"
#include "StarList.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// tycho catalog
#define TYCHO_CATALOG "F:\\Tycho-2\\data\\catalog.dat"
// tycho index
#define TYCHO_INDEX   "F:\\Tycho-2\\data\\index.dat"
// output file
#define TYCHO_OUTPUT  "cat.tycho.bin"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * Coverts the Tycho2 catalog into a binary catalog readable by the
 * StarList class.
 * Usage: Just run -- all file names are defined above
 */
int main(int arg, char *argv[])
{
  Tycho2Reader tychoRead;
  StarList     sList;
  TychoStarRecord trec;
  double minRa, maxRa, minDec, maxDec;
  DWORD i, goal;

  // init the tycho catalog
  if (tychoRead.Init(TYCHO_CATALOG, TYCHO_INDEX) == FALSE32)
  {
    printf("Unable to open Tycho2 database.\n");
    return -1;
  }
  
  // init the binary catalog space
  sList.Init((DWORD)tychoRead.GetStarCount());

  // set the first star
  tychoRead.GetStar(0, &trec);
  sList.List[0][0] = minRa  = maxRa  = trec.Ra;
  sList.List[1][0] = minDec = maxDec = trec.Dec;
  printf(".");

  // read and copy all stars from the tycho2 catalog
  goal = (DWORD)tychoRead.GetStarCount();
  for (i = 1; i < goal; i++)
  {
    tychoRead.GetNextStar(&trec);
    sList.List[0][i] = trec.Ra;
    sList.List[1][i] = trec.Dec;
    sList.List[2][i] = trec.BTMag;

    if (trec.Ra < minRa)
      minRa = trec.Ra;
    if (trec.Ra > maxRa)
      maxRa = trec.Ra;
    if (trec.Dec < minDec)
      minDec = trec.Dec;
    if (trec.Dec > maxDec)
      maxDec = trec.Dec;

    printf(".");
    if ((i % 60) == 0)
      printf(" %u/%u\n", i+1, goal);
    fflush(stdout);
  }
  printf("\n");

  // set bounds
  sList.Bounds[0][0] = minRa;
  sList.Bounds[0][1] = maxRa;
  sList.Bounds[1][0] = minDec;
  sList.Bounds[1][1] = maxDec;

  // close the tycho2 catalog
  tychoRead.Deinit();

  // save the binary file
  if (sList.Save(TYCHO_OUTPUT) == FALSE32)
  {
    printf("Error: unable to save catalog.\n");
    return -1;
  }

  sList.Deinit();

  // finished
  printf("Min RA : %lg\n", minRa);
  printf("Max RA : %lg\n", maxRa);
  printf("Min DEC: %lg\n", minDec);
  printf("Max DEC: %lg\n", maxDec);
  printf("Done\n");

  return 0;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

