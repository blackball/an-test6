/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <fstream.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "StarShapeSys.hpp"
#include <MathExt.h>
#include <BaseTimer.hpp>
#include <BlockMemBank.hpp>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/* 
 * Builds a set of indices according to the parameters in the provided parameter filoe. and
 * the provided star catalog.
 * Usage: <build_param_file> <cat_file>
 * <build_param_file>: file containing index building parameters
 * <cat_file>         : catalog file for indexing
 * Ex: build.params cat.rand.10000.bin
 * 
 * Parameter file should contain:
 * <index_file_base>  : base name for all resulting index files
 * <im_angle>         : maximum image angle
 * <max_shapes>       : maximum shapes per star to record
 * <bincount>         : number of bins to use in index
 * <wedge_angle_cnt>  : number of different wedge angles to use
 * <wedge_angles>+    : the <wedge_angle_cnt> wedge angles
 * <stars_p_shape_cnt>: number of different stars per shape to use
 * <stars_p_shape>+   : the <stars_p_shape_cnt> stars per shape
 *
 * Ex: 
 * lines.tycho.
 * 0.5
 * 6
 * 100000
 * 8
 * 22.5 25.7143 30.0 36.0 45.0 60.0 90.0 180.0
 * 4
 * 9 7 6 4
 */

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * ParamStruct: Structure for holding the input parameters in the
 *              parameter file.
 */
struct ParamStruct
{
  char    baseFileName[2048];
  double  imAngle;
  DWORD   maxShapes;
  DWORD   binCount;
  DWORD   wedgeSizes;
  double *wedgeSizeList;
  DWORD   starCounts;
  DWORD  *starCountList;
};

/* -------------------------------------------------- */

/*
 * deleteParamStruct: Shutdown the passed parameter data.
 */
void deleteParamStruct(ParamStruct *params)
{
  if (params->starCountList != NULL)
    delete [] params->starCountList;
  if (params->wedgeSizeList != NULL)
    delete [] params->wedgeSizeList;
}

/* -------------------------------------------------- */

/*
 * ReadParams: Reads in the parameters in the file 'buildfile', and fills the
 *             passed parameter structure 'params'. Returns TRUE32 if there are no
 *             problems, FALSE32 otherwise.
 * Assumes: 'buildfile' is a valid file name, 'params' is a valid parameter structure.
 */
BOOL32 ReadParams(char *buildfile, ParamStruct *params)
{
  FILE *fp;
  DWORD i;
  BlockMemBank paramBank;

  fp = fopen(buildfile, "rt");
  if (fp == NULL)
    return FALSE32;

  params->starCountList = NULL;
  params->wedgeSizeList = NULL;

  paramBank.Init(sizeof(StarShapeBuildParams), 20);
  // base index name
  if (fscanf(fp, "%s",  params->baseFileName) < 1)
  {
    printf("Error parsing basename\n");
    return FALSE32;
  }
  // image angle
  if (fscanf(fp, "%lg",  &(params->imAngle)) < 1)
  {
    printf("Error parsing image angle\n");
    return FALSE32;
  }
  // max shapes per star
  if (fscanf(fp, "%u",  &(params->maxShapes)) < 1)
  {
    printf("Error parsing max shapes\n");
    return FALSE32;
  }
  // bin count for each index
  if (fscanf(fp, "%u",  &(params->binCount)) < 1)
  {
    printf("Error parsing bin count\n");
    return FALSE32;
  }
  // number of wedge sizes
  if (fscanf(fp, "%u",  &(params->wedgeSizes)) < 1)
  {
    printf("Error parsing wedge size count\n");
    return FALSE32;
  }
  params->wedgeSizeList = new double[params->wedgeSizes];
  // actual wedge sizes
  for (i = 0; i < params->wedgeSizes; i++)
  {
    if (fscanf(fp, "%lf",  &(params->wedgeSizeList[i])) < 1)
    {
      printf("Error parsing wedge size\n");
      return FALSE32;
    }
  }
  // number of counts for stars per shape
  if (fscanf(fp, "%u",  &(params->starCounts)) < 1)
  {
    printf("Error parsing stars per shape count\n");
    return FALSE32;
  }
  params->starCountList = new DWORD[params->starCounts];
  // actual stars per shape
  for (i = 0; i < params->starCounts; i++)
  {
    if (fscanf(fp, "%u",  &(params->starCountList[i])) < 1)
    {
      printf("Error parsing stars per shape\n");
      return FALSE32;
    }
  }

  return TRUE32;
}

