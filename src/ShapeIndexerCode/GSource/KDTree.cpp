/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <memory.h>
#include <stdlib.h>
#include <KDTree.hpp>
#include <PriorityQueue.hpp>
#include <stdio.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#define KD_MIN    0
#define KD_MAX    1

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/* KDTree: Initialized the values in the KD-Tree object
 * Takes: 'dimen' as the number of dimensions in the data
 *        'rules' as a list of rules to use when comparing and searching through
 *                data in the KD-Tree
 * Pre: 'dimen' > 0
 *      'rules' points to a valid list of rules of length dimen. Not all of the elements
 *              of 'rules' are set to ignore
 * Returns: <Nothing>
 */
KDTree::KDTree()
{
	Dimen     = 0;

	Root      = NULL;
	DataCnt   = 0;
}

/* -------------------------------------------------- */

void KDTree::Deinit()
{
	// decide how to remove the tree data
  dataBank.Deinit();
  nodeBank.Deinit();

	Dimen     = 0;

	Root      = NULL;
}

/* -------------------------------------------------- */

/* ~KDTree: De-initialized the KD-Tree objects
 * Takes: <Nothing>
 * Pre: <Nothing<
 * Returns: <Nothing>
 */
KDTree::~KDTree()
{
  Deinit();
}

/* -------------------------------------------------- */

/* GetItemCount: Returns the number of elements in the tree
 * Takes: <Nothing>
 * Pre: <Nothing>
 * Returns: an int representing the number of elements in the tree
 */
DWORD KDTree::GetCount()
{
	return DataCnt;
}

/* -------------------------------------------------- */

float *tempList;
DWORD  tempDimen;
DWORD  tempStart;

int cmpElems(void const *e1, void const *e2)
{
  float *d1, *d2;
  DWORD i;

  d1 = &tempList[*((DWORD*)e1)*tempDimen];
  d2 = &tempList[*((DWORD*)e2)*tempDimen];

  if (d1[tempStart] < d2[tempStart])
    return -1;
  else if (d1[tempStart] > d2[tempStart])
    return 1;

  for (i = 0; i < tempStart; i++)
  {
    if (d1[i] < d2[i])
      return -1;
    else if (d1[i] > d2[i])
      return 1;
  }
  for (i = tempStart+1; i < tempDimen; i++)
  {
    if (d1[i] < d2[i])
      return -1;
    else if (d1[i] > d2[i])
      return 1;
  }

  return 0;
}

/* -------------------------------------------------- */

/* BuildTree: Builds the KD-Tree based on the collected data
 * Takes: <Nothing>
 * Pre: <Nothing>
 * Returns: TRUE32 if the tree was built, FALSE32 if the tree is already built
 */
void KDTree::BuildTree(DWORD dimen, DWORD dataCnt, float *posList, void **dataList, DWORD blockSize)
{
  Deinit();

  assert(dataCnt > 0);

  Dimen       = dimen;
  DataCnt     = dataCnt;
  dataBank.Init(sizeof(float)*Dimen, blockSize);
  nodeBank.Init(sizeof(KDNode),      blockSize);

	// recursively build the tree
	DoBuild(posList, dataList);
}

/* -------------------------------------------------- */

/* FindNearest: Attemps to find the q nearest nodes in the tree to a specified point
 * Takes:'q' as the number of neighbours to look for
 *       'target' as a pointer to the data to find neighbours to
 *       'outList' as a pointer to a list of spaces to place the found neighbours into
 *       'count' as a pointer to fill with the number of valid elements in the outList
 * Pre: 'target' points to valid data of the appropraite size
 *      'outList' points to a valid and appropriately sized buffer
 *      'count' points to a valid int
 * Returns: TRUE32 if neaighbours were searched for, FALSE32 if the tree is not yet built
 *          or the neighbour count is invalid
 */
