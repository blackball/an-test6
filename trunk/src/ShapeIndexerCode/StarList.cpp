/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <stdio.h>
#include <MachinePort.h>
#include "StarList.hpp"
#include <math.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * StarList: Inialize the catalog to a base state.
 * Assumes: <none>
 */
StarList::StarList()
{
  List[0] = NULL;
  List[1] = NULL;
  List[2] = NULL;

  List3D[0] = NULL;
  List3D[1] = NULL;
  List3D[2] = NULL;

  Bounds[0][0] = Bounds[0][1] = Bounds[1][0] = Bounds[1][1] = 0;
  StarCount = 0;
}

/* -------------------------------------------------- */

/*
 * ~StarList: Shutdown.
 * Assumes: <none>
 */
StarList::~StarList()
{
  Deinit();
}

/* -------------------------------------------------- */

/*
 * Init: Initializes the catalog with space for 'count' stars.
 * Assumes: 'count' > 0.
 */
void StarList::Init(DWORD count)
{
  Deinit();

  StarCount = count;

  List[0] = new double[count];
  List[1] = new double[count];
  List[2] = new double[count];

  List3D[0] = new double[count];
  List3D[1] = new double[count];
  List3D[2] = new double[count];
}

/* -------------------------------------------------- */

/*
 * Deinit: Clean up and remove extaneous space.
 * Assumes: <none>
 */
void StarList::Deinit()
{
  if (List[0] != NULL)
  {
    delete [] List[0];
    List[0] = NULL;
    delete [] List[1];
    List[1] = NULL;
    delete [] List[2];
    List[2] = NULL;
  }

  if (List3D[0] != NULL)
  {
    delete [] List3D[0];
    List3D[0] = NULL;
    delete [] List3D[1];
    List3D[1] = NULL;
    delete [] List3D[2];
    List3D[2] = NULL;
  }

  Bounds[0][0] = Bounds[0][1] = 
    Bounds[1][0] = Bounds[1][1] = 0;

  StarCount = 0;
}

/* -------------------------------------------------- */

/*
 * Save: Saves the catalog in binary form into file 'fname'. The file is
 *       endian safe. Returns TRUE32 if the file was written successfully, FALSE32
 *       otherwise.
 * Assumes: 'fname' is a valid string pointer.
 */
