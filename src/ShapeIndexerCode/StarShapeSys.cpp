/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <MathExt.h>
#include <BlockMemBank.hpp>
#include <MachinePort.h>
#include <memory.h>
#include <time.h>
#include <BaseTimer.hpp>
#include <HashTable.hpp>
#include "StarKDTree.hpp"
#include "StarShapeSys.hpp"
#include "StarCamera.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// accuracy used for esimating index size
#define POISS_ACC         0.00000001

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * StarShapeSys: Initizialize index to base state.
 */
StarShapeSys::StarShapeSys()
{
  iInit  = FALSE32;
  iParam = FALSE32;
  iSInit = FALSE32;

  sList = NULL;

  iShapeCount   = 0;

  iStarPoint  = NULL;
  iNearPoint  = NULL;
  iThirdPoint = NULL;
  iCRatioList = NULL;
  iAngleList  = NULL;
  iBinStart   = NULL;
  iBinLength  = NULL;
  iBinData    = NULL;

  BuildParams = NULL;
}

/* -------------------------------------------------- */

/*
 * ~StarShapeSys: Shutdown.
 */
StarShapeSys::~StarShapeSys()
{
  Deinit();

  if (BuildParams != NULL)
    delete [] BuildParams;
  BuildParams = NULL;
}

/* -------------------------------------------------- */

/*
 * Deinit: Cleanup to class and delete all used space.
 * Assumes: <none>
 */
void StarShapeSys::Deinit()
{
  iInit  = FALSE32;

  iShapeCount   = 0;

  if (iStarPoint != NULL)
    delete [] iStarPoint;
  if (iNearPoint != NULL)
    delete [] iNearPoint;
  if (iThirdPoint != NULL)
    delete [] iThirdPoint;
  if (iCRatioList != NULL)
    delete [] iCRatioList;
  if (iAngleList != NULL)
    delete [] iAngleList;
  if (iBinStart != NULL)
    delete [] iBinStart;
  if (iBinLength != NULL)
    delete [] iBinLength;
  if (iBinData != NULL)
    delete [] iBinData;

  iStarPoint  = NULL;
  iNearPoint  = NULL;
  iThirdPoint = NULL;
  iCRatioList = NULL;
  iAngleList  = NULL;
  iBinStart   = NULL;
  iBinLength  = NULL;
  iBinData    = NULL;
}

/* -------------------------------------------------- */

/*
 * EstimateStarsPerIm: Returns the estimated number of stars per image 
 *                     acording to the provided build parameters.
 * Assumes: 'params' refers to a valid set of build parameters, a 'catalog' has already
 *          been set.
 */
double StarShapeSys::EstimateStarsPerIm(StarShapeBuildParams *params)
{
  double wedge;
  double sregion, exStars;
  double N, mapArea;

  if ((iSInit == FALSE32) || (iParam == FALSE32))
    return 0;  
  
  // get the area covered by the indexing wedge
    // first calc surface area of the sphere marked by the image radius
  sregion = 2*PI*(-cos(PI) + cos(PI-MAX_IM_ANGLE*RAD_CONV*0.5));
    // then calc percentage of this region marked out by the wedge
  wedge = params->AngleWedge*RAD_CONV / (2.0*PI);
    // so wedge area equals proportion of the surface region
  wedge = wedge*sregion;

  // number of stars in catalog
  N = sList->StarCount;
  // area of catalog
  mapArea = 4.0*PI;

  // determine expected number of stars in examined region
  exStars = sregion / mapArea * (N-1);

  return exStars;
}

/* -------------------------------------------------- */

/*
 * EstimateShapes: Returns the estimated number of shapes produced
 *                 according to the provided build parameters.
 * Assumes: 'params' refers to a valid set of build parameters, a 'catalog' has already
 *          been set.
 */
double StarShapeSys::EstimateShapes(StarShapeBuildParams *params, double *vlambda)
{
  double wedge;
  double N, mapArea, lambda;
  double sregion, exStars, wcheck;
  double starP, pline;

  if ((iSInit == FALSE32) || (iParam == FALSE32))
    return 0;  
  
  // get the area covered by the indexing wedge
    // first calc surface area of the sphere marked by the image radius
  sregion = 2*PI*(-cos(PI) + cos(PI-MAX_IM_ANGLE*RAD_CONV*0.5));
    // then calc percentage of this region marked out by the wedge
  wedge = params->AngleWedge*RAD_CONV / (2.0*PI);
    // so wedge area equals proportion of the surface region
  wedge = wedge*sregion;

  // number of stars in catalog
  N = sList->StarCount;
  // area of catalog
  mapArea = 4.0*PI;
  // calc area of space examined for line
  starP  = wedge/mapArea;
  // calc lambda param used to determine distribution
  // of stars the will occur in the wedge
  lambda = N*starP;
  if (vlambda != NULL)
    *vlambda = lambda;

  // determine expected number of stars in examined region
  exStars = sregion / mapArea * (N-1);
  // determine expected number of directions tested for lines in total
  wcheck  = N*exStars;

  // find probability of finding >= MIN_LINE_POINTS in a region,
  // given that there are already two stars in the wedge
  pline = 1 - sumPoiss(params->MinShapePoints-2, lambda);

  // return expected number of lines found
  return pline * wcheck;
}

/* -------------------------------------------------- */

/*
 * EstimateSpace: Returns the estimated resulting size of the index (in bytes)
 *                acording to the provided build parameters.
 * Assumes: 'params' refers to a valid set of build parameters, a 'catalog' has already
 *          been set.
 */
double StarShapeSys::EstimateSpace(StarShapeBuildParams *params)
{
  double shapes;
  double b[8];
  DWORD  i;
  double sum, lambda;

  if ((iSInit == FALSE32) || (iParam == FALSE32))
    return 0;  

  // get the estimated number of lines to be found, plus the
  // lambda paramater found for the poisson distribution governing
  // the stars
  shapes  = EstimateShapes(params, &lambda);

  // 1. Center Star Index: DWORD * Lines
  b[0] = (double)sizeof(DWORD)* shapes;
  // 2. Nearest Star Index: DWORD * Lines
  b[1] = (double)sizeof(DWORD) * shapes;
  // 3. Third Star Index: DWORD * Lines
  b[2] = (double)sizeof(DWORD) * shapes;
  // 4. First Ratio: double * Lines
  b[3] = (double)sizeof(double) * shapes * (params->MinShapePoints-1);
  // 5. First Two Angles: double * Lines * 2
  b[4] = (double)sizeof(double) * shapes * params->MinShapePoints;
  // 6.  Bin Start Index: DWORD * Lines
  b[5] = (double)sizeof(DWORD) * shapes;
  // 7. Bin Length index: WORD * BIN_COUNT
  b[6] = (double)sizeof(WORD) * (params->BinCount+1);
  // 8. Bin Data: DWORD * BIN_COUNT
  b[7] = (double)sizeof(DWORD) * (params->BinCount+1);

  // add up all the costs to get the estimated sum in bytes
  sum = 0;
  for (i = 0; i < 8; i++)
  {
    sum += b[i];
  }

  return sum;
}

/* -------------------------------------------------- */

//#define ADD_RANDOM_DIST

/*
 * StarsGenIm: Returns an image in 'im' generated using the star list 'sList', the distractor
 *             list 'dList' (which may be set to NULL) according to the camera parameters in the 
 *             provided camera 'cam', with jitter 'jitter' and a percentage of stars from 'sList'
 *             dropped out according to 'drop_out'. The positions in the resulting image are
 *             all set within [-0.5,0.5]x[-0.5,0.5].
 * Assumes: 'im' is a valid StarImage, 'sList' is a valid StarList, 'dList', is a valid StarList
 *          is it is not set to NULL, 'cam' is a valid StarCamera, properly set to the parameters
 *          of the image that is to be taken, 'jitter' >= 0, drop_out in [0,1].
 */
