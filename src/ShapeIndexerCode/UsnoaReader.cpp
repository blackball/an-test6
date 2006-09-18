/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <BlockMemBank.hpp>
#include <MachinePort.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>
#include "UsnoaReader.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// size of each USNOA record
#define USNOA_REC_SIZE    (sizeof(DWORD)*3)

/* -------------------------------------------------- */
/* -------------------------------------------------- */

double InvRA;           // value to convert RA in the to the actual RA value
double InvDEC;          // value to convert DEC in the to the actual DEC value

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 *  UsnoaReaderV2: Initialize the reader to a base state.
 */
UsnoaReaderV2::UsnoaReaderV2()
{
  CatName = NULL;
  CatFP   = NULL;
  AccelName = NULL;
  CatPos = 0;

  AccelCount = 0;
  AccelList  = NULL;
  AccelStep  = 0;

  StarCount = 0;

  if (getEndian() == L_ENDIAN)
  {
    readRec = ReadLittleERec;
  } else
  {
    readRec = ReadBigERec;
  }

  InvRA  = (1.0/(15*3600*100))/24*360;
  InvDEC = 1.0/(3600*100);
}

/* -------------------------------------------------- */

/*
 * ~UsnoaReaderV2: shutdown.
 */
UsnoaReaderV2::~UsnoaReaderV2()
{
  Deinit();
}

/* -------------------------------------------------- */

/*
 * Open: Opens the file 'catname' and the associated accelerator file 'accelname'. REtruns
 *       TRUE32 if the process succeeds, FALSE32 otherwise.
 * 
 */