/* -------------------------------------------------- */

/* 
 * Builds a set of indices according to the parameters in the provided parameter filoe. and
 * the provided star catalog.
 * Usage: <build_param_file> <cat_file>
 * <build_param_file>: file containing index building parameters
 * <cat_file>         : catalog file for indexing
 * Ex: build.params cat.rand.10000.bin
 * 
 * Parameter file should contain:
 * <index_file_base>  : base name for all resulting index files
 * <im_angle>         : maximum image angle
 * <max_shapes>       : maximum stars per shape
 * <bincount>         : number of bins to use in index
 * <wedge_angle_cnt>  : number of different wedge angles to use
 * <wedge_angles>+    : the <wedge_angle_cnt> wedge angles
 * <stars_p_shape_cnt>: number of different stars per shape to use
 * <stars_p_shape>+   : the <stars_p_shape_cnt> stars per shape
 * Ex: 
 * lines.tycho.
 * 0.5
 * 6
 * 100000
 * 8
 * 22.5 25.7143 30.0 36.0 45.0 60.0 90.0 180.0
 * 4
 * 9 7 6 4
 */
int main(int argc, char *argv[])
{
  StarList  cList;
  StarShapeSys ShapeSys;
  char *build_file;
  char *cat_file;
  BOOL32 iGood;
  ParamStruct params;
  DWORD j;

#ifdef STAR_SHAPE_KDTREE
  printf("Using KD-Tree\n");
#else
  printf("Not Using KD-Tree\n");
#endif

  // output usage
  if (argc < 2)
  {
    printf("\n");
    printf("Usage: <build_param_file> <cat_file>\n");
    printf("<build_param_file>: file containing index building parameters\n");
    printf("<cat_file>         : catalog file for indexing\n");
    printf("\nBuild Params\n");
    printf("Parameter file should contain:\n");
    printf("<index_file_base>  : base name for all resulting index files\n");
    printf("<im_angle>         : maximum image angle\n");
    printf("<max_shapes>       : maximum stars per shape\n");
    printf("<bincount>         : number of bins to use in index\n");
    printf("<wedge_angle_cnt>  : number of different wedge angles to use\n");
    printf("<wedge_angles>+    : the <wedge_angle_cnt> wedge angles\n");
    printf("<stars_p_shape_cnt>: number of different stars per shape to use\n");
    printf("<stars_p_shape>+   : the <stars_p_shape_cnt> stars per shape\n");
    printf("\n");
    return -1;
  } else
  {
    iGood = TRUE32;
    build_file = argv[1];
    cat_file   = argv[2];
    if (ReadParams(build_file, &params) == FALSE32)
    {
      printf("Error parsing command params\n");
      exit(1);
    }
  }
 
  // output build info
  printf("Build Params: %s\n", build_file);
  printf("Catalog File: %s\n", cat_file);
  printf("\n");
  printf("Output      : %s\n", params.baseFileName);
  printf("Max Radius  : %g\n", params.imAngle);
  printf("Max Shapes  : %u\n", params.maxShapes);
  printf("Bin Count   : %u\n", params.binCount);
  printf("Angle Wedges: %u\n", params.wedgeSizes);
  for (j = 0; j < params.wedgeSizes; j++)
  {
    printf("%lg ", params.wedgeSizeList[j]);
  }
  printf("\n");
  printf("Stars P. Shape: %u\n", params.starCounts);
  for (j = 0; j < params.starCounts; j++)
  {
    printf("%u ", params.starCountList[j]);
  }
  printf("\n");
  fflush(stdout);

  // open star list
  if (cList.Open(cat_file) == FALSE32)
  {
    printf("Error opening catalog file: %s\n", cat_file);
    return -1;
  }
  // set star lis
  ShapeSys.SetStarList(&cList);
  // init build params
  ShapeSys.InitBuildParams(params.baseFileName, params.imAngle, params.maxShapes, params.binCount, 
                           params.wedgeSizes, params.wedgeSizeList, params.starCounts, params.starCountList);
  printf("Catalog contains %u stars\n", cList.StarCount);

  // build indices
  if (ShapeSys.BuildIndex() == FALSE32)
  {
    printf("Error trying to build index\n");
    fflush(stdout);
  }

  // finished
  printf("Final Cleanup\n");
  fflush(stdout);

  cList.Deinit();

  return 0;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
