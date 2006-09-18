/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <memory.h>
#include <stdlib.h>
#include "StarKDTree.hpp"
#include <stdio.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#define KD_MIN    0
#define KD_MAX    1

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * StarKDTree: Initialize the tree to a base state.
 */
StarKDTree::StarKDTree()
{
  Nodes     = NULL;
	Root      = NULL;
  PosList   = NULL;
	DataCnt   = 0;
}

/* -------------------------------------------------- */

/*
 * Deinit: Cleanup and shutdown the tree.
 * Assumes: <none>
 */
void StarKDTree::Deinit()
{
  if (Nodes != NULL)
  {
    delete [] Nodes;
    Nodes = NULL;
  }
	Root = NULL;
  DataCnt = 0;
}

/* -------------------------------------------------- */

/*
 * ~StarKDTree: Shutdown.
 */
StarKDTree::~StarKDTree()
{
  Deinit();
}

/* -------------------------------------------------- */

/*
 * distSqdLess: Returns TRUE32 if the squared distance between PosList[:][p1] and 'target' <= maxdist.
 * Assumes: 'p1' references a valid position in 'PosList', target is a valid
 *          n-vector, 'maxdist' > 0.
 */
BOOL32 StarKDTree::distSqdLess(DWORD p1, double *target, double maxdist)
{
  double v1, sum;

#if (STAR_DIMEN == 3)
    v1   = PosList[0][p1] - target[0];
    sum  = v1*v1;
    if (sum > maxdist)
      return FALSE32;
    v1   = PosList[1][p1] - target[1];
    sum += v1*v1;
    if (sum > maxdist)
      return FALSE32;
    v1   = PosList[2][p1] - target[2];
    sum += v1*v1;
    if (sum > maxdist)
      return FALSE32;
#else
  DWORD i;

  v1  = PosList[0][p1] - target[0];
  sum = v1*v1;
  for (i = 1; i < STAR_DIMEN; i++)
  {
    v1 = PosList[i][p1] - target[i];
    sum += v1*v1;
    if (sum > maxdist)
      return FALSE32;
  }
#endif

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * distSqdRawLess: Returns TRUE32 if the squared distance between 'p1' and 'target' <= maxdist.
 * Assumes: 'p1' is a valid n-vector, target is a valid n-vector, 'maxdist' > 0.
 */
BOOL32 StarKDTree::distSqdRawLess(double *p1, double *target, double maxdist)
{
  double v1, sum;

#if (STAR_DIMEN == 3)
    v1   = p1[0] - target[0];
    sum  = v1*v1;
    if (sum > maxdist)
      return FALSE32;
    v1   = p1[1] - target[1];
    sum += v1*v1;
    if (sum > maxdist)
      return FALSE32;
    v1   = p1[2] - target[2];
    sum += v1*v1;
    if (sum > maxdist)
      return FALSE32;
#else
  DWORD i;

  v1  = p1[0] - target[0];
  sum = v1*v1;
  for (i = 1; i < STAR_DIMEN; i++)
  {
    v1 = p1[i] - target[i];
    sum += v1*v1;
    if (sum > maxdist)
      return FALSE32;
  }
#endif

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * GetCount: Returns the numberof elements in the kd-tree.
 * Assumes: <none>
 */
DWORD StarKDTree::GetCount()
{
	return DataCnt;
}

/* -------------------------------------------------- */

/*
 * BuildTree: Builds a kd-tree based on 'dataCnt' elements in 'posList'.
 * Assumes: 'posList' persists for the life of the tree. 'dataCnt' <= |posList[:]|.
 *          'dataCnt' > 0.
 */
void StarKDTree::BuildTree(DWORD dataCnt, double **posList)
{
  Deinit();

  assert(dataCnt > 0);

  // save the position list
  PosList = posList;
  // save the data count
  DataCnt = dataCnt;
  // build all the nodes we'll need
  Nodes   = new StarKDNode[DataCnt];

	// build the tree
	DoBuild();
}

/* -------------------------------------------------- */

/*
 * DoBuild: Performs the actual kd-tree building.
 * Assumes: 'PosList', 'dataCnt', and 'Nodes' are properly initialized.
 */
void StarKDTree::DoBuild()
{
	StarKDNode *dataNode, **dest;
	double range[STAR_DIMEN*2], maxVal;
	DWORD i, choice;
  DWORD minPt, maxPt;
	double dist, midpt, tpt;
  BlockMemBank recBank;
  BYTE tspace[sizeof(StarKDNode**)+sizeof(DWORD)*2+sizeof(DWORD*)*2];
  BYTE *tptr;
  DWORD *index1, *index2;
  DWORD min, max, best, p1, p2, j;
  DWORD curNode;

  // curNode indicates which node in 'Nodes' is next available for use.
  curNode = 0;

  // build the indices of elements
  index1 = new DWORD[DataCnt];
  index2 = new DWORD[DataCnt];
  for (i = 0; i < DataCnt; i++)
  {
    index1[i] = i;
  }

  // Rather than using the stack and recursively calling the build procedure
  // we use a BlockMemBank, which operates nicely as a stack,
  // and push/pop all the parameters we need using it instead. This avoids nasty
  // problems like stack overflow, overhead associated with reallocating local
  // parameters on each recursive call. While this is more pleasant from one perspective,
  // it is also much less readable since it involves jumping back and forth in blocks
  // of bytes to set or get parameters.

  // init the recursive call stack
  recBank.Init(sizeof(StarKDNode**)+sizeof(DWORD)*2+sizeof(DWORD*)*2, 200);

  // place the initial build call on the stack
  tptr = tspace;
  *((StarKDNode***)tptr) = &Root; // the node to fill with any tree
  tptr += sizeof(StarKDNode**);
  *((DWORD*)tptr) = 0;            // the starting position of useful elements in 'index1'
  tptr += sizeof(DWORD);
  *((DWORD*)tptr) = DataCnt-1;    // the ending position of useful elements in 'index1'
  tptr += sizeof(DWORD);
  *((DWORD**)tptr) = index1;      // index1, which contains useful elements
  tptr += sizeof(DWORD*);
  *((DWORD**)tptr) = index2;      // index2, which will be used to hold useful elements later
  recBank.AddNewData(tspace);

  // keep working until all elements on the stack are used up
  while (recBank.GetCount() != 0)
  {
    // pull all the parameters off the stack
    tptr   = recBank.GetLastData();
    dest   = *((StarKDNode***)tptr);
    tptr  += sizeof(StarKDNode**);
    min    = *((DWORD*)tptr);
    tptr  += sizeof(DWORD);
    max    = *((DWORD*)tptr);
    tptr  += sizeof(DWORD);
    index1 = *((DWORD**)tptr);
    tptr  += sizeof(DWORD*);
    index2 = *((DWORD**)tptr);

    // delete stack data
    recBank.RemoveData(recBank.GetLastData());

	  // if there is only one element sent, simply create a leaf
	  if (min == max)
	  {
      // place single node
      dataNode = *dest = &Nodes[curNode++];
      dataNode->Pivot  = 0;
      dataNode->Point  = index1[min];
      dataNode->Left   = dataNode->Right = NULL;
		  continue;
    }
    // no elements sent, so just continue
    if (min > max)
    {
      continue;
    }

    // init range data
	  for (i = 0, minPt = KD_MIN, maxPt = KD_MAX; i < STAR_DIMEN; i++, minPt+=2, maxPt+=2)
	  {
		  range[minPt] =  MAX_DOUBLE;
		  range[maxPt] = -MIN_DOUBLE;
	  }

	  // find ranges for each dimension -- i.e. find the range of values that the 
    // data takes on in each dimension.
    for (i = 0, minPt = KD_MIN, maxPt = KD_MAX; i < STAR_DIMEN; i++, minPt+=2, maxPt+=2)
    {
	    for (j = min; j <= max; j++)
	    {
        if (PosList[i][index1[j]] < range[minPt])
          range[minPt] = PosList[i][index1[j]];
		    if (PosList[i][index1[j]] > range[maxPt])
			    range[maxPt] = PosList[i][index1[j]];
	    }
    }

	  // find the dimension with the maximum range -- this will be our pivot dimension
	  maxVal = -1;
	  for (i = 0, minPt = KD_MIN, maxPt = KD_MAX; i < STAR_DIMEN; i++, minPt+=2, maxPt+=2)
	  {
		  if (maxVal < (range[maxPt] - range[minPt]))
		  {
			  choice = i;
			  maxVal = range[maxPt] - range[minPt];
		  }
	  }

	  // find the midpoint of the pivot dimension
	  midpt = (double)(range[choice*2+KD_MAX] + range[choice*2+KD_MIN])*0.5f;
	  
	  // find element closest to the midpoint of the pivot
	  dist = MAX_DOUBLE;
    best = min;
	  for (j = min; j <= max; j++)
    {
      tpt = PosList[choice][index1[j]] - midpt;
		  tpt = ABS(tpt);
	    if (tpt <= dist)
		  {
		    best = j;
		    dist = tpt;
      }
    }

	  // put the nearest midpoint node into the destination node
		dataNode = *dest = &Nodes[curNode++];
    dataNode->Pivot  = choice;
    dataNode->Point  = index1[best];
    dataNode->Left   = dataNode->Right = NULL;
	  
    // Split the data into their respective sides
    // Note that we place the data into index2 so as to not overwrite
    // values in index1. When we set the recursive call we swap index1 and index2
    // in the parameter list so that the appropriate array of elements is used.
    dist = PosList[choice][index1[best]];
    p1   = min;
    p2   = max;
    // place all elements, ignoring the midpoint element
    for (j = min; j < best; j++)
    {
      if (PosList[choice][index1[j]] <= dist)
      {
        index2[p1++] = index1[j];
      } else
      {
        index2[p2--] = index1[j];
      }
    }
    for (j = best+1; j <= max; j++)
    {
      if (PosList[choice][index1[j]] <= dist)
      {
        index2[p1++] = index1[j];
      } else
      {
        index2[p2--] = index1[j];
      }
    }

    // push the left and right node data into the mem bank -- 
    // this works because the memory bank can be safely treated like
    // a stack, where the last position in the bank is the top 
    // of the stack.

    // left side
    if (p1 > min)
    {
      tptr = tspace;
      *((StarKDNode***)tptr) = &(dataNode->Left);
      tptr += sizeof(StarKDNode**);
      *((DWORD*)tptr) = min;
      tptr += sizeof(DWORD);
      *((DWORD*)tptr) = p1-1;
      tptr += sizeof(DWORD);
      *((DWORD**)tptr) = index2;
      tptr += sizeof(DWORD*);
      *((DWORD**)tptr) = index1;
      recBank.AddNewData(tspace);
    }
    // right side
    if (p2 < max)
    {
      tptr = tspace;
      *((StarKDNode***)tptr) = &(dataNode->Right);
      tptr += sizeof(StarKDNode**);
      *((DWORD*)tptr) = p2+1;
      tptr += sizeof(DWORD);
      *((DWORD*)tptr) = max;
      tptr += sizeof(DWORD);
      *((DWORD**)tptr) = index2;
      tptr += sizeof(DWORD*);
      *((DWORD**)tptr) = index1;
      recBank.AddNewData(tspace);
    }
  }

  // cleanup
  delete [] index1;
  delete [] index2;
}

/* -------------------------------------------------- */

/*
 * inRange: Returns TRUE32 if PosList[:][point] is in the range defined
 *          by 'minRange' and 'maxRange'.
 * Assumes: 'point' <= |PosList[:]|, 'rangeMin' and 'rangeMax' and valid
 *          arrays for ranges.
 */
BOOL32 StarKDTree::inRange(double *rangeMin, double *rangeMax, DWORD point)
{
#if (STAR_DIMEN == 3)
    if ((PosList[0][point] < rangeMin[0]) || (PosList[0][point] > rangeMax[0]))
      return FALSE32;
    if ((PosList[1][point] < rangeMin[1]) || (PosList[1][point] > rangeMax[1]))
      return FALSE32;
    if ((PosList[2][point] < rangeMin[2]) || (PosList[2][point] > rangeMax[2]))
      return FALSE32;
#else
  DWORD i;

  for (i = 0; i < STAR_DIMEN; i++)
  {
    if ((PosList[i][point] < rangeMin[i]) || (PosList[i][point] > rangeMax[i]))
      return FALSE32;
  }
#endif

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * regIntersect: Returns TRUE32 if the region defined by 'hr' intersects the region defined
 *               by 'minRange' and 'maxRange'.
 * Assumes: 'hr', 'rangeMin' and 'rangeMax' and valid
 *          arrays for ranges.
 */
BOOL32 StarKDTree::regIntersect(double *rangeMin, double *rangeMax, double *hr)
{
  DWORD i;
  double *val;

  val = hr;
  for (i = 0; i < STAR_DIMEN; i++)
  {
    if (*val > rangeMax[i])
      return FALSE32;
    val++;
    if (*val < rangeMin[i])
      return FALSE32;
    val++;
  }

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * buildNearPoint: Builds and places in 'checkPt', the nearest point in the region 'hr'
 *                 relative to the point 'target'.
 * Assumes: 'hr', is a valid arrays for a range, 'checkPt' can hold a compelte point,
 *          'target' points to a valid point.
 */
void StarKDTree::buildNearPoint(double *checkPt, double *hr, double *target)
{
  DWORD i, minPt, maxPt;

	// check all the dimensions in the hr for possible points
	for (i = 0, minPt = KD_MIN, maxPt = KD_MAX; i < STAR_DIMEN; i++, minPt+=2, maxPt+=2)
	{
		// find the closest point to the target

    // if target is to the 'left' in this dimension, set the near point
    // to the 'leftmost' point in the region/
		if (target[i] <= hr[minPt])
		{
			checkPt[i] = hr[minPt];
		} else if (target[i] >= hr[maxPt])
		{ // if target is to the 'right' in this dimension, set the near point
      // to the 'rightmost' point in the region/
			checkPt[i] = hr[maxPt];
		} else
    { // otherwise target is inside the region in this dimension, so just use the
      // target value.
			checkPt[i] = target[i];
		}
	}
}

/* -------------------------------------------------- */

/*
 * buildFarPoint: Builds and places in 'checkPt', the farthest point in the region 'hr'
 *                 relative to the point 'target'.
 * Assumes: 'hr', is a valid arrays for a range, 'checkPt' can hold a compelte point,
 *          'target' points to a valid point.
 */
void StarKDTree::buildFarPoint(double *checkPt, double *hr, double *target)
{
  DWORD i, minPt, maxPt;
  double d1, d2;

	// check all the dimensions in the hr for possible points
	for (i = 0, minPt = KD_MIN, maxPt = KD_MAX; i < STAR_DIMEN; i++, minPt+=2, maxPt+=2)
	{
    // determine which end is farthest for the current dimension,
    // and choose that value for the farthest point.
    d1 = target[i] - hr[minPt];
    d2 = target[i] - hr[maxPt];   
    if (ABS(d1) > ABS(d2))
    {
			checkPt[i] = hr[minPt];
		} else
		{
			checkPt[i] = hr[maxPt];
		}
	}
}

/* -------------------------------------------------- */

/*
 * regContained: Returns TRUE32 if 'hr' is completely within the region defined by
 *               'rangeMin' and 'rangeMax', returns FALSE32 otherwise.
 * Assumes: 'hr', 'rangeMin', and 'rangeMax' are valid arrays for a range.
 */
BOOL32 StarKDTree::regContained(double *rangeMin, double *rangeMax, double *hr)
{
  DWORD i;
  double *val;

  val = hr;
  for (i = 0; i < STAR_DIMEN; i++)
  {
    if (*val < rangeMin[i])
      return FALSE32;
    val++;
    if (*val > rangeMax[i])
      return FALSE32;
    val++;
  }

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * FindInDist: Finds all the points in the tree within 'maxdist' of 'target' and
 *             returns their order in 'PosList' via 'retBank'.
 * Assumes: The tree is built, 'target' is a valid point, 'retBank' is a valid and
 *          initializes mem bank (initialized to accept DWORD's).
 */
void StarKDTree::FindInDist(double *target, double maxdist, BlockMemBank *retBank)
{
  StarKDNode *kd;
  double hr[STAR_DIMEN*2];
  DWORD i, pivot;
  BlockMemBank recBank;
  BOOL32 report;
  BYTE *tptr;
  BYTE tspace[sizeof(StarKDNode*)+sizeof(double)*2*STAR_DIMEN+sizeof(BOOL32)];
  double holdval;
  double tempPt[STAR_DIMEN];

  maxdist = maxdist*maxdist;

  // build the complete region of space we are examining. As we traverse
  // from node to node we can adjust this region and use it to determine
  // whether a solution point could possibly exist in the left of right
  // side of the node we are currently examining
  for (i = 0; i < STAR_DIMEN; i++)
  {
    hr[i*2]   = -MAX_DOUBLE;
    hr[i*2+1] =  MAX_DOUBLE;
  }

  // As in the building phase we use a memory bank as the call stack to save allocating
  // lots of extra local parameters.
  recBank.Init(sizeof(StarKDNode*)+sizeof(double)*2*STAR_DIMEN+sizeof(BOOL32), 200);

  // build the first call
  tptr = tspace;
  *((StarKDNode**)tptr) = Root;                 // current node
  tptr += sizeof(StarKDNode*);
  memcpy(tptr, hr, sizeof(double)*2*STAR_DIMEN);// current region being examined
  tptr += sizeof(double)*2*STAR_DIMEN;
  *((BOOL32*)tptr) = FALSE32;                   // indicates whether we should just report all
                                                // nodes at or below the current node, or whether
                                                // we should actually check the node
  recBank.AddNewData(tspace);

  // keep working until the stack is empty
  while (recBank.GetCount() > 0)
  {
    // grab data off the stack
    tptr = recBank.GetLastData();
    kd = *((StarKDNode**)tptr);
    tptr += sizeof(StarKDNode*);
    // skip the region for now -- we may not need it
    tptr += sizeof(double)*2*STAR_DIMEN;
    report = *((BOOL32*)tptr);
    recBank.RemoveData(recBank.GetLastData());

    if (report == TRUE32) // if we're reporting the subtree, just report it all
    {
      // add the node to the return bank
      retBank->AddNewData((void*)&kd->Point);
      // recurse the left branch
      if (kd->Left != NULL)
      {
        tptr = tspace;
        *((StarKDNode**)tptr) = kd->Left;
        tptr += sizeof(StarKDNode*);
        tptr += sizeof(double)*2*STAR_DIMEN;
        *((BOOL32*)tptr) = TRUE32;
        recBank.AddNewData(tspace);
      }
      // recurse the right branch
      if (kd->Right != NULL)
      {
        tptr = tspace;
        *((StarKDNode**)tptr) = kd->Right;
        tptr += sizeof(StarKDNode*);
        tptr += sizeof(double)*2*STAR_DIMEN;
        *((BOOL32*)tptr) = TRUE32;
        recBank.AddNewData(tspace);
      }
    } else
    {
      // see if we should report this node
      if (distSqdLess(kd->Point, target, maxdist) == TRUE32)
      {
        retBank->AddNewData((void*)&kd->Point);
      }

      // now we need the region info, so go back to the stack and get it
      tptr -= sizeof(double)*2*STAR_DIMEN;
      memcpy(hr, tptr, sizeof(double)*2*STAR_DIMEN);
      pivot = kd->Pivot;

      // recurse the left side
      if (kd->Left != NULL)
      {
        // adjust the region for the left branch
        holdval = hr[pivot*2+1];
        hr[pivot*2+1] = PosList[pivot][kd->Point];
        buildFarPoint(tempPt, hr, target);
        if (distSqdRawLess(tempPt, target, maxdist) == TRUE32)
        { // if subtree is completely contained, report it all
          tptr = tspace;
          *((StarKDNode**)tptr) = kd->Left;
          tptr += sizeof(StarKDNode*);
          tptr += sizeof(double)*2*STAR_DIMEN;
          *((BOOL32*)tptr) = TRUE32;
          recBank.AddNewData(tspace);
        } else
        {
          buildNearPoint(tempPt, hr, target);
          if (distSqdRawLess(tempPt, target, maxdist) == TRUE32)
          { // if partially contained, keep looking downwards
            tptr = tspace;
            *((StarKDNode**)tptr) = kd->Left;
            tptr += sizeof(StarKDNode*);
            memcpy(tptr, hr, sizeof(double)*2*STAR_DIMEN);
            tptr += sizeof(double)*2*STAR_DIMEN;
            *((BOOL32*)tptr) = FALSE32;
            recBank.AddNewData(tspace);
          }
        }
        // restore region
        hr[pivot*2+1] = holdval;
      }

      // recurse the right side
      if (kd->Right != NULL)
      {
        // adjust the region for the right branch
        holdval = hr[pivot*2];
        hr[pivot*2] = PosList[pivot][kd->Point];
        buildFarPoint(tempPt, hr, target);
        if (distSqdRawLess(tempPt, target, maxdist) == TRUE32)
        { // if subtree is completely contained, report it all
          tptr = tspace;
          *((StarKDNode**)tptr) = kd->Right;
          tptr += sizeof(StarKDNode*);
          tptr += sizeof(double)*2*STAR_DIMEN;
          *((BOOL32*)tptr) = TRUE32;
          recBank.AddNewData(tspace);
        } else
        {
          buildNearPoint(tempPt, hr, target);
          if (distSqdRawLess(tempPt, target, maxdist) == TRUE32)
          { // if partially contained, keep looking downwards
            tptr = tspace;
            *((StarKDNode**)tptr) = kd->Right;
            tptr += sizeof(StarKDNode*);
            memcpy(tptr, hr, sizeof(double)*2*STAR_DIMEN);
            tptr += sizeof(double)*2*STAR_DIMEN;
            *((BOOL32*)tptr) = FALSE32;
            recBank.AddNewData(tspace);
          }
        }
        // restore region
        hr[pivot*2] = holdval;
      }
    }
  }
}

/* -------------------------------------------------- */

/*
 * FindInRange: Finds all the points in the tree within the range defined by 'minRange' and
 *              'maxRange', and returns their order in 'PosList' via 'retBank'.
 * Assumes: The tree is built, 'maxRange' and 'maxRange' are valid ranges, 'retBank' is a valid and
 *          initializes mem bank (initialized to accept DWORD's).
 */
void StarKDTree::FindInRange(double *rangeMin, double *rangeMax, BlockMemBank *retBank)
{
  StarKDNode *kd;
  double hr[STAR_DIMEN*2];
  DWORD i, pivot;
  BlockMemBank recBank;
  BOOL32 report;
  BYTE *tptr;
  BYTE tspace[sizeof(StarKDNode*)+sizeof(double)*2*STAR_DIMEN+sizeof(BOOL32)];
  double holdval;

  // create the intialize search region
  for (i = 0; i < STAR_DIMEN; i++)
  {
    hr[i*2]   = -MAX_DOUBLE;
    hr[i*2+1] =  MAX_DOUBLE;
  }

  // As usual, use a memory bank as the call stack.
  recBank.Init(sizeof(StarKDNode*)+sizeof(double)*2*STAR_DIMEN+sizeof(BOOL32), 200);

  // buidl the first call
  tptr = tspace;
  *((StarKDNode**)tptr) = Root;                 // current node
  tptr += sizeof(StarKDNode*);
  memcpy(tptr, hr, sizeof(double)*2*STAR_DIMEN);// regino region being examined
  tptr += sizeof(double)*2*STAR_DIMEN;
  *((BOOL32*)tptr) = FALSE32;                   // indicates whether we are simply reporting the
                                                // whole subtree.
  recBank.AddNewData(tspace);

  // keep searching the stack until all calls are processed
  while (recBank.GetCount() > 0)
  {
    // grab data off the stack
    tptr = recBank.GetLastData();
    kd = *((StarKDNode**)tptr);
    tptr += sizeof(StarKDNode*);
    // ignore the region unless we really need it
    tptr += sizeof(double)*2*STAR_DIMEN;
    report = *((BOOL32*)tptr);
    recBank.RemoveData(recBank.GetLastData());

    if (report == TRUE32)
    {// if we're reporting the subtree, just report it all
      retBank->AddNewData((void*)&kd->Point);
      if (kd->Left != NULL)
      {
        tptr = tspace;
        *((StarKDNode**)tptr) = kd->Left;
        tptr += sizeof(StarKDNode*);
        tptr += sizeof(double)*2*STAR_DIMEN;
        *((BOOL32*)tptr) = TRUE32;
        recBank.AddNewData(tspace);
      }
      if (kd->Right != NULL)
      {
        tptr = tspace;
        *((StarKDNode**)tptr) = kd->Right;
        tptr += sizeof(StarKDNode*);
        tptr += sizeof(double)*2*STAR_DIMEN;
        *((BOOL32*)tptr) = TRUE32;
        recBank.AddNewData(tspace);
      }
    } else
    {
      // see if we should report this node
      if (inRange(rangeMin, rangeMax, kd->Point) == TRUE32)
      {
        retBank->AddNewData((void*)&kd->Point);
      }

      // go back and grab the region
      tptr -= sizeof(double)*2*STAR_DIMEN;
      memcpy(hr, tptr, sizeof(double)*2*STAR_DIMEN);
      pivot = kd->Pivot;

      // check the left side
      if (kd->Left != NULL)
      {
        holdval = hr[pivot*2+1];
        hr[pivot*2+1] = PosList[pivot][kd->Point];
        if (regContained(rangeMin, rangeMax, hr) == TRUE32)
        { // if subtree is completely contained, report it all
          tptr = tspace;
          *((StarKDNode**)tptr) = kd->Left;
          tptr += sizeof(StarKDNode*);
          tptr += sizeof(double)*2*STAR_DIMEN;
          *((BOOL32*)tptr) = TRUE32;
          recBank.AddNewData(tspace);
        } else if (regIntersect(rangeMin, rangeMax, hr) == TRUE32)
        { // if partially contained, keep looking downwards
          tptr = tspace;
          *((StarKDNode**)tptr) = kd->Left;
          tptr += sizeof(StarKDNode*);
          memcpy(tptr, hr, sizeof(double)*2*STAR_DIMEN);
          tptr += sizeof(double)*2*STAR_DIMEN;
          *((BOOL32*)tptr) = FALSE32;
          recBank.AddNewData(tspace);
        }
        hr[pivot*2+1] = holdval;
      }

      // check the right side
      if (kd->Right != NULL)
      {
        holdval = hr[pivot*2];
        hr[pivot*2] = PosList[pivot][kd->Point];
        if (regContained(rangeMin, rangeMax, hr) == TRUE32)
        { // if subtree is completely contained, report it all
          tptr = tspace;
          *((StarKDNode**)tptr) = kd->Right;
          tptr += sizeof(StarKDNode*);
          tptr += sizeof(double)*2*STAR_DIMEN;
          *((BOOL32*)tptr) = TRUE32;
          recBank.AddNewData(tspace);
        } else if (regIntersect(rangeMin, rangeMax, hr) == TRUE32)
        { // if partially contained, keep looking downwards
          tptr = tspace;
          *((StarKDNode**)tptr) = kd->Right;
          tptr += sizeof(StarKDNode*);
          memcpy(tptr, hr, sizeof(double)*2*STAR_DIMEN);
          tptr += sizeof(double)*2*STAR_DIMEN;
          *((BOOL32*)tptr) = FALSE32;
          recBank.AddNewData(tspace);
        }
        hr[pivot*2] = holdval;
      }
    }
  }
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
