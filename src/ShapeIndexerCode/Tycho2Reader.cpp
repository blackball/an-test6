/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <BlockMemBank.hpp>
#include <memory.h>
#include "Tycho2Reader.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// Size of an index record (in bytes).
#define INDEX_REC_SIZE    44
// Size of a catalog record (in bytes).
#define CAT_REC_SIZE      208

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * Tycho2Reader: Initialize the reader to a base state.
 */
Tycho2Reader::Tycho2Reader()
{
  Index     = NULL;
  IndexCnt  = 0;
  CatPtr    = NULL;
  StarCount = NULL;
  CurPos    = 0;
}

/* -------------------------------------------------- */

/*
 * ~Tycho2Reader: Shutdown.
 */
Tycho2Reader::~Tycho2Reader()
{
  Deinit();
}

/* -------------------------------------------------- */

/*
 * Deinit: Cleanup and shutdown the reader.
 * Assumes: <none>
 */
void Tycho2Reader::Deinit()
{
  if (Index != NULL)
  {
    delete [] Index;
  }
  Index = NULL;
  IndexCnt = 0;
  if (CatPtr != NULL)
  {
    fclose(CatPtr);
    CatPtr = NULL;
  }

  StarCount = 0;
  CurPos    = 0;
}

/* -------------------------------------------------- */

/*
 * Init: Initializes the reader to the appropriate Tycho2 catalog by using
 *       the supplied catalog file 'fcatName', and the provided index file 'findexName'.
 * Assumes: 'fcatName' and 'findexName' refer to a valid Tycho2 catalog and associated index file
 *          respectively.
 */
