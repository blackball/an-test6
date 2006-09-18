#ifndef __STARLIST_HPP__
#define __STARLIST_HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// indicates a star is not cataloged
#define DIST_STAR_INDEX   MAX_DWORD

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * StarList: A class for containing a simple catalog of stars
 */
class StarList
{
public:
  double *List[3];      // ra, dec, brightness
  double Bounds[2][2];  // depricated
  DWORD  StarCount;     // number of stars in the catalog
  double *List3D[3];    // star positions in 3D coordinates on the unit sphere

public:
  StarList();
  ~StarList();

  // init the catalog space
  void Init(DWORD count);
  // shutdown
  void Deinit();

  // save the catalog in binary format
  BOOL32 Save(char *fname);
  // save in text format
  BOOL32 SaveAsText(char *fname);
  // open a binary catalog
  BOOL32 Open(char *fname);
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