void StarShapeSys::StarsGenIm(StarImage *im, StarList *sList, StarList *dList, 
                             StarCamera *cam, double jitter, double drop_out)
{
  BlockMemBank *tempStars;
  BYTE tempSpace[sizeof(double)*3+sizeof(DWORD)];
  BYTE *tptr;
  double pos3D[3];
  double pos2D[2];
  double tMag;
  DWORD i, goal, ind;
  Enumeration *tenum;
  DWORD distCnt, realCnt;
  double hjitter;

  // prep the jitter
  hjitter = jitter*0.5;
  ind = DIST_STAR_INDEX;

  // prep temp space
  tempStars = new BlockMemBank();
  tempStars->Init(sizeof(double)*3+sizeof(DWORD));

  srand(clock());

  // look through all the catalog stars
  goal = sList->StarCount;
  realCnt = 0;
  for (i = 0; i < goal; i++)
  {
    pos3D[0] = sList->List3D[0][i];
    pos3D[1] = sList->List3D[1][i];
    pos3D[2] = sList->List3D[2][i];

    cam->findScreenPos(pos3D, &pos2D[0], &pos2D[1]);
    if ((pos2D[0] != MAX_DOUBLE) && (pos2D[1] != MAX_DOUBLE))
    {
      // jitter position
      pos2D[0] += randD()*jitter - hjitter;
      pos2D[1] += randD()*jitter - hjitter;
      // test visibility
      if (!(((pos2D[0] < 0) || (pos2D[0] > 1)) || ((pos2D[1] < 0) || (pos2D[1] > 1))))
      {
        if (randD() < drop_out)
          continue;

        tptr = tempSpace;
        memcpy(tptr, &pos2D[0], sizeof(double));
        tptr += sizeof(double);
        memcpy(tptr, &pos2D[1], sizeof(double));
        tptr += sizeof(double);
        memcpy(tptr, &(sList->List[2][i]), sizeof(double));
        tptr += sizeof(double);
        memcpy(tptr, &i, sizeof(DWORD));
        tptr += sizeof(DWORD);

        tempStars->AddNewData(tempSpace);
        realCnt++;
      }
    }
  }

#ifdef ADD_RANDOM_DIST
  // add a number of star proportional to the number of catalog stars
  // in the image
  tMag = 0;
  for (i = 0; i < .2*realCnt; i++)
  {
    pos2D[0] = randD() - 0.5;
    pos2D[1] = randD() - 0.5;

    tptr = tempSpace;
    memcpy(tptr, &pos2D[0], sizeof(double));
    tptr += sizeof(double);
    memcpy(tptr, &pos2D[1], sizeof(double));
    tptr += sizeof(double);
    memcpy(tptr, &tMag, sizeof(double));
    tptr += sizeof(double);
    memcpy(tptr, &ind, sizeof(DWORD));
    tptr += sizeof(DWORD);

    tempStars->AddNewData(tempSpace);
  }
#endif

  // if a distractor list was sent, look through all the distractor stars
  if (dList != NULL)
  {
    goal = dList->StarCount;
    for (i = 0; i < goal; i++)
    {
      pos3D[0] = dList->List3D[0][i];
      pos3D[1] = dList->List3D[1][i];
      pos3D[2] = dList->List3D[2][i];

      cam->findScreenPos(pos3D, &pos2D[0], &pos2D[1]);
      if ((pos2D[0] != MAX_DOUBLE) && (pos2D[1] != MAX_DOUBLE))
      {
        // jitter position
        pos2D[0] += randD()*jitter - hjitter;
        pos2D[1] += randD()*jitter - hjitter;
        // check for visibility
        if (!(((pos2D[0] < 0) || (pos2D[0] > 1)) || ((pos2D[1] < 0) || (pos2D[1] > 1))))
        {
          if (randD() < drop_out)
            continue;

          tptr = tempSpace;
          memcpy(tptr, &pos2D[0], sizeof(double));
          tptr += sizeof(double);
          memcpy(tptr, &pos2D[1], sizeof(double));
          tptr += sizeof(double);
          memcpy(tptr, &(dList->List[2][i]), sizeof(double));
          tptr += sizeof(double);
          memcpy(tptr, &ind, sizeof(DWORD));
          tptr += sizeof(DWORD);

          tempStars->AddNewData(tempSpace);
        }
      }
    }
  }

  // put usable stars into image structure
  tenum = tempStars->NewEnum(FORWARD);
  im->Init(tempStars->GetCount());
  i = 0;
  while (tenum->GetEnumOK() == TRUE32)
  {
    tptr = (BYTE*)tenum->GetEnumNext();

    im->Stars[0][i] = *((double*)tptr) - 0.5;
    tptr += sizeof(double);
    im->Stars[1][i] = *((double*)tptr) - 0.5;
    tptr += sizeof(double);
    im->Stars[2][i] = *((double*)tptr);
    tptr += sizeof(double);
    im->TrueIndex[i] = *((DWORD*)tptr);
    i++;
  }
  delete tenum;

  // cleanup
  delete tempStars;

  // count the distractors
  goal  = im->StarCount;
  distCnt = 0;
  for (i = 0; i < goal; i++)
  {
    if (im->TrueIndex[i] == DIST_STAR_INDEX)
    {
      distCnt++;
    }
  }

#ifdef STAR_SHAPE_VERBOSE
  printf("Image contains %d catalogue stars\nImage contains %d distractor stars\n", im->StarCount-distCnt, distCnt);
  fflush(stdout);
#endif

  // set the image information
  cam->getBasis(im->VBasisI, im->VBasisJ, im->VBasisK);
  im->VScale = 1;
}

/* -------------------------------------------------- */

/*
 * SetStarList: Sets the star list to be used by the system for indexing.
 * Asumes: 'list' is a valid StarList.
 */
void StarShapeSys::SetStarList(StarList *list)
{
  sList = list;

  if (sList != NULL)
    iSInit = TRUE32;
  else
    iSInit = FALSE32;
}

/* -------------------------------------------------- */

/*
 * InitBuildParams: Preps a set of build parameters, automatically creating file names
 *                  and parameters for all combinations of wedge angles and stars per shapes.
 *                  Uses 'outfileBase' as the beginning of each index names, and appends a number and
 *                  '.bin' to the end of each individual index. 'MaxImAngle' refers to the view angle
 *                  at which to index. 'MaxShapes' refers to the maximum number of shapes to index around
 *                  each star. 'BinCount' refers to the number of hashing bins to use in the final index.
 *                  'wedgeCounts' and 'wedgeList' describe the various wedge angles to use, and 'shapeStars' and
 *                  'starsPShapeList' describe the various stars per shape to use.
 * Assumes: 'outfileBase' is a valid file name, MaxImAngle > 0, MaxShapes > 0, BinCount > 0, wedgeCounts > 0
 *          'wedgeList' contaings a list of 'wedgeCounts' wedge angles all in (0, 360].
 */
void StarShapeSys::InitBuildParams(char *outfileBase, double MaxImAngle, DWORD MaxShapes, DWORD BinCount,
                                   DWORD wedgeCounts, double *wedgeList, DWORD shapeStars, DWORD *starsPShapeList)
{
  double vec1[3], vec2[3];
  DWORD i, j, cnt;

  // pre-clean
  if (BuildParams != NULL)
    delete [] BuildParams;

  // init build param space
  BuildParamCount = shapeStars*wedgeCounts;
  BuildParams     = new StarShapeBuildParams[BuildParamCount];

  // set indexing shape info
  MAX_IM_ANGLE = MaxImAngle;
  MAX_SHAPES   = MaxShapes;
  vec1[0] = cos(0*RAD_CONV)*sin(0*RAD_CONV);
  vec1[1] = sin(0*RAD_CONV)*sin(0*RAD_CONV);
  vec1[2] = cos(0*RAD_CONV);
  vec2[0] = cos(0*RAD_CONV)*sin(MAX_IM_ANGLE*RAD_CONV);
  vec2[1] = sin(0*RAD_CONV)*sin(MAX_IM_ANGLE*RAD_CONV);
  vec2[2] = cos(MAX_IM_ANGLE*RAD_CONV);
  MAX_BUILD_DIST = (vec1[0]-vec2[0])*(vec1[0]-vec2[0])+
                   (vec1[1]-vec2[1])*(vec1[1]-vec2[1])+
                   (vec1[2]-vec2[2])*(vec1[2]-vec2[2]);
  MAX_BUILD_DIST = sqrt(MAX_BUILD_DIST) * 0.5 * 1.05;

  // create each set of buidl parameters, the appropriate output name,
  // and determine the minimum number of stars per shape to use.
  MIN_SHAPE_POINTS = 0;
  ANGLE_WEDGE = 0;
  cnt = 0;
  for (i = 0; i < shapeStars; i++)
  {
    if (starsPShapeList[i] > MIN_SHAPE_POINTS)
      MIN_SHAPE_POINTS = starsPShapeList[i];

    for (j = 0; j < wedgeCounts; j++)
    {
      BuildParams[cnt].AngleWedge     = wedgeList[j];
      BuildParams[cnt].BinCount       = BinCount;
      BuildParams[cnt].MaxImAngle     = MaxImAngle;
      BuildParams[cnt].MinShapePoints = starsPShapeList[i];
      sprintf(BuildParams[cnt].OutFile, "%s%u.bin", outfileBase, cnt);
      cnt++;
    }
  }
  // determine the max wedge angle
  for (i = 0; i < wedgeCounts; i++)
  {
    if (wedgeList[i] > ANGLE_WEDGE)
      ANGLE_WEDGE = wedgeList[i];
  }

  // since acos is a strictly decreasing function, we can calculate a cutoff
  // value to test dot products against rather than calculating the full acos
  // during the build phase.
  COS_CUTOFF   = cos(MAX_IM_ANGLE*RAD_CONV*0.5);
  WEDGE_CUTOFF = ANGLE_WEDGE*RAD_CONV*0.5;

  iParam = TRUE32;
}

/* -------------------------------------------------- */

/*
 * InitMatchParams: Preps a set of image matching parameters.
 * Assumes: 'params' refers to a valid set of matching parameters.
 */
void StarShapeSys::InitMatchParams(StarShapeMatchParams *params)
{
  MIN_MATCH_SHAPE_POINTS = MIN_SHAPE_POINTS;
  MATCH_TOL_MIN          = params->DistMatchTolMin;
  MATCH_TOL_MAX          = params->DistMatchTolMax;
  RAD_TOL_MIN            = params->AngMatchTolMin * RAD_CONV;
  RAD_TOL_MAX            = params->AngMatchTolMax * RAD_CONV;
}

