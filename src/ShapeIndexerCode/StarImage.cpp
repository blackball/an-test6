/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <stdio.h>
#include <MachinePort.h>
#include "StarImage.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * StarImage: Initializes the image to a base state.
 * Assumes: <none>
 */
StarImage::StarImage()
{
  Stars[0] = Stars[1] = Stars[2] = 0;
  TrueIndex = 0;
  StarCount = 0;
  KeyCount = 0;
  VBasisI[0] = VBasisI[1] = VBasisI[2] = 0;
  VBasisJ[0] = VBasisJ[1] = VBasisJ[2] = 0;
  VBasisK[0] = VBasisK[1] = VBasisK[2] = 0;
  VScale = 0;
}

/* -------------------------------------------------- */

/*
 * ~StarImage: Shuts down the image.
 * Assumes: <none>
 */
StarImage::~StarImage()
{
  Deinit();
}

/* -------------------------------------------------- */

/*
 * Init: Initializes the star image's data structures to accept
 *       'starCount' stars.
 * Assumes: starCount > 0.
 */
void StarImage::Init(DWORD starCount)
{
  Deinit();

  StarCount = starCount;
  Stars[0]  = new double[StarCount];
  Stars[1]  = new double[StarCount];
  Stars[2]  = new double[StarCount];
  TrueIndex = new DWORD[StarCount];
}

/* -------------------------------------------------- */

/*
 * Deinit: Shut down the memory used by the image.
 * Assumes: <none>
 */
void StarImage::Deinit()
{
  if (Stars[0] != NULL)
  {
    delete [] Stars[0];
    delete [] Stars[1];
    delete [] Stars[2];
    Stars[0] = Stars[1] = Stars[2] = NULL;
  }
  if (TrueIndex != NULL)
  {
    delete [] TrueIndex;
    TrueIndex = NULL;
  }
  StarCount = 0;
  KeyCount  = 0;
  VBasisI[0] = VBasisI[1] = VBasisI[2] = 0;
  VBasisJ[0] = VBasisJ[1] = VBasisJ[2] = 0;
  VBasisK[0] = VBasisK[1] = VBasisK[2] = 0;
  VScale = 0;
}

/* -------------------------------------------------- */

/*
 * Save: Saves the image in binary format. Contains the header info
 *       for the image, plus the id's of the stars. Returns TRUE32 of
 *       save success, FALSE32 otherwise.
 * Assumes: 'fname' is a valid string.
 */
