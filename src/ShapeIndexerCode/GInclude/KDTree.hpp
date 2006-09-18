#ifndef __KDTREE__HPP__
#define __KDTREE__HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>
#include <BlockMemBank.hpp>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

struct KDNearList
{
	float *Data;
	float  Dist;
};

/* -------------------------------------------------- */

/* KDNode: node in the KDTree */
struct KDNode
{
	float   *Point;	// the data in the node
  void    *Data;
	DWORD    Pivot;	// the pivot property around which the children nodes are organized
	KDNode	*Left;	// pointer to the left child
	KDNode	*Right;	// pointer to the right child
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/* KDTree: a class that implements the KD-Tree data type. Organizes data in k-dimensional space
           allowing quicker determination of the nearest neighbours to a provided point.
		   The user should add in tree data using AddData. When all the data is added,
		   The tree can be built using BuildTree, after which searches for nearest neighbours
		   can be performed.
 */
class KDTree
{
protected:
	DWORD		Dimen;		 // number of dimensions in the input
  BOOL32 *Usage;
	KDNode *Root;		 // the root of the KDTree
	DWORD 	DataCnt; // the number of items included in the tree

  BlockMemBank dataBank;
  BlockMemBank nodeBank;

public:
	KDTree();
	~KDTree();

  void   Deinit();

	DWORD  GetCount();
  void   BuildTree(DWORD dimen, DWORD dataCnt, float *posList, void **dataList, DWORD blockSize);
  void   FindInDist(float *target, float max_dist, float (*distSqd)(float *e1, float *e2), void (*retFunc)(float *point, void *data));
  void   FindInRange(float *rangeMin, float *rangeMax, void (*retFunc)(float *point, void *data));
  void   FindKNearest(DWORD KNear, float *target, float (*distSqd)(float *e1, float *e2), void (*retFunc)(float *point, void *data));

protected:
	void   DoBuild(float *posList, void **dataList);
	void   NearestDist(float *target, float maxDist, BlockMemBank *outList, void (*distSqd)(float *e1, float *e2), void (*retFunc)(void *data));

  inline BOOL32 inRange(float *rangeMin, float *rangeMax, float *point);
  inline BOOL32 regIntersect(float *rangeMin, float *rangeMax, float *hr);
  inline BOOL32 regContained(float *rangeMin, float *rangeMax, float *hr);

  inline void   buildNearPoint(float *checkPt, float *hr, float *target);
  inline void   buildFarPoint(float *checkPt, float *hr, float *target);
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