/* -------------------------------------------------- */

/*
* Internal function:
 * grabIndexDat: determines if a particular star 'j' in the star catalog is within the proscribed
 *               distance to the current central star, whose position is represented via 'dir'.
 *               If it is, the 2D position of the star relative to an image centered on 'dir' is 
 *               determined, as well as the relative angle the star makes in the image.
 *               the relative distance within the image is also recorded, and the count of stars
 *               that have been images is increased via 'cCount'.
 */
inline void StarShapeSys::grabIndexDat(DWORD *candidates, double **imstars, double *angles, double *distList,
                                      DWORD *cCount, StarCamera *cam, double *dir, DWORD j)
{
  DWORD tc;
  double pos3D[3];
  double pos2D[2];
  double idist;
  double ang;

  // grab current number of listed stars
  tc = *cCount;

  // grab 3D position
  pos3D[0] = sList->List3D[0][j];
  pos3D[1] = sList->List3D[1][j];
  pos3D[2] = sList->List3D[2][j];

  // first check angle between point and camera
  ang = pos3D[0]*dir[0] + pos3D[1]*dir[1] + pos3D[2]*dir[2];
  if (ang >= COS_CUTOFF)
  {
    // finally image the point
    if (cam->findScreenPos(pos3D, &pos2D[0], &pos2D[1]) == TRUE32)
    {
      candidates[tc] = j;
      imstars[0][tc] = pos2D[0]-0.5;
      imstars[1][tc] = pos2D[1]-0.5;
      idist          = sqrt(imstars[0][tc]*imstars[0][tc] + imstars[1][tc]*imstars[1][tc]);
      distList[tc]   = idist;
      idist          = 1.0 / idist;
      angles[tc]     = atan2(imstars[1][tc]*idist, imstars[0][tc]*idist);
      if (angles[tc] < 0)
        angles[tc] += 2*PI;
      tc++;
    }
  }
  *cCount = tc;
}

/* -------------------------------------------------- */

double *tempDistList    = NULL; // distance of 'nearby' stars
double *tempAngleList   = NULL; // angles of 'nearby' stars
DWORD  *tempNearList    = NULL; // id list of 'nearby' stars
double tempWedgeCutoff;         // wedge cutoff for the current wedge size

/*
 * compareDistWedge: comparision function used by quicksort to sort
 *                   stars within a wedge. This particular version sorts so that
 *                   stars outside the current wedge are sent to the back of the list
 *                   and only those within the wedge are sorted near the front.
 */
int compareDistsWedge(const void *e1, const void *e2)
{
  DWORD d1, d2;
  double tang;

  d1 = *((DWORD*)e1);
  d2 = *((DWORD*)e2);

  // check if either of the values is past the wedge cutoff -- if so, 
  // it's always farther away than any point within the wedge cutoff
  tang = tempAngleList[d1];
  tang = ABS(tang);
  if (tang > tempWedgeCutoff)
    return 1;
  tang = tempAngleList[d2];
  tang = ABS(tang);
  if (tang > tempWedgeCutoff)
    return -1;

  // otherwise compare distances, then angles
  if (tempDistList[tempNearList[d1]] < tempDistList[tempNearList[d2]])
    return -1;
  else if (tempDistList[tempNearList[d1]] > tempDistList[tempNearList[d2]])
    return 1;

  if (tempAngleList[d1] < tempAngleList[d2])
    return -1;
  else if (tempAngleList[d1] > tempAngleList[d2])
    return 1;

  return 0;
}

/*
 * compareDistWedge: comparision function used by quicksort to sort
 *                   stars within a wedge.
 */
int compareDists(const void *e1, const void *e2)
{
  DWORD d1, d2;

  d1 = *((DWORD*)e1);
  d2 = *((DWORD*)e2);

  // compare by distance, then by angle
  if (tempDistList[tempNearList[d1]] < tempDistList[tempNearList[d2]])
    return -1;
  else if (tempDistList[tempNearList[d1]] > tempDistList[tempNearList[d2]])
    return 1;

  if (tempAngleList[d1] < tempAngleList[d2])
    return -1;
  else if (tempAngleList[d1] > tempAngleList[d2])
    return 1;

  return 0;
}

/* -------------------------------------------------- */

DWORD *tempBinList = NULL;  // bin id for each shape

/*
 * compareBins: functino used by qsort to sort shapes according
 *              to the index bin they fall into.
 */
int compareBins(const void *e1, const void *e2)
{
  DWORD d1, d2;

  d1 = *((DWORD*)e1);
  d2 = *((DWORD*)e2);

  if (tempBinList[d1] < tempBinList[d2])
    return -1;
  else if (tempBinList[d1] > tempBinList[d2])
    return 1;

  return 0;
}

/* -------------------------------------------------- */

/*
 * WriteDensity: Writes the density of stars around every star in the pre-set 
 *               catalog to file 'fname', using the image angle set using InitBuildPArams.
 * Assumes: The class StarList is already set, InitBuildParams has already been successfully called, 
 *          and 'fname' refers to a valid file name.
 */