BOOL32 StarImage::Save(char *fname)
{
  FILE *out;
  WORD endianTest;

  // open file
  out = fopen(fname, "wb");
  if (out == NULL)
  {
    return FALSE32;
  }
  
  // save endian info
  endianTest = ENDIAN_TEST_VAL;
  if (fwrite((BYTE*)&endianTest, sizeof(WORD), 1, out) < 1)
  {
    return FALSE32;
  }
  fflush(out);

  // save camera basis
  if (fwrite((BYTE*)VBasisI, sizeof(double)*3, 1, out) < 1)
  {
    return FALSE32;
  }
  if (fwrite((BYTE*)VBasisJ, sizeof(double)*3, 1, out) < 1)
  {
    return FALSE32;
  }
  if (fwrite((BYTE*)VBasisK, sizeof(double)*3, 1, out) < 1)
  {
    return FALSE32;
  }
  // save image scale
  if (fwrite((BYTE*)&VScale, sizeof(double), 1, out) < 1)
  {
    return FALSE32;
  }
  // save star count info
  if (fwrite((BYTE*)&StarCount, sizeof(DWORD), 1, out) < 1)
  {
    return FALSE32;
  }
  if (fwrite((BYTE*)&KeyCount, sizeof(DWORD), 1, out) < 1)
  {
    return FALSE32;
  }
  // save actual star positions and brightness
  if (fwrite((BYTE*)Stars[0], sizeof(double)*StarCount, 1, out) < 1)
  {
    return FALSE32;
  }
  if (fwrite((BYTE*)Stars[1], sizeof(double)*StarCount, 1, out) < 1)
  {
    return FALSE32;
  }
  if (fwrite((BYTE*)Stars[2], sizeof(double)*StarCount, 1, out) < 1)
  {
    return FALSE32;
  }
  // save catalog id info
  if (fwrite((BYTE*)TrueIndex, sizeof(DWORD)*StarCount, 1, out) < 1)
  {
    return FALSE32;
  }

  fclose(out);
  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * SaveAsText: Saves the image in text format. Does not contain header info,
 *             simply a list of '<id> <x> <y>', where <x> and <y> are in
 *             [-1, 1] x [-1, 1]. Returns TRUE32 of save success, FALSE32 otherwise.
 * Assumes: 'fname' is valid string pointer.
 */
BOOL32 StarImage::SaveAsText(char *fname)
{
  FILE *out;
  DWORD i;

  out = fopen(fname, "wt");
  if (out == NULL)
  {
    return FALSE32;
  }

  for (i = 0; i < StarCount; i++)
  {
    // write id
    if (TrueIndex[i] == DIST_STAR_INDEX)
    {
      if (fprintf(out, "-1 ") < 1)
      {
        return FALSE32;
      }
    } else
    {
      if (fprintf(out, "%.9u ", TrueIndex[i]+1) < 1)
      {
        return FALSE32;
      }
    }

    // write star x, y
    if (fprintf(out, "%.12f %.12f\n", Stars[0][i]*2, Stars[1][i]*2) < 2)
    {
      return FALSE32;
    }
  }

  fclose(out);
  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * SaveAsText: Saves the image in text format. Does not contain header info,
 *             simply a list of '<id> <x> <y> <brightness>\n'. Returns TRUE32 of
 *             save success, FALSE32 otherwise.
 * Assumes: 'fname' is valid string pointer.
 */
BOOL32 StarImage::SaveAsText2(char *fname)
{
  FILE *out;
  DWORD i;

  out = fopen(fname, "wt");
  if (out == NULL)
  {
    return FALSE32;
  }

  for (i = 0; i < StarCount; i++)
  {
    // write catalog id
    if (TrueIndex[i] == DIST_STAR_INDEX)
    {
      if (fprintf(out, "-1 ") < 1)
      {
        return FALSE32;
      }
    } else
    {
      if (fprintf(out, "%.9u ", TrueIndex[i]+1) < 1)
      {
        return FALSE32;
      }
    }

    // write star x, y, brightness
    if (fprintf(out, "%.12f %.12f %.12f \n", Stars[0][i]*2, Stars[1][i]*2, Stars[2][i]) < 3)
    {
      return FALSE32;
    }
  }

  fclose(out);
  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * Open: Opens a binary file containing image information.
 * Assumes: 'fname' is valid string pointer. Returns TRUE32 of
 *          open success, FALSE32 otherwise.
 */
BOOL32 StarImage::Open(char *fname)
{
  FILE *in;
  WORD endianTest;
  BOOL32 iSwap;
  DWORD i;

  Deinit();

  // open file
  in = fopen(fname, "rb");
  if (in == NULL)
  {
    return FALSE32;
  }
  
  // get image endian info
  endianTest = ENDIAN_TEST_VAL;
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

  // grab camera basis
  if (fread((BYTE*)VBasisI, sizeof(double)*3, 1, in) < 1)
  {
    return FALSE32;
  }
  if (fread((BYTE*)VBasisJ, sizeof(double)*3, 1, in) < 1)
  {
    return FALSE32;
  }
  if (fread((BYTE*)VBasisK, sizeof(double)*3, 1, in) < 1)
  {
    return FALSE32;
  }

  // grab remaining header info
  if (fread((BYTE*)&VScale, sizeof(double), 1, in) < 1)
  {
    return FALSE32;
  }
  if (fread((BYTE*)&StarCount, sizeof(DWORD), 1, in) < 1)
  {
    return FALSE32;
  }
  if (fread((BYTE*)&KeyCount, sizeof(DWORD), 1, in) < 1)
  {
    return FALSE32;
  }
  // swap byte order as needed
  if (iSwap == TRUE32)
  {
    swapEndian(&VBasisI[0]);
    swapEndian(&VBasisI[1]);
    swapEndian(&VBasisI[2]);
    swapEndian(&VBasisJ[0]);
    swapEndian(&VBasisJ[1]);
    swapEndian(&VBasisJ[2]);
    swapEndian(&VBasisK[0]);
    swapEndian(&VBasisK[1]);
    swapEndian(&VBasisK[2]);
    swapEndian(&VScale);
    swapEndian(&StarCount);
    swapEndian(&KeyCount);
  }

  // init star space
  Stars[0] = new double[StarCount];
  Stars[1] = new double[StarCount];
  Stars[2] = new double[StarCount];
  TrueIndex = new DWORD[StarCount];

  // read in star info
  if (fread((BYTE*)Stars[0], sizeof(double)*StarCount, 1, in) < 1)
  {
    return FALSE32;
  }
  if (fread((BYTE*)Stars[1], sizeof(double)*StarCount, 1, in) < 1)
  {
    return FALSE32;
  }
  if (fread((BYTE*)Stars[2], sizeof(double)*StarCount, 1, in) < 1)
  {
    return FALSE32;
  }
  // read in index values
  if (fread((BYTE*)TrueIndex, sizeof(DWORD)*StarCount, 1, in) < 1)
  {
    return FALSE32;
  }
  fclose(in);

  // update star info byte order as needed
  if (iSwap == TRUE32)
  {
    for (i = 0; i < StarCount; i++)
    {
      swapEndian(&Stars[0][i]);
      swapEndian(&Stars[1][i]);
      swapEndian(&Stars[2][i]);
      swapEndian(&TrueIndex[i]);
    }
  }

  return TRUE32;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
