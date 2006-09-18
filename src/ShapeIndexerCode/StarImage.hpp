#ifndef __STARIMAGE_HPP__
#define __STARIMAGE_HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>
#include "StarList.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * StarImage: Class representing the x/y positions of 
 *            stars in an image. Note: Files that are written and read
 *            are endian safe.
 */
class StarImage
{
public:
  // the abstract image info--the camera basis and image scaling
  double  VBasisI[3];
  double  VBasisJ[3];
  double  VBasisK[3];
  double  VScale;

  double *Stars[3];   // The star positions, in [-1,1]
  DWORD  *TrueIndex;  // The true id's of the stars relative to the generating catalog
  DWORD   StarCount;  // Number of stars in the image
  DWORD   KeyCount;   // depricated
  DWORD   RealCount;  // Number of cataloged stars in the image
  DWORD   FakeCount;  // Number of uncataloged stars in the image

public:
  StarImage();
  ~StarImage();

  // Initialize the space for the image
  void Init(DWORD starCount);
  // Cleanup
  void Deinit();

  // Save a binary version of the image.
  BOOL32 Save(char *fname);
  // Save a text version of the image.
  BOOL32 SaveAsText(char *fname);
  // Save a text version of the image in another format.
  BOOL32 SaveAsText2(char *fname);
  // Read in an image.
  BOOL32 Open(char *fname);
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