BOOL32 StarShapeSys::WriteDensity(char *fname)
{
  DWORD i, starCount;
  FILE *fp;
  StarKDTree kdtree;
  BlockMemBank  tempRetBank;
  DWORD  *candidates;
  DWORD  *index;
  DWORD  *nearList;
  double *nearAList;
  double *distList;
  double *imstars[2];
  double *angles;
  StarCamera cam;
  double dir[3];
  double tstar[3];
  Enumeration  *tempRetEnum;
  DWORD cCount;
  DWORD *tret;

  // check if params ar set
  if (iParam == FALSE32)
    return FALSE32;
  // check if star list is set
  if (iSInit == FALSE32)
    return FALSE32;
  // open output file
  fp = fopen(fname, "wt");
  if (fp == NULL)
    return FALSE32;

  starCount = sList->StarCount;

  // build temp space
  angles     = new double[sList->StarCount];
  candidates = new DWORD[sList->StarCount];
  index      = new DWORD[sList->StarCount];
  nearList   = new DWORD[sList->StarCount];
  nearAList  = new double[sList->StarCount];
  distList   = new double[sList->StarCount];
  imstars[0] = new double[sList->StarCount];
  imstars[1] = new double[sList->StarCount];

  // create the kd-tree
#ifdef STAR_SHAPE_VERBOSE
  printf("Building KDTree\n");
  fflush(stdout);
#endif
  kdtree.BuildTree(starCount, sList->List3D);
  tempRetBank.Init(sizeof(DWORD), 2000);

  cam.setLens(MAX_IM_ANGLE);

#ifdef STAR_SHAPE_VERBOSE
  printf("Extracting densities\n");
  fflush(stdout);
#endif

  // for each star, find all 'nearby' stars and record the density to file
  fprintf(fp, "%u\n", starCount);
  for (i = 0; i < starCount; i++)
  {
    cam.set(0, 0, 0,
            sList->List3D[0][i], sList->List3D[1][i], sList->List3D[2][i],
            0, 0, 1);
    cam.getDirection(dir);

    tstar[0] = sList->List3D[0][i];
    tstar[1] = sList->List3D[1][i];
    tstar[2] = sList->List3D[2][i];
    tempRetBank.Empty();
    kdtree.FindInDist(tstar, MAX_BUILD_DIST, &tempRetBank);
    tempRetEnum = tempRetBank.NewEnum(FORWARD);
    cCount = 0;
    while (tempRetEnum->GetEnumOK() == TRUE32)
    {
      tret = (DWORD*)tempRetEnum->GetEnumNext();
      if (*tret != i)
        grabIndexDat(candidates, imstars, angles, distList, &cCount, &cam, dir, *tret);
    }
    fprintf(fp, "%u\n", cCount);

#ifdef STAR_SHAPE_VERBOSE
    printf(".");
    if (((i+1) % 60) == 0)
      printf(" %u/%u\n", i+1, starCount);
    fflush(stdout);
#endif
  }
#ifdef STAR_SHAPE_VERBOSE
  printf("\n");
  fflush(stdout);
#endif

  fclose(fp);

  // cleanup
  delete [] angles;
  delete [] candidates;
  delete [] index;
  delete [] nearList;
  delete [] nearAList;
  delete [] distList;
  delete [] imstars[0];
  delete [] imstars[1];

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * checkExpShapeDensity: value comparison function used for lookup
 *                       by the HashTable class in BuildIndex.
 */
BOOL32 checkExpShapeDensity(void *found, void *check)
{
  if (((StarExpShapeCount*)found)->density == *((DWORD*)check))
    return TRUE32;

  return FALSE32;
}

/* -------------------------------------------------- */

double *tempIndVal; // reference to current expected shapes per star

/*
 * cmpIndVals: function used by qsort for sorting various sets of indexing
 *             parameters in order from fewest to greatest number of expected
 *             shapes per star.
 */
int cmpIndVals(void const *e1, void const *e2)
{
  DWORD d1, d2;

  d1 = *((DWORD*)e1);
  d2 = *((DWORD*)e2);

  if (tempIndVal[d1] < tempIndVal[d2])
    return -1;
  else if (tempIndVal[d1] > tempIndVal[d2])
    return 1;

  return 0;
}

/* -------------------------------------------------- */

/*
 * BuildIndex: Builds a set of indexes based on the parameters provided to InitBuildParams,
 *             and the star catalog provided to SetStarList.
 * Assumes: InitBuildParams has been successfully called, and SetStarList has been successfully 
 *          called.
 */
BOOL32 StarShapeSys::BuildIndex()
{
  double *angles;
  double adif, idist, tval;
  DWORD  i, j, k, cCount, aCount, tlen, starCount;
  DWORD  *parCount, pCount;
  DWORD  *candidates;
  DWORD  *index;
  DWORD  *nearList;
  double *nearAList;
  DWORD   bLast;
  double *distList;
  double *imstars[2];
  double  tang;
  double dir[3];
  double tstar[3];
  BlockMemBank **ShapeList;
  Enumeration  *tShapeEnum;
  BYTE *tempSpace;
  BYTE *tempPtr;
  BlockMemBank **TempShapeBank;
  Enumeration  *TempShapeEnum;
  BlockMemBank *ExpShapeBank;
  BlockMemBank *ExpOrderBank;
  HashTable    *ExpShapeTable;
  DWORD shapeCount;
  double *indVal;
  DWORD bStart, bNum, bCount;
#ifdef STAR_SHAPE_KDTREE
  StarKDTree kdtree;
  BlockMemBank  tempRetBank;
  Enumeration  *tempRetEnum;
#endif
  double *WedgeCutoff;
  StarCamera cam;
  double nearStarTally;
  DWORD goal, *tret;
  BaseTimer timer;
  DWORD *acceptCount;
  DWORD param, iparam;
  DWORD *indOrder;
  StarExpShapeCount *expShapes;
#ifdef STAR_SHAPE_DEBUG
  FILE *shapeCountFile;
  FILE **shapeIOFile;
  char outShapeFile[2048];
#endif
 
  // ensure indexer has been properly set up
  if ((iParam == FALSE32) || (iSInit == FALSE32))
    return FALSE32;

  // start timing
  timer.Start();

  // grab number of stars in catalog
  starCount = sList->StarCount;

  // init temp space
  angles     = new double[starCount];
  candidates = new DWORD[starCount];
  index      = new DWORD[starCount];
  nearList   = new DWORD[starCount];
  nearAList  = new double[starCount];
  distList   = new double[starCount];
  imstars[0] = new double[starCount];
  imstars[1] = new double[starCount];
  acceptCount= new DWORD[BuildParamCount];

  // init temp. resizeable index space
  parCount  = new DWORD[BuildParamCount];
  ShapeList = new BlockMemBank*[BuildParamCount];
  tempSpace = new BYTE[sizeof(DWORD)*3 + sizeof(double) + (sizeof(double)+sizeof(double))*(MIN_SHAPE_POINTS-1)];
  TempShapeBank = new BlockMemBank*[BuildParamCount];
  WedgeCutoff   = new double[BuildParamCount];
  for (i = 0; i < BuildParamCount; i++)
  {
    // size of each shape's data in bytes
    pCount = sizeof(DWORD)*3 + sizeof(double) + (sizeof(double)+sizeof(double))*(BuildParams[i].MinShapePoints-1);
    // official shape bank
    ShapeList[i] = new BlockMemBank();
    ShapeList[i]->Init(pCount, 2000);
    // temporary shape bank
    TempShapeBank[i] = new BlockMemBank();
    TempShapeBank[i]->Init(pCount, 50);
    WedgeCutoff[i] = BuildParams[i].AngleWedge*RAD_CONV*0.5;
  }
  ExpShapeBank = new BlockMemBank();
  ExpShapeBank->Init(sizeof(StarExpShapeCount), 100);
  ExpOrderBank = new BlockMemBank();
  ExpOrderBank->Init(sizeof(DWORD)*BuildParamCount, 100);
  ExpShapeTable = new HashTable(100, 0.5f, checkExpShapeDensity);
  indVal = new double[BuildParamCount];

#ifdef STAR_SHAPE_VERBOSE
#ifdef STAR_SHAPE_KDTREE
  printf("Building KDTree\n");
  fflush(stdout);
#endif
#endif

#ifdef STAR_SHAPE_KDTREE
  // build the kd-tree for star lookup
  kdtree.BuildTree(starCount, sList->List3D);
  tempRetBank.Init(sizeof(DWORD), 2000);
#endif
  
  nearStarTally = 0;

  // find all lines for each star
  cam.setLens(MAX_IM_ANGLE);

#ifdef STAR_SHAPE_DEBUG
  shapeIOFile = new FILE*[BuildParamCount];
  for (param = 0; param < BuildParamCount; param++)
  {
    sprintf(outShapeFile, "shapeOK%u.txt", param);
    shapeIOFile[param] = fopen(outShapeFile, "wt");
    fprintf(shapeIOFile[param], "%u\n", starCount);
  }
  sprintf(outShapeFile, "shapeCount.txt");
  shapeCountFile = fopen(outShapeFile, "wt");
  fprintf(shapeCountFile, "%u\n", starCount);
#endif

#ifdef STAR_SHAPE_VERBOSE
  printf("Indexing\n");
  fflush(stdout);
#endif

  nearStarTally = 0;
  memset(acceptCount, 0, sizeof(DWORD)*BuildParamCount);
  for (i = 0; i < starCount; i++)
  {
    // init camera direction
    cam.set(0, 0, 0,
            sList->List3D[0][i], sList->List3D[1][i], sList->List3D[2][i],
            0, 0, 1);
    cam.getDirection(dir);

    // grab the 3D position of the current star
    tstar[0] = sList->List3D[0][i];
    tstar[1] = sList->List3D[1][i];
    tstar[2] = sList->List3D[2][i];

#ifdef STAR_SHAPE_KDTREE
    // find all the stars close 'enough' to the current star of interest
    tempRetBank.Empty();
    kdtree.FindInDist(tstar, MAX_BUILD_DIST, &tempRetBank);
    tempRetEnum = tempRetBank.NewEnum(FORWARD);
    // look through the stars, perform a final check and transform them into 2D coordinates
    cCount = 0;
    while (tempRetEnum->GetEnumOK() == TRUE32)
    {
      tret = (DWORD*)tempRetEnum->GetEnumNext();
      if (*tret != i)
        grabIndexDat(candidates, imstars, angles, distList, &cCount, &cam, dir, *tret);
    }
#else
    // safe, slower version used to find all 'nearby' stars

    // grab all the stars that should be considered -- but ignore the i'th star
    cCount = 0;
    for (j = 0; j < i; j++)
    {
      grabIndexDat(candidates, imstars, angles, distList, &cCount, &cam, dir, j);
    }
    for (j = i+1; j < starCount; j++)
    {
      grabIndexDat(candidates, imstars, angles, distList, &cCount, &cam, dir, j);
    }
#endif

    // zero star counts in wedge for each wedge size
    memset(parCount, 0, sizeof(DWORD)*BuildParamCount);

    nearStarTally += cCount;
    // consider each near star as the direction star
    for (j = 0; j < cCount; j++)
    {
      // find all points within the directed wedge
      adif = PI-angles[j];
      pCount = 0;
      memset(parCount, 0, sizeof(DWORD)*BuildParamCount);
      for (k = 0; k < cCount; k++)
      {
        // see if the star falls within the largest wedge
        tang = mod(angles[k] + adif, 2*PI) - PI;
        if (ABS(tang) <= WEDGE_CUTOFF)
        {
          nearList[pCount]  = k;
          nearAList[pCount] = tang;
          pCount++;

          // determine which of the wedge sizes the star falls into
          tang = ABS(tang);
          for (param = 0; param < BuildParamCount; param++)
          {
            if (tang <= WedgeCutoff[param])
            {
              parCount[param]++;
            }
          }
        }
      }

      // try to build shapes for ALL sets of parameters
      for (param = 0; param < BuildParamCount; param++)
      {
        // if the shape produces enough points, save the shape
        if (parCount[param] >= BuildParams[param].MinShapePoints)
        {
          // organize points by increasing distance within the wedge
          tempDistList    = distList;
          tempNearList    = nearList;
          tempAngleList   = nearAList;
          tempWedgeCutoff = WedgeCutoff[param];
          for (k = 0; k < pCount; k++)
          {
            index[k] = k;
          }
          // we have to do this sort here, since the sort uses the angle of stars
          // relative to the orienting star to break ties.
          qsort(index, pCount, sizeof(DWORD), compareDistsWedge);

          // record all the ratios
          idist = 1.0 / distList[nearList[index[BuildParams[param].MinShapePoints-1]]];
          // number of the center star
          tempPtr = tempSpace;
          *((DWORD*)tempPtr) = i;                             // center star
          tempPtr += sizeof(DWORD);
          *((DWORD*)tempPtr) = candidates[nearList[index[BuildParams[param].MinShapePoints-1]]];  // farthest star
          tempPtr += sizeof(DWORD);
          *((DWORD*)tempPtr) = candidates[nearList[index[0]]]; // first non-central star
          tempPtr += sizeof(DWORD);
          // record star angles within the wedge (since the following loop skips the
          // first element on behalf of the ratio list)
          *((double*)tempPtr) = nearAList[index[BuildParams[param].MinShapePoints-1]];
          tempPtr += sizeof(double);
          // recording remaining ratios
          for (k = 1; k < BuildParams[param].MinShapePoints; k++)
          {
            tval = idist * distList[nearList[index[k-1]]];
            *((double*)tempPtr) = nearAList[index[k-1]];
            tempPtr += sizeof(double);
            *((double*)tempPtr) = tval;
            tempPtr += sizeof(double);
          }
          TempShapeBank[param]->AddNewData(tempSpace);
        }
      }
    }

    // obtain the order of parameters to use. First see if the parameters are already calculated,
    // and if not, calculate let them for the first and last time.
    if (ExpShapeTable->Find(cCount, (void**)&expShapes, &cCount) == TRUE32)
    {
      indOrder = expShapes->indOrder;
    } else
    {
      // allocate a new expected shape count structure
      expShapes = (StarExpShapeCount*)ExpShapeBank->NewData();
      expShapes->density  = cCount;
      expShapes->indOrder = (DWORD*)ExpOrderBank->NewData();
      indOrder = expShapes->indOrder;
      // calculate values
      for (j = 0; j < BuildParamCount; j++)
      {
        indOrder[j] = j;
        indVal[j]   = (1.0 - sumPoiss(BuildParams[j].MinShapePoints - 2, ((double)cCount)*(BuildParams[j].AngleWedge)/360.0)) * (double)cCount;
      }
      // determine the sorted order of values from smallest to largest
      tempIndVal = indVal;
      qsort(indOrder, BuildParamCount, sizeof(DWORD), cmpIndVals);
      ExpShapeTable->Add(cCount, expShapes, DM_IGNORE);
    }

    // for each parameter set, add the temporary shapes to the complete global list
    // while fewer than the maximum number of shapes have been added
    shapeCount = 0;
    for (iparam = 0; iparam < BuildParamCount; iparam++)
    {
      param = indOrder[iparam];
      if (shapeCount < MAX_SHAPES)
      {
        // copy all the shapes into the real star list
        acceptCount[param]++;
        TempShapeEnum = TempShapeBank[param]->NewEnum(FORWARD);
        while (TempShapeEnum->GetEnumOK() == TRUE32)
        {
          tempPtr = (BYTE*)TempShapeEnum->GetEnumNext();
          ShapeList[param]->AddNewData(tempPtr);
        }
        delete TempShapeEnum;
        shapeCount += TempShapeBank[param]->GetCount();

#ifdef STAR_SHAPE_DEBUG
        fprintf(shapeIOFile[param], "%u\n", TempShapeBank[param]->GetCount());
      } else
      {
        fprintf(shapeIOFile[param], "0\n");
      }
#else
      }
#endif
      TempShapeBank[param]->Empty();
    }
#ifdef STAR_SHAPE_DEBUG
    fprintf(shapeCountFile, "%u\n", shapeCount);
#endif

#ifdef STAR_SHAPE_VERBOSE
    // output progress
    printf(".");
    if (((i+1) % 60) == 0)
    {
      printf(" %u/%u %lg A.Stars\n", i+1, starCount, nearStarTally/(i+1));
      for (param = 0; param < BuildParamCount; param++)
      {
        printf("%u: %g A.Shapes  ", param, (double)ShapeList[param]->GetCount()/acceptCount[param]);
      }
      printf("\n");
    }
    fflush(stdout);
#endif

  }
  // delete expected shape order info
  delete ExpShapeTable;
  delete ExpOrderBank;
  delete ExpShapeBank;
  delete [] indVal;

#ifdef STAR_SHAPE_DEBUG
  for (param = 0; param < BuildParamCount; param++)
  {
    fclose(shapeIOFile[param]);
  }
  fclose(shapeCountFile);
#endif

#ifdef STAR_SHAPE_VERBOSE
  for (param = 0; param < BuildParamCount; param++)
  {
    printf("\n\nIndex %u:\n", param);
    printf("Finished: %u/%u -- %u shapes\n", i, starCount, ShapeList[param]->GetCount());
    printf("%u stars accepted\n", acceptCount[param]);
  }
  fflush(stdout);
#endif

  // delete array space
  delete [] angles;
  delete [] candidates;
  delete [] index;
  delete [] nearList;
  delete [] nearAList;
  delete [] distList;
  delete [] imstars[0];
  delete [] imstars[1];

  delete [] acceptCount;
  delete [] tempSpace;

  // build and save final indexes
  for (param = 0; param < BuildParamCount; param++)
  {
    // store matching information
    iShapeCount = ShapeList[param]->GetCount();
    iStarPoint  = new DWORD[iShapeCount];
    iNearPoint  = new DWORD[iShapeCount];
    iThirdPoint = new DWORD[iShapeCount];
    iCRatioList = new double[iShapeCount*(BuildParams[param].MinShapePoints-1)];
    iAngleList  = new double [iShapeCount*BuildParams[param].MinShapePoints];

    // extract all shape info from temp space into final lists
    pCount = 0;
    aCount = 0;
    tShapeEnum  = ShapeList[param]->NewEnum(FORWARD);
    for (i = 0; i < iShapeCount; i++)
    {
      tempPtr = (BYTE*)tShapeEnum->GetEnumNext();

      iStarPoint[i]  = *((DWORD*)tempPtr);
      tempPtr += sizeof(DWORD);

      iNearPoint[i]  = *((DWORD*)tempPtr);
      tempPtr += sizeof(DWORD);

      iThirdPoint[i] = *((DWORD*)tempPtr);
      tempPtr += sizeof(DWORD);

      iAngleList[aCount++] = *((double*)tempPtr);
      tempPtr += sizeof(double);

      for (j = 1; j < BuildParams[param].MinShapePoints; j++, aCount++, pCount++)
      {
        iAngleList[aCount] = *((double*)tempPtr);
        tempPtr += sizeof(double);
        iCRatioList[pCount] = *((double*)tempPtr);
        tempPtr += sizeof(double);
      }
    }
    delete tShapeEnum;

    // delete temp index space
    delete ShapeList[param];
    delete TempShapeBank[param];

    // build temp bin space
    candidates = new DWORD[iShapeCount];
    index      = new DWORD[iShapeCount];
    for (i = 0; i < iShapeCount; i++)
    {
      index[i] = i;
      candidates[i] = (DWORD)floor(iCRatioList[i*(BuildParams[param].MinShapePoints-1)]*BuildParams[param].BinCount);
      assert(candidates[i] <= BuildParams[param].BinCount);
    }

    // sort shapes
    tempBinList = candidates;
    qsort(index, iShapeCount, sizeof(DWORD), compareBins);

    // build bin space
    iBinStart  = new DWORD[BuildParams[param].BinCount+1];
    iBinLength = new WORD[BuildParams[param].BinCount+1];
    iBinData   = new DWORD[iShapeCount];

    // organize all bins
    bLast = 0;
    bStart = 0;
    bNum   = 0;
    bCount = 0;
    i = 0;
    goal = BuildParams[param].BinCount+1;
    iBinStart[0]  = 0;
    iBinLength[0] = 0;
    while (i < iShapeCount)
    {
      iBinData[i] = index[i];

      tlen  = candidates[index[i]];
      for (j = bLast+1; j < tlen; j++)
      {
        iBinStart[j]  = bStart;
        iBinLength[j] = 0;
      }
      iBinStart[tlen] = bStart;
      bCount = 1;
      bStart++;
      i++;
      while ((i < iShapeCount) && (candidates[index[i]] == tlen))
      {
        iBinData[i] = index[i];
        bStart++;
        bCount++;
        i++;
      }
      iBinLength[tlen] = (WORD)bCount;
      bLast = tlen;
    }
    for (j = bLast+1; j < goal; j++)
    {
      iBinStart[j]  = bStart;
      iBinLength[j] = 0;
    }

    // delete temp bin space
    delete [] candidates;
    delete [] index;

    // save resulting index
    SaveIndex(&BuildParams[param]);

    Deinit();
  }
  // final cleanup
  delete [] ShapeList;
  delete [] TempShapeBank;

  delete [] BuildParams;
  BuildParams = NULL;

  // report elapsed time
  printf("\n\nTime: %f secs\n", timer.Elapsed());

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * OpenIndex: Read a binary index from file 'fname'.
 * Assumes: 'fname' refers to a valid shape index file.
 */
BOOL32 StarShapeSys::OpenIndex(char *fname)
{
  FILE *fp;
  BOOL32 iSwap;
  WORD eval;
  DWORD i, goal;

  Deinit();

  fp = fopen(fname, "rb");
  if (fp == NULL)
    return FALSE32;

  // read endian info
  if (fread(&eval, sizeof(WORD), 1, fp) < 1)
  {
    return FALSE32;
  }

  // check endian info
  if (getEndian() != eval)
    iSwap = TRUE32;
  else
    iSwap = FALSE32;

  // --- read header info ---

  // read number of lines in index
  if (fread(&iShapeCount, sizeof(DWORD), 1, fp) < 1)
  {
    return FALSE32;
  }
  // read maximum image radius in degrees used to index with
  if (fread(&MAX_IM_ANGLE, sizeof(double), 1, fp) < 1)
  {
    return FALSE32;
  }
  // read wedge size used for indexing
  if (fread(&ANGLE_WEDGE, sizeof(double), 1, fp) < 1)
  {
    return FALSE32;
  }
  // read minimum number of points per line used in indexing
  if (fread(&MIN_SHAPE_POINTS, sizeof(DWORD), 1, fp) < 1)
  {
    return FALSE32;
  }
  // read number of bins used in the index
  if (fread(&BIN_COUNT, sizeof(DWORD), 1, fp) < 1)
  {
    return FALSE32;
  }

  // swap byte order as needed
  if (iSwap == TRUE32)
  {
    swapEndian(&iShapeCount);
    swapEndian(&MAX_IM_ANGLE);
    swapEndian(&ANGLE_WEDGE);
    swapEndian(&MIN_SHAPE_POINTS);
    swapEndian(&BIN_COUNT);
  }
  // calculate tolerance in radians
  WEDGE_CUTOFF = ANGLE_WEDGE * RAD_CONV * 0.5;

  // --- read data ---

  if (iShapeCount != 0)
  {
    iStarPoint = new DWORD[iShapeCount];
    // read origin star number for each line
    if (fread(iStarPoint, sizeof(DWORD)*iShapeCount, 1, fp) < 1)
    {
      return FALSE32;
    }
    iNearPoint = new DWORD[iShapeCount];
    // read nearest star number for each line
    if (fread(iNearPoint, sizeof(DWORD)*iShapeCount, 1, fp) < 1)
    {
      return FALSE32;
    }
    iThirdPoint = new DWORD[iShapeCount];
    // read third star number for each line
    if (fread(iThirdPoint, sizeof(DWORD)*iShapeCount, 1, fp) < 1)
    {
      return FALSE32;
    } 
    iCRatioList = new double[iShapeCount*(MIN_SHAPE_POINTS-1)];
    // read extra ratios
    if (fread(iCRatioList, sizeof(double)*iShapeCount*(MIN_SHAPE_POINTS-1), 1, fp) < 1)
    {
      return FALSE32;
    }
    iAngleList = new double[iShapeCount*MIN_SHAPE_POINTS];
    // read extra ratios
    if (fread(iAngleList, sizeof(double)*iShapeCount*MIN_SHAPE_POINTS, 1, fp) < 1)
    {
      return FALSE32;
    }

    iBinStart = new DWORD[BIN_COUNT+1];
    // read bin start positions
    if (fread(iBinStart, sizeof(DWORD)*(BIN_COUNT+1), 1, fp) < 1)
    {
      return FALSE32;
    }
    iBinLength = new WORD[BIN_COUNT+1];
    // read bin lengths
    if (fread(iBinLength, sizeof(WORD)*(BIN_COUNT+1), 1, fp) < 1)
    {
      return FALSE32;
    }
    iBinData = new DWORD[iShapeCount];
    // read bin content
    if (fread(iBinData, sizeof(DWORD)*iShapeCount, 1, fp) < 1)
    {
      return FALSE32;
    }

    // swap data byte order as needed
    if (iSwap == TRUE32)
    {
      for (i = 0; i < iShapeCount; i++)
      {
        swapEndian(&iStarPoint[i]);
        swapEndian(&iNearPoint[i]);
        swapEndian(&iThirdPoint[i]);
        swapEndian(&iBinData[i]);
      }

      goal = iShapeCount*(MIN_SHAPE_POINTS-1);
      for (i = 0; i < goal; i++)
      {
        swapEndian(&iCRatioList[i]);
      }

      goal = iShapeCount*MIN_SHAPE_POINTS;
      for (i = 0; i < goal; i++)
      {
        swapEndian(&iAngleList[i]);
      }

      goal = BIN_COUNT+1;
      for (i = 0; i < goal; i++)
      {
        swapEndian(&iBinStart[i]);
        swapEndian(&iBinLength[i]);
      }
    }
  }

  // note that we're now initialized
  iInit  = TRUE32;
  iSInit = TRUE32;

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * SaveIndex: Saves to file the current index info and the parameter info provided
 *            by the variable 'params'.
 * Assumes: The index has been properly built via BuildIndex, and the param info in
 *          'params' properly matches this data.
 */
BOOL32 StarShapeSys::SaveIndex(StarShapeBuildParams *params)
{
  FILE *fp;
  WORD eval;

  fp = fopen(params->OutFile, "wb");
  if (fp == NULL)
    return FALSE32;

  eval = getEndian();

  // write endian info
  if (fwrite(&eval, sizeof(WORD), 1, fp) < 1)
  {
    return FALSE32;
  }

  // --- record header info ---

  // record number of lines in index
  if (fwrite(&iShapeCount, sizeof(DWORD), 1, fp) < 1)
  {
    return FALSE32;
  }
  // record maximum image radius in degrees used to index with
  if (fwrite(&params->MaxImAngle, sizeof(double), 1, fp) < 1)
  {
    return FALSE32;
  }
  // record wedge size used for indexing
  if (fwrite(&params->AngleWedge, sizeof(double), 1, fp) < 1)
  {
    return FALSE32;
  }
  // record minimum number of points per line used in indexing
  if (fwrite(&params->MinShapePoints, sizeof(DWORD), 1, fp) < 1)
  {
    return FALSE32;
  }
  // record number of bins used in the index
  if (fwrite(&params->BinCount, sizeof(DWORD), 1, fp) < 1)
  {
    return FALSE32;
  }

  // --- record data ---

  // save origin star number for each line
  if (fwrite(iStarPoint, sizeof(DWORD)*iShapeCount, 1, fp) < 1)
  {
    return FALSE32;
  }
  // save nearest star number for each line
  if (fwrite(iNearPoint, sizeof(DWORD)*iShapeCount, 1, fp) < 1)
  {
    return FALSE32;
  }
  // save direction star number for each line
  if (fwrite(iThirdPoint, sizeof(DWORD)*iShapeCount, 1, fp) < 1)
  {
    return FALSE32;
  } 
  // save extra ratios
  if (fwrite(iCRatioList, sizeof(double)*iShapeCount*(params->MinShapePoints-1), 1, fp) < 1)
  {
    return FALSE32;
  }
  // save angle ratios
  if (fwrite(iAngleList, sizeof(double)*iShapeCount*params->MinShapePoints, 1, fp) < 1)
  {
    return FALSE32;
  }
  // save bin start positions
  if (fwrite(iBinStart, sizeof(DWORD)*(params->BinCount+1), 1, fp) < 1)
  {
    return FALSE32;
  }
  // save bin lengths
  if (fwrite(iBinLength, sizeof(WORD)*(params->BinCount+1), 1, fp) < 1)
  {
    return FALSE32;
  }
  // save bin content
  if (fwrite(iBinData, sizeof(DWORD)*iShapeCount, 1, fp) < 1)
  {
    return FALSE32;
  }

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * SaveRatioAsText: Saves the ratios in the shape index into a text file.
 * Assumes: 'fname' is a valid filename, the index has been properly initialized
 *          by BuildIndex or OpenIndex.
 */
BOOL32 StarShapeSys::SaveRatiosAsText(char *fname)
{
  FILE *out;
  DWORD i, j;

  out = fopen(fname, "wb");
  if (out == NULL)
  {
    return FALSE32;
  }

  fprintf(out, "%u %u\n", iShapeCount, iShapeCount*(MIN_SHAPE_POINTS-2));
  for (i = 0; i < iShapeCount; i++)
  {
    fprintf(out, "%lg\n", iCRatioList[i*(MIN_SHAPE_POINTS-1)]);
  }
  for (i = 0; i < iShapeCount; i++)
  {
    for (j = 0; j < MIN_SHAPE_POINTS-2; j++)
    fprintf(out, "%lg\n", iCRatioList[i*(MIN_SHAPE_POINTS-1)+j+1]);
  }
  fclose(out);

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * SaveAnglesAsText: Saves the angles in the shape index into a text file.
 * Assumes: 'fname' is a valid filename, the index has been properly initialized
 *          by BuildIndex or OpenIndex.
 */
BOOL32 StarShapeSys::SaveAnglesAsText(char *fname)
{
  FILE *out;
  DWORD i, goal;

  out = fopen(fname, "wb");
  if (out == NULL)
  {
    return FALSE32;
  }

  fprintf(out, "%u\n", iShapeCount*MIN_SHAPE_POINTS);
  goal = iShapeCount*MIN_SHAPE_POINTS;
  for (i = 0; i < goal; i++)
  {
    fprintf(out, "%lg\n", iAngleList[i]);
  }
  fclose(out);

  return TRUE32;
}

/* -------------------------------------------------- */

/* 
 * GetStarsAngle: Returns the angle, in radians, between the two points denoted by
 *                ra1, dec1, and ra2, dec2.
 * Assumes: the ra and dec information is in radians.
 */
double StarShapeSys::GetStarsAngle(double ra1, double dec1, double ra2, double dec2)
{
  double v2[2];
  double baseVal;
  double theta;
  
  v2[0] = ra1-ra2;
  v2[1] = dec1-dec2;
  baseVal = 1.0/sqrt(v2[0]*v2[0] + v2[1]*v2[1]);
  v2[0] *= baseVal;
  v2[1] *= baseVal;

  theta = -atan2(v2[1], v2[0])+PI;

  return theta;
}

/* -------------------------------------------------- */

/* 
 * SolveOneMatch: Returns TRUE32 if a plausible solution has been found using the info
 *                in te provided 'match' variable. The solution information is returned
 *                in the provided 'match' variable.
 * Assumes: The current StarList matches the one used by the current shape index.
 */
BOOL32 StarShapeSys::SolveOneMatch(StarImage *im, StarShapeMatch *match)
{
  double imp1[3], imp2[3], imp3[3];
  double tp1[3], tp2[3], tp3[3];
  double p1[3], p2[3], p3[3];
  DWORD i1, i2, i3;
  DWORD ti1, ti2, ti3;
  double A[9];
  double d;
  int    indx[3];
  double i[3], j[3], k[3];
  double viewAngle;
  double cTheta;
  double temp, z1;
  double a, b, c;
  double cTheta2;
  double l1, l2, l3;

  if (iSInit == FALSE32)
    return FALSE32;

  // grab star image indexes and corresponding catalog indexes
  i1  = match->imStar;
  ti1 = match->starNum;
  i2  = match->imNearStar;
  ti2 = match->nearStarNum;
  i3  = match->imThirdStar;
  ti3 = match->thirdStarNum;

  // grab in image star positions
  imp1[0] = im->Stars[0][i1]; imp1[1] = im->Stars[1][i1];
  imp2[0] = im->Stars[0][i2]; imp2[1] = im->Stars[1][i2];
  imp3[0] = im->Stars[0][i3]; imp3[1] = im->Stars[1][i3];

  // grab corresponding catalog star positions
  tp1[0] = sList->List3D[0][ti1]; 
  tp1[1] = sList->List3D[1][ti1];
  tp1[2] = sList->List3D[2][ti1];
  tp2[0] = sList->List3D[0][ti2]; 
  tp2[1] = sList->List3D[1][ti2];
  tp2[2] = sList->List3D[2][ti2];
  tp3[0] = sList->List3D[0][ti3]; 
  tp3[1] = sList->List3D[1][ti3];
  tp3[2] = sList->List3D[2][ti3];

  // grab angle between actual star positions 1 and 2
  cTheta  = dot(tp1, tp2);

  // calculate distance to image plane in pixel's
  a = imp1[0]*imp2[0]+imp1[1]*imp2[1];
  b = imp1[0]*imp1[0]+imp1[1]*imp1[1];
  c = imp2[0]*imp2[0]+imp2[1]*imp2[1];
  cTheta2 = cTheta*cTheta;
  temp = -2*a+(b+c)*cTheta2;
  temp = temp*temp;
  z1 = (1.0/sqrt(2)) * 
        sqrt( (-1.0/(cTheta2-1)) * 
              (-2*a + (b+c)*cTheta2 + 
                sqrt( -4*(cTheta2-1)*(-a*a+b*c*cTheta2) + temp)
              )
             );

  // distance is the same for all image star positions
  imp3[2] = imp2[2] = imp1[2] = z1;

  // create camera space positions
  p1[0] = imp1[0]; p1[1] = imp1[1]; p1[2] = imp1[2];
  p2[0] = imp2[0]; p2[1] = imp2[1]; p2[2] = imp2[2];
  p3[0] = imp3[0]; p3[1] = imp3[1]; p3[2] = imp3[2];
  normalize(p1);
  normalize(p2);
  normalize(p3);

  // now solve for the camera basis using the relationship
  // between the true star positions and the projected positions
  A[0] = tp1[0]; A[1] = tp1[1]; A[2] = tp1[2];
  A[3] = tp2[0]; A[4] = tp2[1]; A[5] = tp2[2];
  A[6] = tp3[0]; A[7] = tp3[1]; A[8] = tp3[2];
  if (luDCmp(A, 3, indx, &d) == FALSE32)
  {
    return FALSE32;
  }

  // solve for i and k using LU decomposition
  i[0] = p1[0]; i[1] = p2[0]; i[2] = p3[0]; 
  luBackSub(A, 3, indx, i);
  i[0] = -i[0]; i[1] = -i[1]; i[2] = -i[2];
  k[0] = p1[2]; k[1] = p2[2]; k[2] = p3[2]; 
  luBackSub(A, 3, indx, k);

  // solve for j using the cross product
  cross(j, k, i);

  l1 = sqrt(dot(i, i)) - 1;
  l1 = ABS(l1);
  l2 = sqrt(dot(j, j)) - 1;
  l2 = ABS(l2);
  l3 = sqrt(dot(k, k)) - 1;
  l3 = ABS(l3);

  if ((l1 > 0.01) || ((l2 > 0.01) || (l3 > 0.01)))
  {
    return FALSE32;
  }

  // determine view angle
  viewAngle = atan(0.5/imp1[2])*2;

  match->imI[0] = i[0]; match->imI[1] = i[1]; match->imI[2] = i[2];
  match->imJ[0] = j[0]; match->imJ[1] = j[1]; match->imJ[2] = j[2];
  match->imK[0] = k[0]; match->imK[1] = k[1]; match->imK[2] = k[2];
  match->viewAngle = viewAngle;

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * SolveImage: Tries to solve the image 'im', returning the plausible solution count
 *             in 'solveCount' and the actual solutions in 'solveList'. Returns FALSE32
 *             if the class is not properly prepared to solve yet.
 * Assumes: The class has been intialized via OpenIndex, has a valid associated catalog set
 *          via SetStarList, and that 'im' refers to a valid StarImage.
 */
BOOL32 StarShapeSys::SolveImage(StarImage *im, DWORD *solveCount, StarShapeMatch **solveList)
{
  DWORD i, j, k, p, m, a;
  DWORD starCount;
  double sx, sy, tx, ty;
  double *distList, *nearAList, *imstars[2], *angles, *tempRats;
  double idist, adif, tang;
  DWORD *nearList, *index;
  DWORD pCount, jPos, kGoal, pGoal, sI;
  double curRatio, dif, difa1, difa2;
  DWORD mStart, mGoal, skippy, extraStart;
  DWORD sLine, startNum, aStart, aEnd, lStart, lEnd, tskip, matchy;
  DWORD n, dn;
  BOOL32 iGood;
  BlockMemBank *solveBank;
  StarShapeMatch tsolve;
  Enumeration *tenum;
  StarShapeMatch *tSolvePtr;
  BYTE *bptr;
  double RadRegion;
  double temp;
  BOOL32 iSolved;
  double RadDif, RatioDif;
  double tMatchTol;
  double tMatchRad;
  double prop;

  RadDif   = RAD_TOL_MAX - RAD_TOL_MIN;
  RatioDif = MATCH_TOL_MAX - MATCH_TOL_MIN;

  starCount = im->StarCount;

  if ((iInit == FALSE32) || (iSInit == FALSE32))
    return FALSE32;

  RadRegion = WEDGE_CUTOFF+RAD_TOL_MAX;

  distList   = new double[starCount];
  imstars[0] = new double[starCount];
  imstars[1] = new double[starCount];
  angles     = new double[starCount];
  nearList   = new DWORD[starCount];
  nearAList  = new double[starCount];
  index      = new DWORD[starCount];
  tempRats   = new double[starCount];

  solveBank = new BlockMemBank();
  solveBank->Init(sizeof(StarShapeMatch), 100);
  tenum = solveBank->NewEnum(FORWARD);

  iSolved = FALSE32;
  for (i = 0; i < starCount; i++)
  {
    sx = im->Stars[0][i];
    sy = im->Stars[1][i];

    // find all distances and angles
    for (j = 0; j < starCount; j++)
    {
      tx = imstars[0][j] = im->Stars[0][j] - sx;
      ty = imstars[1][j] = im->Stars[1][j] - sy;

      distList[j] = sqrt(tx*tx + ty*ty);
      idist = 1.0 / distList[j];
      angles[j]   = atan2(ty * idist, tx * idist);
      if (angles[j] < 0)
        angles[j] += 2*PI;
    }

    for (j = 0; j < starCount; j++)
    {
      if (j == i)
        continue;

      // find all the points within the wedge, ignoring the center star 'i'
      adif = PI - angles[j];
      pCount = 0;
      for (k = 0; k < i; k++)
      {
        tang = mod(angles[k] + adif, 2*PI) - PI;
        if (ABS(tang) <= RadRegion)
        {
          nearList[pCount]  = k;
          nearAList[pCount] = tang;
          pCount++;
        }
      }
      for (k = i+1; k < starCount; k++)
      {
        tang = mod(angles[k] + adif, 2*PI) - PI;
        if (ABS(tang) <= RadRegion)
        {
          nearList[pCount]  = k;
          nearAList[pCount] = tang;
          pCount++;
        }
      }
 
      // if the line produces enough points, save the line
      if (pCount >= MIN_MATCH_SHAPE_POINTS)
      {
        for (k = 0; k < pCount; k++)
        {
          if (nearList[k] == j)
            jPos = k;
          index[k] = k;
        }
        tempDistList  = distList;
        tempNearList  = nearList;
        tempAngleList = nearAList;

        // we have to do this sort here, since the sort uses the angle of stars
        // relative to the orienting star to break ties.
        qsort(index, pCount, sizeof(DWORD), compareDists);

        // find out how many starting stars we can skip (min of position of
        // direction star and stars needed for a line match)
        for (k = 0; k < pCount; k++)
        {
          if (index[k] == jPos)
          {
            jPos = k;
            kGoal = k+1;
            break;
          }
        }

        if (MIN_MATCH_SHAPE_POINTS > kGoal) 
          kGoal = pCount-MIN_MATCH_SHAPE_POINTS+1;
        else
          kGoal = pCount-kGoal+2;

        for (k = 0; k < kGoal; k++)
        {
          idist = 1.0 / distList[nearList[index[pCount-k-1]]];
          prop  = 1.0 - ((idist > 1) ? 1.0:idist);
          tMatchTol = MATCH_TOL_MIN + RatioDif * prop;
          prop  = 1.0 - ((distList[nearList[index[k]]] > 1) ? 1.0:distList[nearList[index[k]]]);
          tMatchRad = RAD_TOL_MIN   + RadDif   * prop;
          for (p = 0; p < pCount; p++)
          {
            tempRats[p] = idist * distList[nearList[index[p]]];
          }
          pGoal = kGoal-k;
          for (p = 0; p < pGoal; p++)
          {
            curRatio = tempRats[p];
            
            // determine how many stars we can skip in the second phase
            // matching
            skippy = pCount-k-MIN_MATCH_SHAPE_POINTS;
            extraStart = p+1;

            temp = floor((curRatio-tMatchTol)*BIN_COUNT);
            if (temp < 0) 
              temp = 0;
            sI = (DWORD)temp;
            mStart = iBinStart[sI];

            temp = floor((curRatio+tMatchTol)*BIN_COUNT);
            if (temp > BIN_COUNT) 
              temp = BIN_COUNT;
            sI = (DWORD)temp;
            mGoal  = iBinStart[sI] + iBinLength[sI];

            // try to match lines in this bin
            for (m = mStart; m < mGoal; m++)
            {
              sLine    = iBinData[m];
              startNum = iStarPoint[sLine];
              lStart   = sLine*(MIN_SHAPE_POINTS-1);
              lEnd     = lStart+(MIN_SHAPE_POINTS-1);
              aStart   = sLine*MIN_SHAPE_POINTS;
              aEnd     = aStart+MIN_SHAPE_POINTS;

              difa1 = iAngleList[aStart]   - nearAList[index[pCount-k-1]];
              difa2 = iAngleList[aStart+1] - nearAList[index[p]];
              dif = curRatio - iCRatioList[lStart];
              if ((ABS(dif) <= tMatchTol) && ((ABS(difa1) <= tMatchRad) && (ABS(difa2) <= tMatchRad)))
              {
                aStart += 2;
                lStart++;
                tskip  = skippy;
                matchy = 2;
                // see how many ratios we can match -- allows a certain
                // number of ordered false failures dictated by 'skippy'
                iGood  = FALSE32;
                for (n = lStart, a = aStart, dn = extraStart; ((n < lEnd) && (dn < pCount-k)); dn++)
                {
                  // if we don't match, try to skip the ratio, otherwise
                  // count the match
                  dif   = tempRats[dn] - iCRatioList[n];
                  difa1 = nearAList[index[dn]] - iAngleList[a];
                  if ((ABS(dif) > tMatchTol) || (ABS(difa1) > tMatchRad))
                  {
                    if ((tskip > 0) && (n != jPos))
                    {
                      tskip--;
                    } else
                    {
                      break;
                    }
                  } else
                  {
                    matchy++;
                    n++;
                    a++;
                  }
                }
                // make sure we matched enough ratios
                if (matchy >= MIN_MATCH_SHAPE_POINTS)
                  iGood = TRUE32;

                // note match
                if (iGood == TRUE32)
                {
                  // add match to list
                  tsolve.pointsMatched = matchy;
                  tsolve.imStar        = i;
                  tsolve.starNum       = iStarPoint[sLine];
                  tsolve.imNearStar    = nearList[index[pCount-k-1]];
                  tsolve.nearStarNum   = iNearPoint[sLine];
                  tsolve.lineNum       = sLine;
                  tsolve.imThirdStar   = nearList[index[p]];
                  tsolve.thirdStarNum  = iThirdPoint[sLine];

                  // avoid duplicate matches
                  tenum->ReInitEnum();
                  while (tenum->GetEnumOK() == TRUE32)
                  {
                    bptr = (BYTE*)tenum->GetEnumNext();
                    if ((((StarShapeMatch*)bptr)->imStar == i) && 
                        (((StarShapeMatch*)bptr)->imNearStar == nearList[index[pCount-k-1]]))
                    {
                       iGood = FALSE32;
                    }
                  }
                  if (iGood == TRUE32)
                  {
                    if (SolveOneMatch(im, &tsolve) == TRUE32)
                    {
#ifdef STAR_SHAPE_QUICKSOLVE
                      // check for good match
                      DWORD err1, err2, err3;

                      err1 = (tsolve.starNum      == im->TrueIndex[tsolve.imStar]);
                      err2 = (tsolve.nearStarNum  == im->TrueIndex[tsolve.imNearStar]);
                      err3 = (tsolve.thirdStarNum == im->TrueIndex[tsolve.imThirdStar]);
                      if (((err1) && (err2)) && (err3))
                      {
                        solveBank->AddNewData(&tsolve);
                        iSolved = TRUE32;
                        printf("^");
                      } else
                      {
                        printf("*");
                      }
#else
                      printf("^");
                      solveBank->AddNewData(&tsolve);
#endif
                    } else
                    {
                      printf("!");
                    }
                  }
                }
              }
              if (iSolved == TRUE32)
                break;
            }
            if (iSolved == TRUE32)
              break;
          }
          if (iSolved == TRUE32)
            break;
        }
      }
      if (iSolved == TRUE32)
        break;
    }
    if (iSolved == TRUE32)
      break;
#ifdef STAR_SHAPE_VERBOSE
    printf(".");
    if (((i+1) % 60) == 0)
      printf(" %d/%d\n", i+1, starCount);
    fflush(stdout);
#endif
  }
#ifdef STAR_SHAPE_VERBOSE
  printf("\n");
  fflush(stdout);
#endif

  // copy solution info into an array
  *solveCount = solveBank->GetCount();
  if (*solveCount > 0)
  {
    *solveList  = new StarShapeMatch[*solveCount];
    tSolvePtr   = *solveList;
    tenum->ReInitEnum();
    i = 0;
    while (tenum->GetEnumOK() == TRUE32)
    {
      bptr = (BYTE*)tenum->GetEnumNext();
      memcpy(&tSolvePtr[i], bptr, sizeof(StarShapeMatch));
      i++;
    }
  } else
  {
    *solveCount = 0;
    *solveList  = NULL;
  }

  // cleanup
  delete tenum;
  delete solveBank;

  delete [] distList;
  delete [] imstars[0];
  delete [] imstars[1];
  delete [] angles;
  delete [] nearList;
  delete [] nearAList;
  delete [] index;
  delete [] tempRats;

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * GetMaxImAngle: Returns the currently set image angle used for indexing.
 * Assumes: <none>
 */
double StarShapeSys::GetMaxImAngle()
{
  return MAX_IM_ANGLE;
}

/* -------------------------------------------------- */

/*
 * GetMinShapePoints: Returns the currently set minimum points per shape.
 * Assumes: <none>
 */
DWORD StarShapeSys::GetMinShapePoints()
{
  return MIN_SHAPE_POINTS;
}

/* -------------------------------------------------- */

/*
 * GetWedgeAngle: Returns the currently set wedge angle.
 * Assumes: <none>
 */
double StarShapeSys::GetWedgeAngle()
{
  return ANGLE_WEDGE;
}

/* -------------------------------------------------- */

/*
 * FindShapes: Returns all the shapes centered on the star 'starNum' in the current index.
 * Assumes: <none>
 */
void StarShapeSys::FindShapes(DWORD starNum)
{
  DWORD i, j, goal;
  
  if (iInit == FALSE32)
  {
    return;
  }

  goal = MIN_SHAPE_POINTS-1;
  for (i = 0; i < iShapeCount; i++)
  {
    if (iStarPoint[i] == starNum)
    {
      printf("Shape %u\n", i);
      printf("Center: %u    Far: %u    Third: %u\n", starNum, iNearPoint[i], iThirdPoint[i]);
      printf("Ratios:\n");
      for (j = 0; j < goal; j++)
      {
        printf("%lg ", iCRatioList[i*goal+j]);
      }
      printf("\n");
      printf("Angles:\n");
      for (j = 0; j < MIN_SHAPE_POINTS; j++)
      {
        printf("%lg ", iAngleList[i*MIN_SHAPE_POINTS+j]);
      }
      printf("\n\n");
      fflush(stdout);
    }
  }
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
