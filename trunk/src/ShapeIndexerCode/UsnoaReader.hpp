#ifndef __USNOAREADER_HPP__
#define __USNOAREADER_HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>
#include <stdio.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * AccelInfo: Structure used to jump within the USNOA catalog to particular
 *            regions of the catalog.
 */ 
struct AccelInfo
{
  float BaseRA;     // start right ascension
  DWORD StarPos;    // starting record
  DWORD StartByte;  // byte offset
  DWORD StarCount;  // number of the starting star
};

/* -------------------------------------------------- */

/*
 * StarRecord: Info for a single star in the USNOA catalog.
 */
struct StarRecord
{
  DWORD  StarNum;     // number of the star

  double RA;          // right ascension angle
  double DEC;         // declination angle

  BOOL32 Correlated;  // correlated to a GSC star
  BOOL32 MagIffy;     // indicates the magnitude value is questionable

  int    Field;       // plate used (?)
  double BMag;        // Blue magnitude
  double RMag;        // Red magnitude
};

/* -------------------------------------------------- */

/*
 * UsnoaReaderV2: Class for reading a particular catalog file in the USNOA catalog.
 */
class UsnoaReaderV2
{
protected:
  char *CatName;    // file name
  FILE *CatFP;      // file pointer
  DWORD CatPos;     // current position in the catalog
  char *AccelName;  // name of the accelerator file
  float AccelStep;  // region of the reader in RA
  DWORD StarCount;  // number of stars in the file
  
  DWORD      AccelCount;  // number of accelerator offsets
  AccelInfo *AccelList;   // the actual accelerators
  
  void (UsnoaReaderV2::*readRec)(BYTE *space);  // function pointer to the record reading function.

public:
  UsnoaReaderV2();
  ~UsnoaReaderV2();

  // open the catalog file and associated accelerator file
  BOOL32 Open(char *catname, char *accelname);
  // return a particular star in the file
  BOOL32 GetStar(DWORD recNum, StarRecord *rec);
  // return the next available star in the file
  BOOL32 GetNextStar(StarRecord *rec);
  // return the current star number
  DWORD  GetCurPos();
  // return the number of stars in the file
  DWORD  GetStarCount();

  // return the order of the first star in the file > than the provided 'ra'
  DWORD  FirstBlockNum(double ra);
  // return the number of stars in accelerator block 'num'
  DWORD  AccelBlockStars(DWORD num);
  // return the number of accelerators
  DWORD  GetAccelCount();
  // jump to start of accelerator block 'num'
  BOOL32 SeekBlock(DWORD num);
  // return the starting RA of accelerator block 'num'
  double GetAccelBaseRA(DWORD num);

  // return the name of the reader's catalog file
  char*  GetCatName();
  // return the name of the reader's accelerator file
  char*  GetAccelName();
  // close down the reader
  void   Deinit();

protected:
  void ReadBigERec(BYTE *space);
  void ReadLittleERec(BYTE *space);
};

/* -------------------------------------------------- */

/*
 * UsnoaManagerV2: Manages reading from the complete USNOA star catalog
 */
class UsnoaManagerV2
{
protected:
  DWORD          FCount;    // number of sub-file in the catalog
  char         **FNames;    // names of the catalog files
  double         TotStars;  // total catalog stars
  double         CurStar;   // current catalog star
  UsnoaReaderV2 *Readers;   // list of file readers
  double        *DEC_Starts;// the DEC starting points for each reader
  DWORD          CurOffset; // the current reader star read offset
  DWORD          CurReader; // the current reader number 

public:
  UsnoaManagerV2();
  ~UsnoaManagerV2();

  // shutdown
  void   Deinit();
  // open the catalog
  BOOL32 Init(char **fnames, DWORD fcount);
  // return the star count
  double GetStarCount();
  // return the current star position
  double GetCurPos();

  // return a particular star
  BOOL32 GetStar(double recNum, StarRecord *rec);
  // return the next available star
  BOOL32 GetNextStar(StarRecord *rec);
  // return all the stars in a particular region
  BOOL32 GetRegion(StarRecord **dlist, DWORD *dcount, double minRa, double maxRa, double minDec, double maxDec);
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