void KDTree::FindKNearest(DWORD KNear, float *target, float (*distSqd)(float *e1, float *e2), void (*retFunc)(float *point, void *data))
{
  KDNode *kd;
  DWORD nearCnt, i, pivot;
  float maxDist, dist;
  float *checkPt, *hr;
  void *garbage;
  PriorityQueue pq;
  BlockMemBank recBank;
  BOOL32 firstRecur, leftIsFar;
  BYTE *tspace, *tptr;

  if (Root == NULL)
    return;

  maxDist  = MAX_FLOAT;
  nearCnt  = 0;
  kd       = Root;

  tspace  = new BYTE[sizeof(KDNode**)+sizeof(float)*Dimen*2+sizeof(BOOL32)*2];
  pivot   = kd->Pivot;
  checkPt = new float[Dimen];
  hr = (float*)(tspace + sizeof(KDNode**));
  for (i = 0; i < Dimen; i++)
  {
    hr[i*2]   = -MAX_FLOAT;
    hr[i*2+1] =  MAX_FLOAT;
  }

  recBank.Init(sizeof(KDNode**)+sizeof(float)*Dimen*2+sizeof(BOOL32)*2, 200);
  tptr = tspace;
  *((KDNode**)tptr) = Root;
  tptr += sizeof(KDNode**);
  memcpy(tptr, hr, sizeof(float)*Dimen*2);
  tptr += sizeof(float)*Dimen*2;
  *((BOOL32*)tptr) = TRUE32;
  tptr += sizeof(BOOL32);
  *((BOOL32*)tptr) = FALSE32;
  recBank.AddNewData(tspace);

  while (recBank.GetCount() > 0)
  {
    // pull info off the stack
    tptr   = recBank.GetLastData();
    kd     = *((KDNode**)tptr);
    tptr  += sizeof(KDNode*);
    memcpy(hr, tptr, sizeof(float)*Dimen*2);
    tptr += sizeof(float)*Dimen*2;
    firstRecur = *((BOOL32*)tptr);
    tptr += sizeof(BOOL32);
    leftIsFar  = *((BOOL32*)tptr);

    pivot   = kd->Pivot;

    // Return to the start of the data--i.e. don't remove it from the stack yet.
    // We may still make use of it (i.e. avoid deallocating and then reallocating the space).
    tptr   = recBank.GetLastData();

    if ((kd->Left == NULL) && (kd->Right == NULL))
    {
      // delete stack data
      recBank.RemoveData(recBank.GetLastData());

      dist = (*distSqd)(target, kd->Point);
      if (pq.GetCount() < KNear)
      {
        pq.Add(dist, (void*)kd, DM_IGNORE);
        pq.PeekMaxPriority(&maxDist);
      } else
      {
        if (dist <= maxDist)
        {
          pq.Add(dist, (void*)kd, DM_IGNORE);
          pq.ExtractMax(&garbage);
          pq.PeekMaxPriority(&maxDist);
        }
      }
    } else if (firstRecur == TRUE32)
    {
      if (target[pivot] <= kd->Point[pivot])
      {
        // change values for data already in the stack bank to represent the
        //  second part of the recursion
        tptr += sizeof(KDNode*);
        tptr += sizeof(float)*Dimen*2;
        *((BOOL32*)tptr) = FALSE32;
        tptr += sizeof(BOOL32);
        *((BOOL32*)tptr) = FALSE32; // right is farther than left 

        if (kd->Left != NULL)
        {
          // add the left side to the stack, setting it to run the first recursion
          tptr = tspace;
          *((KDNode**)tptr) = kd->Left;
          tptr += sizeof(KDNode*);
          memcpy(tptr, hr, sizeof(float)*Dimen*2);
          hr = (float*)tptr;
          hr[pivot*2+1] = kd->Point[pivot];
          tptr += sizeof(float)*Dimen*2;
          *((BOOL32*)tptr) = TRUE32;
          recBank.AddNewData(tspace);
        }
      } else
      {
        // change values for data already in the stack bank to represent the
        //  second part of the recursion
        tptr += sizeof(KDNode*);
        tptr += sizeof(float)*Dimen*2;
        *((BOOL32*)tptr) = FALSE32;
        tptr += sizeof(BOOL32);
        *((BOOL32*)tptr) = TRUE32; // left is farther than right

        if (kd->Right != NULL)
        {
          // add the left side to the stack, setting it to run the first recursion
          tptr = tspace;
          *((KDNode**)tptr) = kd->Right;
          tptr += sizeof(KDNode*);
          memcpy(tptr, hr, sizeof(float)*Dimen*2);
          hr = (float*)tptr;
          hr[pivot*2] = kd->Point[pivot];
          tptr += sizeof(float)*Dimen*2;
          *((BOOL32*)tptr) = TRUE32;
          recBank.AddNewData(tspace);
        }
      }
    } else
    {
      // see if we should add the current point to the 
      // k-nearest list.
      dist = (*distSqd)(target, kd->Point);
      if (pq.GetCount() < KNear)
      {
        pq.Add(dist, (void*)kd, DM_IGNORE);
        pq.PeekMaxPriority(&maxDist);
      } else
      {
        if (dist <= maxDist)
        {
          pq.Add(dist, (void*)kd, DM_IGNORE);
          pq.ExtractMax(&garbage);
          pq.PeekMaxPriority(&maxDist);
        }
      }

      if (leftIsFar == TRUE32)
      {
        hr[pivot*2+1]     = kd->Point[pivot];
      } else
      {
        hr[pivot*2]       = kd->Point[pivot];
      }

      buildNearPoint(checkPt, hr, target);
      if ((*distSqd)(checkPt, target) <= maxDist)
      { // if the further region could contain the nearest point, examine it
        if (leftIsFar == TRUE32)
        {
          if (kd->Left != NULL)
          {
            *((KDNode**)tptr) = kd->Left;
            tptr += sizeof(KDNode*);
            hr    = (float*)tptr;
            hr[pivot*2+1]     = kd->Point[pivot];
            tptr += sizeof(float)*Dimen*2;
            *((BOOL32*)tptr) = TRUE32;
          } else
            recBank.RemoveData(recBank.GetLastData());
        } else
        {
          if (kd->Right != NULL)
          {
            *((KDNode**)tptr) = kd->Right;
            tptr += sizeof(KDNode*);
            hr    = (float*)tptr;
            hr[pivot*2]       = kd->Point[pivot];
            tptr += sizeof(float)*Dimen*2;
            *((BOOL32*)tptr) = TRUE32;
          } else
            recBank.RemoveData(recBank.GetLastData());
        }
      } else
      {
        // delete stack data
        recBank.RemoveData(recBank.GetLastData());
      }
    }
  }

  // return results
  i = pq.GetCount();
  while (pq.GetCount() > 0)
  {
    i--;
    pq.ExtractMax((void**)&kd);
    (*retFunc)(kd->Point, kd->Data);
  }

  // cleanup
  delete [] checkPt;
  delete [] tspace;
}