BOOL32 Tycho2Reader::Init(char *fcatName, char *findexName)
{
  FILE *in;
  char inBuffer[INDEX_REC_SIZE];
  DWORD endPos, i;

  Deinit();

  // read in index
  in = fopen(findexName, "rb");
  if (in == NULL)
  {
    return FALSE32;
  }
  // determine the size of the index file
  if (fseek(in, 0, SEEK_END) != 0)
  {
    return FALSE32;
  }
  endPos = ftell(in);
  if (fseek(in, 0, SEEK_SET) != 0)
  {
    return FALSE32;
  }
  // calculate the number of index records
  IndexCnt = endPos/INDEX_REC_SIZE;
  // read all the index records
  Index = new TychoIndex[IndexCnt];
  for (i = 0; i < IndexCnt; i++)
  {
    // read in rec
    if (fread(inBuffer, INDEX_REC_SIZE, 1, in) < 0)
    {
      fclose(in);
      return FALSE32;
    }

    // parse record
    sscanf(inBuffer, "%7u|%6u|%6g|%6g|%6g|%6g", &Index[i].t2Rec, &Index[i].s1Rec, &Index[i].RaMin, 
                                                &Index[i].RaMax, &Index[i].DecMin, &Index[i].DecMax);
    Index[i].t2Rec  -= 1;
    Index[i].s1Rec  -= 1;
    Index[i].DecMin += 90;
    Index[i].DecMax += 90;
  }
  fclose(in);

  // read in catalog info
  CatPtr = fopen(fcatName, "rb");
  if (CatPtr == NULL)
  {
    return FALSE32;
  }
  // determine the number of records in the catalog
  if (fseek(CatPtr, 0, SEEK_END) != 0)
  {
    return FALSE32;
  }
  endPos = ftell(CatPtr);
  if (fseek(CatPtr, 0, SEEK_SET) != 0)
  {
    return FALSE32;
  }
  // calculate the corrsponding star count
  StarCount = endPos/CAT_REC_SIZE;

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * GetStarCount: Returns the number of stars in the open catalog.
 * Assumes: The Tycho2 catalog is already open.
 */
DWORD Tycho2Reader::GetStarCount()
{
  return StarCount;
}

/* -------------------------------------------------- */

/*
 * GetCurPos: Returns the current position of the catalog reader.
 * Assumes: The Tycho2 catalog is already open.
 */
DWORD Tycho2Reader::GetCurPos()
{
  return CurPos;
}

/* -------------------------------------------------- */

/*
 * ReadRec: Reads a single record from the open catalog. Returns TRUE32
 *          if the read succeeds, FALSE32 otherwise.
 * Assumes: The Tycho2 catalog is already open, 'rec' refers to a valid 
 *          destination record.
 */
BOOL32 Tycho2Reader::ReadRec(TychoStarRecord *rec)
{
  char inBuffer[CAT_REC_SIZE];

  // read in rec
  if (fread(inBuffer, CAT_REC_SIZE, 1, CatPtr) < 0)
  {
    return FALSE32;
  }

  // blank out the record
  memset(rec, 0, sizeof(TychoStarRecord));

  // parse record -- parsed this way because sometimes
  // info is left blank rather than flagged
  sscanf(inBuffer,    "%4d", &rec->tyc1);
  sscanf(inBuffer+5,  "%5d", &rec->tyc2);
  sscanf(inBuffer+11, "%1d", &rec->tyc3);
  sscanf(inBuffer+13, "%12f", &rec->pFlag);
  sscanf(inBuffer+15, "%12f", &rec->mRa);
  sscanf(inBuffer+28, "%12f", &rec->mDec);
  sscanf(inBuffer+41, "%7f",  &rec->pmRa);
  sscanf(inBuffer+49, "%7f",  &rec->pmDec);
  sscanf(inBuffer+57, "%3d",  &rec->errmRa);
  sscanf(inBuffer+61, "%3d",  &rec->errmDec);
  sscanf(inBuffer+49, "%7f",  &rec->errPmRa);
  sscanf(inBuffer+57, "%4f",  &rec->errPmDec);
  sscanf(inBuffer+61, "%4f",  &rec->errDec);
  sscanf(inBuffer+65, "%4f",  &rec->errPmRa);
  sscanf(inBuffer+70, "%4f",  &rec->errPmDec);
  sscanf(inBuffer+75, "%7f",  &rec->mepRa);
  sscanf(inBuffer+83, "%7f",  &rec->mepDec);
  sscanf(inBuffer+91, "%2d",  &rec->posUsed);
  sscanf(inBuffer+94, "%3f",  &rec->g_mRa);
  sscanf(inBuffer+98, "%3f",  &rec->g_mDec);
  sscanf(inBuffer+102,"%3f",  &rec->g_pmRa);
  sscanf(inBuffer+106,"%3f",  &rec->g_pmDec);
  sscanf(inBuffer+111,"%6f",  &rec->BTMag);
  sscanf(inBuffer+117,"%5f",  &rec->errBTMag);
  sscanf(inBuffer+123,"%6f",  &rec->VTMag);
  sscanf(inBuffer+130,"%5f",  &rec->errVTMag);
  sscanf(inBuffer+136,"%3d",  &rec->prox);
  sscanf(inBuffer+140,"%c",   &rec->tycho);
  sscanf(inBuffer+142,"%6c",  &rec->hip);
  sscanf(inBuffer+148,"%3c",  &rec->CCDM);
  sscanf(inBuffer+152,"%12f", &rec->Ra);
  sscanf(inBuffer+165,"%12f", &rec->Dec);
  sscanf(inBuffer+178,"%4f",  &rec->epRa);
  sscanf(inBuffer+183,"%4f",  &rec->epDec);
  sscanf(inBuffer+188,"%5f",  &rec->errRa);
  sscanf(inBuffer+194,"%5f",  &rec->errDec);
  sscanf(inBuffer+200,"%c",   &rec->posFlag);
  sscanf(inBuffer+202,"%4f",  &rec->corr);

  // end strings
  rec->hip[6]   = '\0';
  rec->CCDM[3]  = '\0';
  // adjust values
  rec->mDec    += 90.0f;
  rec->Dec     += 90.0f;

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * GetStar: Returns in 'rec' a particular star 'recNum' in the catalog. The reader
 *          moves to the appropriate read position, reads the record, and 
 *          end in the read position after the record. Returns TRUE32 if the star is
 *          read successfully, FALSE32 otherwise.
 * Assumes: 'rec' is a valid destination record. The catalog is already open.
 */
BOOL32 Tycho2Reader::GetStar(DWORD recNum, TychoStarRecord *rec)
{
  // check the validity of the requested star
  if (recNum >= StarCount)
  {
    return FALSE32;
  }
  // move to the star position
  if (recNum != CurPos)
  {
    if (fseek(CatPtr, (DWORD)recNum*CAT_REC_SIZE, SEEK_SET) != 0)
    {
      return FALSE32;
    }
  }
  // read the record
  if (ReadRec(rec) == FALSE32)
  {
    return FALSE32;
  }
  // update the read position
  CurPos = recNum+1;

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * GetNextStar: Returns in 'rec' the next available star, in order, from the Tycho2 catalog.
 *              Returns TRUE32 id the read succeeds, FALSE32 otherwise.
 * Assumes: 'rec' is a valid destination record. The catalog is already open.
 */
BOOL32 Tycho2Reader::GetNextStar(TychoStarRecord *rec)
{
  // check validity of the read position
  if (CurPos >= StarCount)
  {
    return FALSE32;
  }

  // read record
  ReadRec(rec);
  // update read position
  CurPos++;

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * GetRecCnt: Returns the number of stars referenced by a particular index 'i'.
 * Assumes: 'i' < Number of indices. The catalog is already open.
 */
DWORD Tycho2Reader::GetRecCnt(DWORD i)
{
  // if the last index is checked, use number of stars in catalog
  // to determine the number in the region
  if (i == (IndexCnt-1))
  {
    return StarCount-Index[i].t2Rec;
  }

  // calculate the number of stars in the requested region
  return Index[i+1].t2Rec-Index[i].t2Rec;
}

/* -------------------------------------------------- */

/*
 * GetRegion: Returns the stars within the region defined by [minRa, maxRa] and [minDec, maxDec].
 *            The count of stars is returned via 'dcount', and the stars themselves via 'dlist'.
 * Assumes: 'dlist' is a valid record pointer-pointer, and 'dcount' is a valid DWORD pointer. The Tycho2 
 *          catalog is already open.
 */
BOOL32 Tycho2Reader::GetRegion(TychoStarRecord **dlist, DWORD *dcount, double minRa, double maxRa, double minDec, double maxDec)
{
  DWORD i, j;
  BlockMemBank starBank;
  TychoStarRecord trec;
  Enumeration *tenum;
  BYTE *bptr;
  DWORD recCnt;
  BOOL32 vertCatch;
  BOOL32 horzCatch;

  // initialize a temporary bank
  starBank.Init(sizeof(TychoStarRecord), 100);

  // check all indexes
  for (i = 0; i < IndexCnt; i++)
  {
    // if index refers to a region outside the requested region, simply ignore the index
    if (((Index[i].RaMax < minRa) || (Index[i].RaMin > maxRa)) || ((Index[i].DecMax < minDec) || (Index[i].DecMin > maxDec)))
    {
    } else
    {
      // if index sits completely within the requested region, grab all stars in the index
      if (((Index[i].RaMin >= minRa) && (Index[i].RaMax <= maxRa)) && ((Index[i].DecMin >= minDec) && (Index[i].DecMax <= maxDec)))
      {
        fseek(CatPtr, Index[i].t2Rec*CAT_REC_SIZE, SEEK_SET);
        recCnt = GetRecCnt(i);
        for (j = 0; j < recCnt; j++)
        {
          if (ReadRec(&trec) == FALSE32)
          {
            return FALSE32;
          }
          starBank.AddNewData((BYTE*)&trec);
        }
      } else
      {
        // determine if the index intersects the requested region in both RA and DEC
        if (((Index[i].RaMin <= minRa) && (Index[i].RaMax >= maxRa)) && ((Index[i].DecMin <= minDec) && (Index[i].DecMax >= maxDec)))
        {
          horzCatch = vertCatch = TRUE32;
        } else
        {
          if (((minRa >= Index[i].RaMin) && (minRa <= Index[i].RaMax)) || ((maxRa >= Index[i].RaMin) && (maxRa <= Index[i].RaMax)))
          {
            vertCatch = TRUE32;
          } else
          {
            vertCatch = FALSE32;
          }
          if (((minDec >= Index[i].DecMin) && (minDec <= Index[i].DecMax)) || ((maxDec >= Index[i].DecMin) && (maxDec <= Index[i].DecMax)))
          {
            horzCatch = TRUE32;
          } else
          {
            horzCatch = FALSE32;
          }
        }
        if ((vertCatch == TRUE32) && (horzCatch == TRUE32))
        {
          // read all the records in the indexed region and return those that fall within the 
          // requested area
          fseek(CatPtr, Index[i].t2Rec*CAT_REC_SIZE, SEEK_SET);
          recCnt = GetRecCnt(i);
          for (j = 0; j < recCnt; j++)
          {
            if (ReadRec(&trec) == FALSE32)
            {
              return FALSE32;
            }
            if (((trec.Ra >= minRa) && (trec.Ra <= maxRa)) && ((trec.Dec >= minDec) && (trec.Dec <= maxDec)))
            {
              starBank.AddNewData((BYTE*)&trec);
            }
          }

        }
      }
    }
  }

  // copy the stars we found (if we found any) for the caller
  if (starBank.GetCount() > 0)
  {
    *dlist = new TychoStarRecord[starBank.GetCount()];
    *dcount = starBank.GetCount();
    j = 0;
    tenum = starBank.NewEnum(FORWARD);
    while (tenum->GetEnumOK() == TRUE32)
    {
      bptr = (BYTE*)tenum->GetEnumNext();
      memcpy(&(*dlist)[j], bptr, sizeof(TychoStarRecord));
      j++;
    }
    delete tenum;
    starBank.Empty();
  } else
  {
    *dcount = 0;
    *dlist  = NULL;
  }

  // return the read pointer to the start of the catalog
  CurPos = 0;
  fseek(CatPtr, 0, SEEK_SET);

  return TRUE32;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