BOOL32 UsnoaReaderV2::Open(char *catname, char *accelname)
{
  FILE *accelPtr;
  BlockMemBank *bank;
  BYTE tempSpace[sizeof(float)+2*sizeof(DWORD)], *tptr;
  DWORD i;
  Enumeration *tenum;

  Deinit();

  // copy catalog and accelerator file names
  CatName = new char[strlen(catname)+1];
  strcpy(CatName, catname);
  AccelName = new char[strlen(accelname)+1];
  strcpy(AccelName, accelname);

  // first grab accelerator file info
  bank = new BlockMemBank();
  bank->Init(sizeof(float)+2*sizeof(DWORD));
  accelPtr = fopen(accelname, "rb");
  if (accelPtr == NULL)
  {
    delete bank;
    return FALSE32;
  }
  // extract all acceleration info
  while (!feof(accelPtr))
  {
    if (fscanf(accelPtr, "%f %u %u", &tempSpace[0], 
               &tempSpace[sizeof(float)], &tempSpace[sizeof(float)+sizeof(DWORD)]) == 3)
    {
      bank->AddNewData(tempSpace);
    }
  }
  fclose(accelPtr);

  if (bank->GetCount() == 0)
  {
    delete bank;
    return FALSE32;
  }

  tenum = bank->NewEnum(FORWARD);
  AccelCount = bank->GetCount();
  AccelList  = new AccelInfo[AccelCount];
  i = 0;
  while (tenum->GetEnumOK() == TRUE32)
  {
    tptr = (BYTE*)tenum->GetEnumNext();
    AccelList[i].BaseRA     = (float) (*((float*)tptr)/24.0*360.0 - 180.0);
    AccelList[i].StarPos    = *((DWORD*)(tptr+sizeof(float)));
    AccelList[i].StarCount  = *((DWORD*)(tptr+sizeof(float)+sizeof(DWORD)));
    i++;
  }
  delete tenum;
  delete bank;

  // grab the star count
  StarCount = AccelList[AccelCount-1].StarPos+AccelList[AccelCount-1].StarCount;
  // update the accelerator positions to be byte offsets rather than record offsets
  for (i = 0; i < AccelCount; i++)
  {
    AccelList[i].StarPos--;
    AccelList[i].StartByte = AccelList[i].StarPos*3*sizeof(DWORD);
  }
  // determine the steppingrate of the accel list
  if (AccelCount > 0)
  {
    AccelStep = AccelList[1].BaseRA - AccelList[0].BaseRA;
  }

  // now open the actual catalog
  CatFP = fopen(CatName, "rb");
  if (CatFP == NULL)
  {
    return FALSE32;
  }
  CatPos = 0;

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * GetStar: Attempts to read star number 'recNum' and place the contents
 *          into record 'rec'. Returns TRUE32 on success, FALSE32 on failure.
 * Assumes: 'rec' is a valid StarRecord pointer.
 */
BOOL32 UsnoaReaderV2::GetStar(DWORD recNum, StarRecord *rec)
{
  BYTE RecSpace[USNOA_REC_SIZE];
  char DigSpace[20];
  signed long last;

  if (recNum >= StarCount)
  {
    return FALSE32;
  }

  // seek to data as needed
  if (recNum != CatPos)
  {
    if (fseek(CatFP, recNum*USNOA_REC_SIZE, SEEK_SET))
    {
      return FALSE32;
    }
  }
  CatPos = recNum+1;

  // read data raw
  if (fread(RecSpace, USNOA_REC_SIZE, 1, CatFP) < 1)
  {
    return FALSE32;
  }

  // adjust data
  (this->*readRec)(RecSpace);

  // interpret record
  rec->StarNum = recNum;
  rec->RA  = ((double)*((DWORD*)&RecSpace[0]))*InvRA - 180.0;
  rec->DEC = ((double)*((DWORD*)&RecSpace[sizeof(DWORD)]))*InvDEC - 90.0;
  last = *((signed long*)&RecSpace[sizeof(DWORD)*2]);
  sprintf(DigSpace, "%+.10d", last);

  if (DigSpace[0] == '-')
  {
    rec->Correlated = TRUE32;
  } else
  {
    rec->Correlated = FALSE32;
  }
  if (DigSpace[1] == '1')
  {
    rec->MagIffy = TRUE32;
  } else
  {
    rec->MagIffy = FALSE32;
  }

  rec->Field = 100*(DigSpace[2]-'0') + 10*(DigSpace[3]-'0') + (DigSpace[4]-'0');
  rec->BMag  = (100*(DigSpace[5]-'0') + 10*(DigSpace[6]-'0') + (DigSpace[7]-'0'))*0.1;
  rec->RMag  = (100*(DigSpace[8]-'0') + 10*(DigSpace[9]-'0') + (DigSpace[10]-'0'))*0.1;

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * GetNextStar: Fills 'rec' with the next available star in the catalog.
 *              Returns TRUE32 on success, FALSE32 on failure.
 * Assumes: 'rec' is a valid StarRecord pointer.
 */
BOOL32 UsnoaReaderV2::GetNextStar(StarRecord *rec)
{
  return GetStar(CatPos, rec);
}

/* -------------------------------------------------- */

/*
 * GetCurPos: Returns the current record position in the catalog file.
 * Assumes: <none>
 */
DWORD UsnoaReaderV2::GetCurPos()
{
  return CatPos;
}

/* -------------------------------------------------- */

/*
 * GetCatName: Returns a pointer to the name of the catalog file
 *             this reader is reading from. Returns NULL if no file is open.
 * Assumes: <none>
 */
char* UsnoaReaderV2::GetCatName()
{
  return CatName;
}

/* -------------------------------------------------- */

/*
 * GetAccelName: Returns a pointer to the name of the accelerator file
 *               this reader is reading from. Returns NULL if no accelerator file is open.
 * Assumes: <none>
 */
char* UsnoaReaderV2::GetAccelName()
{
  return AccelName;
}

/* -------------------------------------------------- */

/*
 * GetStarCount: Returns the number of records in the currently open catalog file.
 * Assumes: <none>
 */
DWORD UsnoaReaderV2::GetStarCount()
{
  return StarCount;
}

/* -------------------------------------------------- */

/*
 * ReadBigERec: Adjusts data from a file read given that the running
 *              CPU is big-endian.
 * Assumes: <none>
 */
void UsnoaReaderV2::ReadBigERec(BYTE *space)
{
}

/* -------------------------------------------------- */

/*
 * ReadLittleERec: Adjusts data from a file read given that the running
 *                CPU is little-endian.
 * Assumes: 'space' refers to space containing a complete catalog record.
 */
void UsnoaReaderV2::ReadLittleERec(BYTE *space)
{
  // data is in big endian format, so byte swap as needed
  swapEndian((DWORD*)(&space[0]));
  swapEndian((DWORD*)(&space[sizeof(DWORD)]));
  swapEndian((DWORD*)(&space[sizeof(DWORD)*2]));
}

/* -------------------------------------------------- */

/*
 * Deinit: Shuts down the allocated data from reading from the current catalog file.
 * Assumes: <none>
 */
void UsnoaReaderV2::Deinit()
{
  if (CatFP != NULL)
  {
    fclose(CatFP);
    CatFP = NULL;
  }
  if (CatName != NULL)
  {
    delete [] CatName;
    CatName = NULL;
  }
  if (AccelName != NULL)
  {
    delete [] AccelName;
    AccelName = NULL;
  }

  AccelCount = 0;
  if (AccelList != NULL)
  {
    delete [] AccelList;
    AccelList = NULL;
  }
  AccelStep = 0;
}

/* -------------------------------------------------- */

/*
 * FirstBlockNum: Returns the order of the first star in the file > than the provided 'ra'.
 * Assumes: <none>
 */
DWORD UsnoaReaderV2::FirstBlockNum(double ra)
{
  DWORD i;

  for (i = 0; i < AccelCount; i++)
  {
    if ((i+1) < AccelCount)
    {
      if (AccelList[i+1].BaseRA > ra)
      {
        return i;
      }
    } else
    {
      return i;
    }
  }

  return 0;
}

/* -------------------------------------------------- */

/*
 * AccelBlockStars: Return the number of stars in accelerator block 'num'.
 * Assumes: <none>
 */
DWORD UsnoaReaderV2::AccelBlockStars(DWORD num)
{
  if (num >= AccelCount)
  {
    return 0;
  }

  return AccelList[num].StarCount;
}

/* -------------------------------------------------- */

/*
 * GetAccelCount: Returns the number of accelerators.
 * Assumes: <none>
 */
DWORD UsnoaReaderV2::GetAccelCount()
{
  return AccelCount;
}

/* -------------------------------------------------- */

/*
 * SeekBlock: Jump the reader to start of accelerator block 'num'. Returns TRUE32 
 *            on success, FALSE32 on failure.
 * Assumes: <none>
 */
BOOL32 UsnoaReaderV2::SeekBlock(DWORD num)
{
  if (num >= AccelCount)
  {
    return FALSE32;
  }

  if (fseek(CatFP, AccelList[num].StartByte, SEEK_SET) == 0)
  {
    CatPos = AccelList[num].StarPos;
    return TRUE32;
  }

  return FALSE32;
}

/* -------------------------------------------------- */

/*
 * GetAccelBaseRA: Returns the starting RA of accelerator block 'num'.
 * Assumes: <none>
 */
double UsnoaReaderV2::GetAccelBaseRA(DWORD num)
{
  if (num >= AccelCount)
  {
    return -1;
  }

  return AccelList[num].BaseRA;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * UsnoaManagerV2: Initializes the class to a base state.
 */
UsnoaManagerV2::UsnoaManagerV2()
{
  FCount  = 0;
  FNames  = NULL;
  Readers = NULL;

  TotStars = 0;
  CurStar  = 0;
  DEC_Starts = NULL;

  CurOffset = 0;
  CurReader = 0;
}

/* -------------------------------------------------- */

/*
 * ~UsnoaManagerV2: Final shutdown.
 */
UsnoaManagerV2::~UsnoaManagerV2()
{
  Deinit();
}

/* -------------------------------------------------- */

/*
 * Deinit: Shuts down all the allocated space in the class.
 * Assumes: <none>
 */
void UsnoaManagerV2::Deinit()
{
  DWORD i;

  if (FCount > 0)
  {
    for (i = 0; i < FCount; i++)
    {
      delete [] FNames[i];
    }
    delete [] FNames;
    delete [] Readers;
    delete [] DEC_Starts;
    Readers = NULL;
    FNames = NULL;
    FCount = 0;
    TotStars = 0;
    CurStar  = 0;
  }
}

/* -------------------------------------------------- */

/*
 * Init: Initializes the class by opening and preparing to read
 *       from all the USNOA catalog files denoted in 'fnames'. 'fnames' should contain
 *       a list of strings containing the path and main name of each file to be opened. The
 *       extensions for the repsective catalog and accelerator files should *not* be added as they are
 *       assumed to be '.cat' and '.acc' respectively.
 * Ex: F:\USNOA\cat_file_1
 *     F:\USNOA\cat_file_2
 *       'fcount' denotes the number of names in 'fnames'. The function returns TRUE32 is all files
 *       were properly found and initialized, FALSE32 otherwise.
 * Assumes: 'fnames' contains 'fcount' valid file names'.
 */
BOOL32 UsnoaManagerV2::Init(char **fnames, DWORD fcount)
{
  DWORD i, slen;
  char catName[1024], accName[1024];
  BOOL32 ret;

  Deinit();

  FCount = fcount;
  FNames = new char*[fcount];

  ret = TRUE32;
  DEC_Starts = new double[FCount];
  Readers   = new UsnoaReaderV2[FCount];
  // open all catalog files
  for (i = 0; i < FCount; i++)
  {
    slen = strlen(fnames[i]);
    FNames[i] = new char[slen+1];
    strcpy(FNames[i], fnames[i]);
    
    // build catalog and accelerator names
    strcpy(catName, FNames[i]);
    strcat(catName, ".cat");
    strcpy(accName, FNames[i]);
    strcat(accName, ".acc");

    if (Readers[i].Open(catName, accName) == FALSE32)
    {
      ret = FALSE32;
    } else
    {
      // count up all stars
      TotStars += Readers[i].GetStarCount();
      catName[slen] = '\0';
      // save DEC beginning value for each catalog file
      DEC_Starts[i] = (double)atoi(catName+slen-4)/10.0 - 90;
    }
  }

  return ret;
}

/* -------------------------------------------------- */

/*
 * GetStarCount: Returns the total number of stars accross all catalog files.
 * Assumes: <none>
 */
double UsnoaManagerV2::GetStarCount()
{
  return TotStars;
}

/* -------------------------------------------------- */

/*
 * GetCurPos: Returns the current star record number next available from reading in
 *            the total catalog.
 * Assumes: <none>
 */
double UsnoaManagerV2::GetCurPos()
{
  return CurStar;
}

/* -------------------------------------------------- */

/*
 * GetStar: Returns a particular star based on it's record number from the complete catalog into
 *          the parameter 'rec'. Returns FALSE32 on failure, TRUE32 on success.
 * Assumes: 'rec' is a valid StarRecord pointer.
 */
BOOL32 UsnoaManagerV2::GetStar(double recNum, StarRecord *rec)
{
  double tsum;

  if (recNum >= TotStars)
  {
    return FALSE32;
  }

  CurReader = 0;
  CurOffset = 0;
  tsum = 0;
  while ((tsum+(double)Readers[CurReader].GetStarCount()) < recNum)
  {
    tsum += (double)Readers[CurReader].GetStarCount();
    CurReader++;
  }
  CurOffset = (DWORD)(recNum - tsum);
  return Readers[CurReader].GetStar(CurOffset, rec);
}

/* -------------------------------------------------- */

/*
 * GetNextStar: Returns the next available for reading from the complete catalog into the parameter
 *              'rec'. Returns TRUE32 on success, FALSE32 on failure.
 * Assumes: 'rec' is a valid StarRecord pointer.
 */
BOOL32 UsnoaManagerV2::GetNextStar(StarRecord *rec)
{
  BOOL32 ret;

  if (CurStar >= TotStars)
  {
    return FALSE32;
  }

  ret = Readers[CurReader].GetNextStar(rec);
  CurStar++;
  CurOffset++;
  if (Readers[CurReader].GetStarCount() <= CurOffset)
  {
    CurOffset = 0;
    CurReader++;
  }

  return ret;
}

/* -------------------------------------------------- */

/*
 * GetRegion: Returns all the stars within the region [minRa,maxRa]x[minDec,maxDec] into the pointer
 *            'dlist', and indicates the number of records via 'dcount'. Returns TRUE32 on success, FALSE32
 *            on failure. Space is allocated for the records and should be deallocated by the user.
 * Assumes: 'dlist' is a valid StarRecord pointer-pointer. 'dcount' is a valid DWORD pointer.
 */
BOOL32 UsnoaManagerV2::GetRegion(StarRecord **dlist, DWORD *dcount, double minRa, double maxRa, double minDec, double maxDec)
{
  DWORD i, j, k, goal;
  DWORD dStart, dEnd;
  DWORD rStart, rEnd;
  BlockMemBank starBank;
  StarRecord trec;
  Enumeration *tenum;
  BYTE *bptr;

  starBank.Init(sizeof(StarRecord), 1000);

  // determine dec start block
  for (i = 0; i < FCount; i++)
  {
    if ((i+1) < FCount)
    {
      if (DEC_Starts[i+1] > minDec)
      {
        dStart = i;
        break;
      }
    } else
    {
      dStart = i;
      break;
    }
  }

  // determine dec end block
  for (; i < FCount; i++)
  {
    if ((i+1) < FCount)
    {
      if (DEC_Starts[i+1] > maxDec)
      {
        dEnd = i;
        break;
      }
    } else
    {
      dEnd = i;
      break;
    }
  }

  // read all the blocks
  for (i = dStart; i <= dEnd; i++)
  {
    rStart = Readers[i].FirstBlockNum(minRa);
    rEnd   = Readers[i].FirstBlockNum(maxRa);

    if (Readers[i].SeekBlock(rStart) == FALSE32)
    {
      return FALSE32;
    }
    // read all stars within the block
    for (j = rStart; j <= rEnd; j++)
    {
      goal = Readers[i].AccelBlockStars(j);
      for (k = 0; k < goal; k++)
      {
        if (Readers[i].GetNextStar(&trec) == FALSE32)
        {
          return FALSE32;
        }
        if (((trec.RA >= minRa) && (trec.RA <= maxRa)) &&
            ((trec.DEC >= minDec) && (trec.DEC <= maxDec)))
        {
          starBank.AddNewData((BYTE*)&trec);
        }
      }
    }
  }

  // allocate space for the stars and set the values in the resulting list
  *dcount = starBank.GetCount();
  goal    = *dcount;
  *dlist  = new StarRecord[goal];
  tenum   = starBank.NewEnum(FORWARD);
  i = 0;
  while (tenum->GetEnumOK() == TRUE32)
  {
    bptr = (BYTE*)(tenum->GetEnumNext());
    memcpy(&(*dlist)[i], bptr, sizeof(StarRecord));
    i++;
  }

  return TRUE32;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