/* -------------------------------------------------- */

/* RecBuild: Recursively attempts to build the KD-Tree based on the provided data
 * Takes: 'dest' as a pointer to a pointer to accept the address of the next node in
 *               the tree
 *        'node' as a pointer to a list of nodes to add to the tree. 
 * Pre: 'dest' points to a valid pointer
 *      'node' refers to NULL or a valid list of nodes, each node linked through the 
 *             Right member of the previous node
 * Returns: <Nothing>
 */
void KDTree::DoBuild(float *posList, void **dataList)
{
	KDNode *dataNode, **dest;
	float *range, maxVal;
	DWORD i, choice;
  DWORD minPt, maxPt;
	float dist, midpt, tpt;
  BlockMemBank recBank;
  BYTE tspace[sizeof(KDNode**)+sizeof(DWORD)*2+sizeof(DWORD*)*2];
  BYTE *tptr;
  DWORD *index1, *index2;
  DWORD min, max, best, p1, p2, j;
  float *pos;

  index1 = new DWORD[DataCnt];
  index2 = new DWORD[DataCnt];
  for (i = 0; i < DataCnt; i++)
  {
    index1[i] = i;
  }

	// init the range holders
  range = new float[Dimen*2];

  recBank.Init(sizeof(KDNode**)+sizeof(DWORD)*2+sizeof(DWORD*)*2, 200);
  tptr = tspace;
  *((KDNode***)tptr) = &Root;
  tptr += sizeof(KDNode**);
  *((DWORD*)tptr) = 0;
  tptr += sizeof(DWORD);
  *((DWORD*)tptr) = DataCnt-1;
  tptr += sizeof(DWORD);
  *((DWORD**)tptr) = index1;
  tptr += sizeof(DWORD*);
  *((DWORD**)tptr) = index2;
  recBank.AddNewData(tspace);

  while (recBank.GetCount() != 0)
  {
    tptr   = recBank.GetLastData();
    dest   = *((KDNode***)tptr);
    tptr  += sizeof(KDNode**);
    min    = *((DWORD*)tptr);
    tptr  += sizeof(DWORD);
    max    = *((DWORD*)tptr);
    tptr  += sizeof(DWORD);
    index1 = *((DWORD**)tptr);
    tptr  += sizeof(DWORD*);
    index2 = *((DWORD**)tptr);

    // delete stack data
    recBank.RemoveData(recBank.GetLastData());

	  // if the sent node is NULL, return NULL as the trivial case
	  if (min == max)
	  {
      // place single node
      dataNode = *dest = (KDNode*)nodeBank.NewData();
      dataNode->Pivot = choice;
      dataNode->Data  = dataList[index1[min]];
      dataNode->Point = &posList[index1[min]*Dimen];
      dataNode->Left  = dataNode->Right = NULL;
		  continue;
    }
    if (min > max)
    {
      continue;
    }

    // init range data
	  for (i = 0, minPt = KD_MIN, maxPt = KD_MAX; i < Dimen; i++, minPt+=2, maxPt+=2)
	  {
		  range[minPt] =  MAX_FLOAT;
		  range[maxPt] = -MIN_FLOAT;
	  }

	  // find ranges for each dimension
	  for (j = min; j <= max; j++)
	  {
      pos = &posList[index1[j]*Dimen];
		  for (i = 0, minPt = KD_MIN, maxPt = KD_MAX; i < Dimen; i++, minPt+=2, maxPt+=2)
		  {
			  if (pos[i] < range[minPt])
			  {
				  range[minPt] = pos[i];
			  }
			  if (pos[i] > range[maxPt])
			  {
				  range[maxPt] = pos[i];
			  }
		  }
	  }

	  // find the dimension with the maximum range
	  maxVal = -1;
	  for (i = 0, minPt = KD_MIN, maxPt = KD_MAX; i < Dimen; i++, minPt+=2, maxPt+=2)
	  {
		  if (maxVal < (range[maxPt] - range[minPt]))
		  {
			  choice = i;
			  maxVal = range[maxPt] - range[minPt];
		  }
	  }

	  // find the midpoint of the data
	  midpt = (float)(range[choice*2+KD_MAX] + range[choice*2+KD_MIN])*0.5f;
	  
	  // find element closest to the midpoint of the pivot range
	  dist = MAX_FLOAT;
    best = min;
	  for (j = min; j <= max; j++)
    {
      tpt = posList[index1[j]*Dimen + choice] - midpt;
		  tpt = ABS(tpt);
	    if (tpt <= dist)
		  {
		    best = j;
		    dist = tpt;
      }
    }

	  // put the pivot node into the destination node
		dataNode = *dest = (KDNode*)nodeBank.NewData();
    dataNode->Pivot = choice;
    dataNode->Data  = dataList[index1[best]];
    dataNode->Point = &posList[index1[best]*Dimen];
    dataNode->Left  = dataNode->Right = NULL;
	  
    // split the data into their respective sides
    dist = posList[index1[best]*Dimen+choice];
    p1   = min;
    p2   = max;
    for (j = min; j < best; j++)
    {
      if (posList[index1[j]*Dimen+choice] <= dist)
      {
        index2[p1++] = index1[j];
      } else
      {
        index2[p2--] = index1[j];
      }
    }
    for (j = best+1; j <= max; j++)
    {
      if (posList[index1[j]*Dimen+choice] <= dist)
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
      *((KDNode***)tptr) = &(dataNode->Left);
      tptr += sizeof(KDNode**);
      *((DWORD*)tptr) = min;
      tptr += sizeof(DWORD);
      *((DWORD*)tptr) = p1-1;
      tptr += sizeof(DWORD);
      *((DWORD**)tptr) = index2;
      tptr += sizeof(DWORD*);
      *((DWORD**)tptr) = index1;
      recBank.AddNewData(tspace);
    }

    if (p2 < max)
    {
      // right side
      tptr = tspace;
      *((KDNode***)tptr) = &(dataNode->Right);
      tptr += sizeof(KDNode**);
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
  delete [] range;
}

/* -------------------------------------------------- */

BOOL32 KDTree::inRange(float *rangeMin, float *rangeMax, float *point)
{
  DWORD i;

  for (i = 0; i < Dimen; i++)
  {
    if ((point[i] < rangeMin[i]) || (point[i] > rangeMax[i]))
      return FALSE32;
  }

  return TRUE32;
}

/* -------------------------------------------------- */

BOOL32 KDTree::regIntersect(float *rangeMin, float *rangeMax, float *hr)
{
  DWORD i;
  float *val;

  val = hr;
  for (i = 0; i < Dimen; i++)
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

void KDTree::buildNearPoint(float *checkPt, float *hr, float *target)
{
  DWORD i, minPt, maxPt;

	// check all the dimensions in the hr for possible points
	for (i = 0, minPt = KD_MIN, maxPt = KD_MAX; i < Dimen; i++, minPt+=2, maxPt+=2)
	{
		// find the closest point to the target
		if (target[i] <= hr[minPt])
		{
			checkPt[i] = hr[minPt];
		} else if (target[i] >= hr[maxPt])
		{
			checkPt[i] = hr[maxPt];
		} else
		{
			checkPt[i] = target[i];
		}
	}
}

/* -------------------------------------------------- */

void KDTree::buildFarPoint(float *checkPt, float *hr, float *target)
{
  DWORD i, minPt, maxPt;
  float d1, d2;

	// check all the dimensions in the hr for possible points
	for (i = 0, minPt = KD_MIN, maxPt = KD_MAX; i < Dimen; i++, minPt+=2, maxPt+=2)
	{
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

BOOL32 KDTree::regContained(float *rangeMin, float *rangeMax, float *hr)
{
  DWORD i;
  float *val;

  val = hr;
  for (i = 0; i < Dimen; i++)
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

void KDTree::FindInDist(float *target, float maxdist, float (*distSqd)(float *e1, float *e2), void (*retFunc)(float *point, void *data))
{
  KDNode *kd;
  float *hr;
  DWORD i, pivot;
  BlockMemBank recBank;
  BOOL32 report;
  BYTE *tspace, *tptr;
  float holdval;
  float *tempPt;

  maxdist = maxdist*maxdist;

  hr     = new float[Dimen*2];
  tempPt = new float[Dimen];
  for (i = 0; i < Dimen; i++)
  {
    hr[i*2]   = -MAX_FLOAT;
    hr[i*2+1] =  MAX_FLOAT;
  }

  // seed the stack
  tspace  = new BYTE[sizeof(KDNode*)+sizeof(float)*2*Dimen+sizeof(BOOL32)];
  recBank.Init(sizeof(KDNode*)+sizeof(float)*2*Dimen+sizeof(BOOL32), 200);
  tptr = tspace;
  *((KDNode**)tptr) = Root;
  tptr += sizeof(KDNode*);
  memcpy(tptr, hr, sizeof(float)*2*Dimen);
  tptr += sizeof(float)*2*Dimen;
  *((BOOL32*)tptr) = FALSE32;
  recBank.AddNewData(tspace);

  while (recBank.GetCount() > 0)
  {
    // grab data off the stack
    tptr = recBank.GetLastData();
    kd = *((KDNode**)tptr);
    tptr += sizeof(KDNode*);
    tptr += sizeof(float)*2*Dimen;
    report = *((BOOL32*)tptr);
    recBank.RemoveData(recBank.GetLastData());

    if (report == TRUE32) // if we're reporting the subtree, just report it all
    {
      (*retFunc)(kd->Point, kd->Data);
      if (kd->Left != NULL)
      {
        tptr = tspace;
        *((KDNode**)tptr) = kd->Left;
        tptr += sizeof(KDNode*);
        tptr += sizeof(float)*2*Dimen;
        *((BOOL32*)tptr) = TRUE32;
        recBank.AddNewData(tspace);
      }
      if (kd->Right != NULL)
      {
        tptr = tspace;
        *((KDNode**)tptr) = kd->Right;
        tptr += sizeof(KDNode*);
        tptr += sizeof(float)*2*Dimen;
        *((BOOL32*)tptr) = TRUE32;
        recBank.AddNewData(tspace);
      }
    } else
    {
      // see if we should report this node
      if ( (holdval = (*distSqd)(kd->Point, target)) <= maxdist)
      {
        (*retFunc)(kd->Point, kd->Data);
      }

      tptr -= sizeof(float)*2*Dimen;
      memcpy(hr, tptr, sizeof(float)*2*Dimen);
      pivot = kd->Pivot;

      if (kd->Left != NULL)
      {
        holdval = hr[pivot*2+1];
        hr[pivot*2+1] = kd->Point[pivot];
        buildFarPoint(tempPt, hr, target);
        if ((*distSqd)(tempPt, target) <= maxdist)
        { // if subtree is completely contained, report it all
          tptr = tspace;
          *((KDNode**)tptr) = kd->Left;
          tptr += sizeof(KDNode*);
          tptr += sizeof(float)*2*Dimen;
          *((BOOL32*)tptr) = TRUE32;
          recBank.AddNewData(tspace);
        } else
        {
          buildNearPoint(tempPt, hr, target);
          if ((*distSqd)(tempPt, target) <= maxdist)
          { // if partially contained, keep looking downwards
            tptr = tspace;
            *((KDNode**)tptr) = kd->Left;
            tptr += sizeof(KDNode*);
            memcpy(tptr, hr, sizeof(float)*2*Dimen);
            tptr += sizeof(float)*2*Dimen;
            *((BOOL32*)tptr) = FALSE32;
            recBank.AddNewData(tspace);
          }
        }
        hr[pivot*2+1] = holdval;
      }

      if (kd->Right != NULL)
      {
        holdval = hr[pivot*2];
        hr[pivot*2] = kd->Point[pivot];
        buildFarPoint(tempPt, hr, target);
        if ((*distSqd)(tempPt, target) <= maxdist)
        { // if subtree is completely contained, report it all
          tptr = tspace;
          *((KDNode**)tptr) = kd->Right;
          tptr += sizeof(KDNode*);
          tptr += sizeof(float)*2*Dimen;
          *((BOOL32*)tptr) = TRUE32;
          recBank.AddNewData(tspace);
        } else
        {
          buildNearPoint(tempPt, hr, target);
          if ((*distSqd)(tempPt, target) <= maxdist)
          { // if partially contained, keep looking downwards
            tptr = tspace;
            *((KDNode**)tptr) = kd->Right;
            tptr += sizeof(KDNode*);
            memcpy(tptr, hr, sizeof(float)*2*Dimen);
            tptr += sizeof(float)*2*Dimen;
            *((BOOL32*)tptr) = FALSE32;
            recBank.AddNewData(tspace);
          }
        }
        hr[pivot*2] = holdval;
      }
    }
  }

  // cleanup
  delete [] tempPt;
  delete [] tspace;
}

/* -------------------------------------------------- */

void KDTree::FindInRange(float *rangeMin, float *rangeMax, void (*retFunc)(float *point, void *data))
{
  KDNode *kd;
  float *hr;
  DWORD i, pivot;
  BlockMemBank recBank;
  BOOL32 report;
  BYTE *tspace, *tptr;
  float holdval;

  hr = new float[Dimen*2];
  for (i = 0; i < Dimen; i++)
  {
    hr[i*2]   = -MAX_FLOAT;
    hr[i*2+1] =  MAX_FLOAT;
  }

  // seed the stack
  tspace  = new BYTE[sizeof(KDNode*)+sizeof(float)*2*Dimen+sizeof(BOOL32)];
  recBank.Init(sizeof(KDNode*)+sizeof(float)*2*Dimen+sizeof(BOOL32), 200);
  tptr = tspace;
  *((KDNode**)tptr) = Root;
  tptr += sizeof(KDNode*);
  memcpy(tptr, hr, sizeof(float)*2*Dimen);
  tptr += sizeof(float)*2*Dimen;
  *((BOOL32*)tptr) = FALSE32;
  recBank.AddNewData(tspace);

  while (recBank.GetCount() > 0)
  {
    // grab data off the stack
    tptr = recBank.GetLastData();
    kd = *((KDNode**)tptr);
    tptr += sizeof(KDNode*);
    tptr += sizeof(float)*2*Dimen;
    report = *((BOOL32*)tptr);
    recBank.RemoveData(recBank.GetLastData());

    if (report == TRUE32) // if we're reporting the subtree, just report it all
    {
      (*retFunc)(kd->Point, kd->Data);
      if (kd->Left != NULL)
      {
        tptr = tspace;
        *((KDNode**)tptr) = kd->Left;
        tptr += sizeof(KDNode*);
        tptr += sizeof(float)*2*Dimen;
        *((BOOL32*)tptr) = TRUE32;
        recBank.AddNewData(tspace);
      }
      if (kd->Right != NULL)
      {
        tptr = tspace;
        *((KDNode**)tptr) = kd->Right;
        tptr += sizeof(KDNode*);
        tptr += sizeof(float)*2*Dimen;
        *((BOOL32*)tptr) = TRUE32;
        recBank.AddNewData(tspace);
      }
    } else
    {
      // see if we should report this node
      if (inRange(rangeMin, rangeMax, kd->Point) == TRUE32)
      {
        (*retFunc)(kd->Point, kd->Data);
      }

      tptr -= sizeof(float)*2*Dimen;
      memcpy(hr, tptr, sizeof(float)*2*Dimen);
      pivot = kd->Pivot;

      if (kd->Left != NULL)
      {
        holdval = hr[pivot*2+1];
        hr[pivot*2+1] = kd->Point[pivot];
        if (regContained(rangeMin, rangeMax, hr) == TRUE32)
        { // if subtree is completely contained, report it all
          tptr = tspace;
          *((KDNode**)tptr) = kd->Left;
          tptr += sizeof(KDNode*);
          tptr += sizeof(float)*2*Dimen;
          *((BOOL32*)tptr) = TRUE32;
          recBank.AddNewData(tspace);
        } else if (regIntersect(rangeMin, rangeMax, hr) == TRUE32)
        { // if partially contained, keep looking downwards
          tptr = tspace;
          *((KDNode**)tptr) = kd->Left;
          tptr += sizeof(KDNode*);
          memcpy(tptr, hr, sizeof(float)*2*Dimen);
          tptr += sizeof(float)*2*Dimen;
          *((BOOL32*)tptr) = FALSE32;
          recBank.AddNewData(tspace);
        }
        hr[pivot*2+1] = holdval;
      }

      if (kd->Right != NULL)
      {
        holdval = hr[pivot*2];
        hr[pivot*2] = kd->Point[pivot];
        if (regContained(rangeMin, rangeMax, hr) == TRUE32)
        { // if subtree is completely contained, report it all
          tptr = tspace;
          *((KDNode**)tptr) = kd->Right;
          tptr += sizeof(KDNode*);
          tptr += sizeof(float)*2*Dimen;
          *((BOOL32*)tptr) = TRUE32;
          recBank.AddNewData(tspace);
        } else if (regIntersect(rangeMin, rangeMax, hr) == TRUE32)
        { // if partially contained, keep looking downwards
          tptr = tspace;
          *((KDNode**)tptr) = kd->Right;
          tptr += sizeof(KDNode*);
          memcpy(tptr, hr, sizeof(float)*2*Dimen);
          tptr += sizeof(float)*2*Dimen;
          *((BOOL32*)tptr) = FALSE32;
          recBank.AddNewData(tspace);
        }
        hr[pivot*2] = holdval;
      }
    }
  }

  delete [] tspace;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
