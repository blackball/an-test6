#ifndef __TYCHO2READER_HPP__
#define __TYCHO2READER_HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>
#include <stdio.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * TychoStarRecord: Holds a single record from the Tycho2 catalog. See 'format.txt' for
 *                  a more complete rundown of the meaning of each value used below.
 */
struct TychoStarRecord
{
  int   tyc1;       // Tycho id 1
  int   tyc2;       // Tycho id 2
  int   tyc3;       // Tycho id 3

  char   pFlag;     // Mean position
  float  mRa;       // Mean RA, epoch J2000
  float  mDec;      // Mean DEC, epoch J2000
  float  pmRa;      // Proper motion in RA*cos(DEC)
  float  pmDec;     // Proper motion in DEC
  int    errmRa;    // Error of RA in RA*cos(DEC)
  int    errmDec;   // Error of DEC
  float  errPmRa;   // Error of proper motion in RA*cos(DEC)
  float  errPmDec;  // Error of proper motion in DEC
  float  mepRa;     // Mean epoch of RA
  float  mepDec;    // Mean epoch of DEC
  int    posUsed;   // Number of positions used
  float  g_mRa;     // Goodness of mean RA fit
  float  g_mDec;    // Goodness of mean DEC fit
  float  g_pmRa;    // Goodness of proper motion RA
  float  g_pmDec;   // Goodness of proper motion DEC
  float  BTMag;     // Tycho2 BT magnitude
  float  errBTMag;  // Error of BT magnitude
  float  VTMag;     // Tycho2 VT magnitude
  float  errVTMag;  // Error of VY magnitude
  int    prox;      // Proximity indicator of nearest other star in catalog or supplement
  char   tycho;     // Is Tycho1 star
  char   hip[7];    // Hipparcos number
  char   CCDM[4];   // CCDM component number for Hipparcos stars
  float  Ra;        // Observed Tycho2 RA
  float  Dec;       // Observed Tycho2 DEC
  float  epRa;      // Epoch 1990 of RA
  float  epDec;     // Epoch 1990 of DEC
  float  errRa;     // Error of RA in RA*cos(DEC)
  float  errDec;    // Error of DEC
  char   posFlag;   // Tycho of Tycho2 solution
  float  corr;      // Correlation of RA and DEC
};

/* -------------------------------------------------- */

/*
 * TychoIndex: Record used to quickly index into the Tycho2 catalog
 */
struct TychoIndex
{
  DWORD t2Rec;  // Tycho2 record number
  DWORD s1Rec;  // Supplemental record number
  float RaMin;  // RA min of region
  float RaMax;  // RA max of region
  float DecMin; // DEC min of region
  float DecMax; // DEC max of region
};

/* -------------------------------------------------- */

/*
 * Tycho2Reader: A class used of extracting stars from the Tycho2 star catalog. Stars can be extracted on at
 *               a time, or they can be extracted by specifying a region.
 */
class Tycho2Reader
{
protected:
  TychoIndex    *Index;     // List of lookup indices
  DWORD          IndexCnt;  // Number of lookup indices
  FILE          *CatPtr;    // Pointer to catalog file
  DWORD          StarCount; // Number of stars in catalog

  DWORD          CurPos;    // Current position in the catalog file

public:
  Tycho2Reader();
  ~Tycho2Reader();

  // Shutdown catalog
  void   Deinit();
  // Open up catalog for reading
  BOOL32 Init(char *fcatName, char *findexName);
  // Return number of stars in catalog
  DWORD  GetStarCount();
  // Return current position in the file
  DWORD  GetCurPos();

  // Returns the star in position recNum. The current position in the file becomes recNum+1
  BOOL32 GetStar(DWORD recNum, TychoStarRecord *rec);
  // Returns the next available star in the catalog
  BOOL32 GetNextStar(TychoStarRecord *rec);
  // Returns all the stars within the regions bounded by [minRA, maxRA] and [minDec, maxDec].
  BOOL32 GetRegion(TychoStarRecord **dlist, DWORD *dcount, double minRa, double maxRa, double minDec, double maxDec);

protected:
  BOOL32 ReadRec(TychoStarRecord *rec);
  DWORD  GetRecCnt(DWORD i);
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