BOOL32 StarList::Save(char *fname)
{
  FILE *out;
  WORD endianTest;

  // open file
  out = fopen(fname, "wb");
  if (out == NULL)
  {
    return FALSE32;
  }
  
  // write the endian of the file data so it can be recovered on any machine
  endianTest = ENDIAN_TEST_VAL;
  if (fwrite((BYTE*)&endianTest, sizeof(WORD), 1, out) < 1)
  {
    return FALSE32;
  }
  fflush(out);

  // save the star count
  if (fwrite((BYTE*)&StarCount, sizeof(DWORD), 1, out) < 1)
  {
    return FALSE32;
  }
  fflush(out);

  // save the bounds -- depricated
  if (fwrite((BYTE*)&Bounds[0][0], sizeof(double), 1, out) < 1)
  {
    return FALSE32;
  }
  if (fwrite((BYTE*)&Bounds[0][1], sizeof(double), 1, out) < 1)
  {
    return FALSE32;
  }
  if (fwrite((BYTE*)&Bounds[1][0], sizeof(double), 1, out) < 1)
  {
    return FALSE32;
  }
  if (fwrite((BYTE*)&Bounds[1][1], sizeof(double), 1, out) < 1)
  {
    return FALSE32;
  }
  fflush(out);

  // save the ra,dec,brightness info. The 3D info can be recovered directly from 
  // the angle info.
  if (StarCount > 0)
  {
    if (fwrite((BYTE*)List[0], sizeof(double)*StarCount, 1, out) < 1)
    {
      return FALSE32;
    }
    fflush(out);
    if (fwrite((BYTE*)List[1], sizeof(double)*StarCount, 1, out) < 1)
    {
      return FALSE32;
    }
    fflush(out);
    if (fwrite((BYTE*)List[2], sizeof(double)*StarCount, 1, out) < 1)
    {
      return FALSE32;
    }
    fflush(out);
  }

  fclose(out);

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * SaveAsText: Saves the catalog in text form into file 'fname'. Contains the
 *             star count follows by a list of '<ra> <dec> <brightness>\n'. Returns 
 *             TRUE32 if the file was written successfully, FALSE32 otherwise.
 * Assumes: 'fname' is a valid string pointer.
 */
BOOL32 StarList::SaveAsText(char *fname)
{
  FILE *out;
  DWORD i;

  // open file
  out = fopen(fname, "wt");
  if (out == NULL)
  {
    return FALSE32;
  }

  // write star count
  if (fprintf(out, "%u\n", StarCount) < 1)
  {
    return FALSE32;
  }

  // write ra, dec, brightness info
  for (i = 0; i < StarCount; i++)
  {
    if (fprintf(out, "%f %f %f\n", List[0][i], List[1][i], List[2][i]) < 3)
    {
      return FALSE32;
    }
  }

  fclose(out);
  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * Open: Opens a catalog in binary form from a file 'fname'. Returns 
 *       TRUE32 if the file was read successfully, FALSE32 otherwise.
 * Assumes: 'fname' is a valid string pointer.
 */
BOOL32 StarList::Open(char *fname)
{
  FILE *in;
  WORD endianTest;
  BOOL32 iSwap;
  double idist;
  DWORD i;

  // clean up any existing space
  Deinit();

  // file open
  in = fopen(fname, "rb");
  if (in == NULL)
  {
    printf("Unable to open file\n");
    return FALSE32;
  }

  // test endian of data
  if (fread((BYTE*)&endianTest, sizeof(WORD), 1, in) < 1)
  {
    return FALSE32;
  }
  if (endianTest == ENDIAN_TEST_VAL)
  {
    iSwap = FALSE32;
  } else
  {
    iSwap = TRUE32;
  }

  // grab star count
  if (fread((BYTE*)&StarCount, sizeof(DWORD), 1, in) < 1)
  {
    return FALSE32;
  }
  // swap endian of star count as needed
  if (iSwap == TRUE32)
  {
    swapEndian(&StarCount);
  }

  // build ra, dec, brightness space
  List[0] = new double[StarCount];
  List[1] = new double[StarCount];
  List[2] = new double[StarCount];

  // read bounds -- depricated
  if (fread((BYTE*)&Bounds[0][0], sizeof(double), 1, in) < 1)
  {
    return FALSE32;
  }
  if (fread((BYTE*)&Bounds[0][1], sizeof(double), 1, in) < 1)
  {
    return FALSE32;
  }
  if (fread((BYTE*)&Bounds[1][0], sizeof(double), 1, in) < 1)
  {
    return FALSE32;
  }
  if (fread((BYTE*)&Bounds[1][1], sizeof(double), 1, in) < 1)
  {
    return FALSE32;
  }
  // read all the star info
  if (StarCount > 0)
  {
    if (fread((BYTE*)List[0], sizeof(double)*StarCount, 1, in) < 1)
    {
      printf("Unable to read RA positions\n");
      return FALSE32;
    }
    if (fread((BYTE*)List[1], sizeof(double)*StarCount, 1, in) < 1)
    {
      printf("Unable to read Dec positions\n");
      return FALSE32;
    }
    if (fread((BYTE*)List[2], sizeof(double)*StarCount, 1, in) < 1)
    {
      printf("Unable to read Mag positions\n");
      return FALSE32;
    }
  }
  fclose(in);

  // swap data endian as needed
  if (iSwap == TRUE32)
  {
    swapEndian(&Bounds[0][0]);
    swapEndian(&Bounds[0][1]);
    swapEndian(&Bounds[1][0]);
    swapEndian(&Bounds[1][1]);

    for (i = 0; i < StarCount; i++)
    {
      swapEndian(&List[0][i]);
      swapEndian(&List[1][i]);
      swapEndian(&List[2][i]);
    }
  }

  // bild the 3D point version of the data
  List3D[0] = new double[StarCount];
  List3D[1] = new double[StarCount];
  List3D[2] = new double[StarCount];
  for (i = 0; i < StarCount; i++)
  {
    List3D[0][i] = cos(List[0][i]*RAD_CONV)*sin(List[1][i]*RAD_CONV);
    List3D[1][i] = sin(List[0][i]*RAD_CONV)*sin(List[1][i]*RAD_CONV);
    List3D[2][i] = cos(List[1][i]*RAD_CONV);
    idist = 1.0 / sqrt(List3D[0][i]*List3D[0][i]+List3D[1][i]*List3D[1][i]+List3D[2][i]*List3D[2][i]);
    List3D[0][i] *= idist;
    List3D[1][i] *= idist;
    List3D[2][i] *= idist;
  }

  return TRUE32;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
