#ifndef __STARKDTREE__HPP__
#define __STARKDTREE__HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>
#include <BlockMemBank.hpp>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// dimension of the data used in the kd-tree
#define STAR_DIMEN    3

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/* 
 * StarKDNode: node in the KD-Tree
 */
struct StarKDNode
{
  DWORD        Point; // The number of the point in the class' PosList.
	DWORD        Pivot;	// The pivot dimension around which the children nodes are organized.
	StarKDNode	*Left;	// Pointer to the left child
	StarKDNode	*Right;	// Pointer to the right child
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/* 
 * StarKDTree: A structure for speeding up the indexing process. Constructs a tree representing all the
 *             points in the catalog, and can then be used to search for points in the range, or point
 *             within a pre-specified (euclidian) distance of another point. The implementation listed here is strictly
 *             for 3D points. Changing the define 'STAR_DIMEN' will adjust the dimension of the data used in the 
 *             tree.
 */
class StarKDTree
{
protected:
	StarKDNode *Root;		   // The root of the KDTree
	DWORD 	    DataCnt;   // The number of items included in the tree
  double    **PosList;   // Pointer to the point data. The data is x-by-y where 'x' is the dimension of the
                         // data, and 'y' is equal to 'DataCnt'.

  StarKDNode *Nodes;     // The nodes used in the tree. Since the tree is built using the complete
                         // dataset, the nodes can be allocated in a single block.

public:
	StarKDTree();
	~StarKDTree();

  // Shutdown the tree
  void   Deinit();

  // Return the number of elements in the tree.
	DWORD  GetCount();
  // Build a tree using 'dataCnt' elements of dimension STAR_DIMEN. The points are provided
  // by 'posList''.
  void   BuildTree(DWORD dataCnt, double **posList);
  // Find all the points that fall within a distance 'max_dist' from the point 'target'. Solutions
  // are provided by order in PosList' in the memory bank 'retBank'.
  void   FindInDist(double *target, double max_dist, BlockMemBank *retBank);
  // Find all the pointer that fall within the ranges 'rangeMin' and 'rangeMax', where each 
  // element of the two arrays carresponds to the minimum and maximum range value for a particular 
  // dimension. Solutions are provided by order in 'PosList' in the memory bank 'retBank'.
  void   FindInRange(double *rangeMin, double *rangeMax, BlockMemBank *retBank);

protected:
	void   DoBuild();

  inline BOOL32 inRange(double *rangeMin, double *rangeMax, DWORD point);
  inline BOOL32 regIntersect(double *rangeMin, double *rangeMax, double *hr);
  inline BOOL32 regContained(double *rangeMin, double *rangeMax, double *hr);

  inline void   buildNearPoint(double *checkPt, double *hr, double *target);
  inline void   buildFarPoint(double *checkPt, double *hr, double *target);

  inline BOOL32 distSqdLess(DWORD p1, double *target, double maxdist);
  inline BOOL32 distSqdRawLess(double *p1, double *target, double maxdist);

};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
