#ifndef __STARSHAPESYS_HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>
#include "StarList.hpp"
#include "StarImage.hpp"
#include "StarCamera.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// Output information during indexing/solving to console
#define STAR_SHAPE_VERBOSE
// On solving, when the correct solution is found return immediate (don't check
// *all* possible shapes
#define STAR_SHAPE_QUICKSOLVE
// Output information on shape counts
//#define STAR_SHAPE_DEBUG
// use kd-tree during indexing phase
#define STAR_SHAPE_KDTREE

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * StarShapeBuildParams: Parameters used for building indexes.
 */
struct StarShapeBuildParams
{
  double MaxImAngle;            // image view angle
  char   OutFile[1024];         // output file name
  double AngleWedge;            // wedge angle
  DWORD  MinShapePoints;        // points per shape
  DWORD  BinCount;              // bins to use in the index
};

/* -------------------------------------------------- */

/*
 *  StarShapeMatchParams: Parameters used for matching images to indexes.
 */
struct StarShapeMatchParams
{
  double DistMatchTolMin;   // minimum tolerance on distances
  double DistMatchTolMax;   // maximum tolerance on distances
  double AngMatchTolMin;    // minimum tolerance on angles
  double AngMatchTolMax;    // maximum tolerance on angles
};

/* -------------------------------------------------- */

/*
 * StarLineMatch: Resulting potential image solution.
 */
struct StarShapeMatch
{
  DWORD  pointsMatched;   // Number of points matched
  DWORD  imStar;          // Central star id in image
  DWORD  starNum;         // Central star id in catalog
  DWORD  imNearStar;      // Distance measuring star id in image
  DWORD  nearStarNum;     // Distance measuring star id in catalog
  DWORD  imThirdStar;     // Third star id in image
  DWORD  thirdStarNum;    // Third star id in catalog
  DWORD  lineNum;         // ID of shape in index
  double imI[3];          // Solved image basis
  double imJ[3];
  double imK[3];
  double viewAngle;       // Solved view angle
};

/* -------------------------------------------------- */

struct StarExpShapeCount
{
  DWORD  density;
  DWORD *indOrder;
};

/* -------------------------------------------------- */

/*
 * StarShapeSys: Class for building shape indexes and for solving images.
 */
class StarShapeSys
{
protected:
  BOOL32 iInit;   // Is index initialized for solving
  BOOL32 iSInit;  // Is star catalog initialized for building/solving
  BOOL32 iParam;  // Are build parameters set

  double MAX_IM_ANGLE;          // image view angle used
  double COS_CUTOFF;            // cos of view angle
  double ANGLE_WEDGE;           // wedge angle used
  double WEDGE_CUTOFF;          // wedge cutoff value
  DWORD  MIN_SHAPE_POINTS;      // points per shape
  DWORD  MIN_MATCH_SHAPE_POINTS;// minimum points per shape for matching
  DWORD  BIN_COUNT;             // number of bins
  double MATCH_TOL_MIN;         // minimum distance tolerance
  double MATCH_TOL_MAX;         // maximum distance tolerance
  double RAD_TOL_MIN;           // minimum angle tolerance
  double RAD_TOL_MAX;           // maximum angle tolerance
  double MAX_BUILD_DIST;        // 3D 'near' distance for buliding index
  DWORD  MAX_SHAPES;            // maximum shapes for building

  DWORD   iShapeCount;           // shapes in index
  DWORD  *iStarPoint;           // list of central star id's
  DWORD  *iNearPoint;           // list of distance measuring star id's
  DWORD  *iThirdPoint;          // list of third star id's
  double *iCRatioList;          // list of shape ratios
  double *iAngleList;           // list of shape angles
  DWORD  *iBinStart;            // starting offset of each bin (in iBinData)
  WORD   *iBinLength;           // length of each bin (in iBinData)
  DWORD  *iBinData;             // list of shape ID's

  StarShapeBuildParams *BuildParams;     // list of build parameters
  DWORD                 BuildParamCount; // number of different build parameters

  StarList *sList;                       // build/solve catalog
 
public:
  StarShapeSys();
  ~StarShapeSys();

  // generate a 2D image from the provided catalog
  void   StarsGenIm(StarImage *im, StarList *sList, StarList *dList, 
                    StarCamera *cam, double jitter, double drop_out);
  // set catalog used for building/solving
  void   SetStarList(StarList *sList);
  // setup build parameters
  void   InitBuildParams(char *outfileBase, double MaxImAngle, DWORD MaxShapes, DWORD BinCount, 
                         DWORD wedgeCounts, double *wedgeList, DWORD shapeStars, DWORD *starsPShapeList);
  // setup match parameters
  void   InitMatchParams(StarShapeMatchParams *params);
  // build index(es)
  BOOL32 BuildIndex();
  // open an index for use in solving
  BOOL32 OpenIndex(char *fname);
  // save index ratios to text file
  BOOL32 SaveRatiosAsText(char *fname);
  // save angles to text file
  BOOL32 SaveAnglesAsText(char *fname);
  // try to solve an image
  BOOL32 SolveImage(StarImage *im, DWORD *solveCount, StarShapeMatch **solveList);
  // estimate number of stars per image according to supplied build params
  double EstimateStarsPerIm(StarShapeBuildParams *params);
  // estimate total shapes according to supplied build params
  double EstimateShapes(StarShapeBuildParams *params, double *vlambda = NULL);
  // estimate required space in bytes according to supplied build params
  double EstimateSpace(StarShapeBuildParams *params);
  // return image angle originally used for index buliding
  double GetMaxImAngle();
  // return stars per shape
  DWORD  GetMinShapePoints();
  // return wedge angle
  double GetWedgeAngle();
  // write the density of stars around every catalog star to file
  BOOL32 WriteDensity(char *fname);
  // prints all shapes associated with the provided 'starNum' in the index
  void FindShapes(DWORD starNum);

protected:
  void   Deinit();
  void   grabIndexDat(DWORD *candidates, double **imstars, double *angles, double *distList,
                      DWORD *cCount, StarCamera *cam, double *dir, DWORD j);
  double GetStarsAngle(double ra1, double dec1, double ra2, double dec2);
  BOOL32 SolveOneMatch(StarImage *im, StarShapeMatch *match);
  BOOL32 SaveIndex(StarShapeBuildParams *params);
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
