/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <time.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <MachinePort.h>
#include <BlockMemBank.hpp>
#include <StringExt.h>
#include <MathExt.h>
#include "StarImage.hpp"
#include "StarShapeSys.hpp"
#include "StarCamera.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * Given a list of image names and a list of indexes, the program reads each
 * image and then attempts to solve the images useing the indexes.
 *
 * Usage: <output_results> <cat_file> <index_file> <name_file>
 * <output_results>: output file for results
 * <cat_file>      : catalog star file
 * <index_desc>    : index file descriptor
 * <name_file>     : a file containing a list of star images to solve
 * Ex: out.txt cat.rand.10000.bin index.lst im__image.lst
 *
 * <index_desc> contains lines of the form:
 * <index_name> <min_angle_tol> <max_angle_tol> <min_distance_tol> <max_distance_tol>
 * <index_name>      : Refers to the name of an index to use.
 * <min_angle_tol>   : Refers to the smallest tolerance to use when matching 
 *                     angles in shapes.
 * <max_angle_tol>   : Refers to the largest tolerance to use when matching 
 *                     angles in shapes.
 * <min_distance_tol>: Refers to the smallest tolerance to use when matching 
 *                     distances in shapes.
 * <max_distance_tol>: Refers to the largest tolerance to use when matching 
 *                     distances in shapes.
 * Ex: index.01.v3.1.lines 0.01 0.1 0.0001 0.001
 * 
 * <name_file> contains a list of image file names
 *
 * Each image file contains lines of the form:
 * <id> <x> <y>
 * where <x> and <y> are in [-1, 1] x [-1, 1].
 */

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// minimum distance matching tolerance
#define MIN_DIST_TOL      0.000000001
// 1 => reverse x/y positions in the image file
// 0 => don't reverse
#define REVERSE_X         0

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * DeleteNameList: deletes a list of names and lookup offsets
 * Assumes: 'list' and 'offsets' refer to allocated space.
 */
void DeleteNameList(char *list, DWORD *offsets)
{
  delete [] list;
  delete [] offsets;
}

/* -------------------------------------------------- */

/*
 * DeleteShapeSys: Deletes all 'sysCount' shape systems and the associated use order.
 * Assumes: 'sysCount' shape systems exist in 'sysList', and 'sysOrder' refers to allocated space.
 */
void DeleteShapeSys(DWORD sysCount, StarShapeSys **sysList, DWORD *sysOrder)
{
  DWORD i;

  for (i = 0; i < sysCount; i++)
  {
    delete sysList[i];
  }
  delete [] sysList;
  delete [] sysOrder;
}

/* -------------------------------------------------- */

/*
 * ReadAsciiImage: Reads in an ascii image from 'fname' into the provided StarImage 'im'. Returns
 *                 TRUE32 if the read succeeds, FALSE32 if it fails.
 * Assumes: 'fname' refers to a valid ascii star image, 'im' is a valid StarImage to place the 
 *          data into.
 */
BOOL32 ReadAsciiImage(char *fname, StarImage *im)
{
  FILE *in;
  BlockMemBank tbank;
  float data[3];
  float *tptr;
  Enumeration *tenum;
  DWORD i;

  in = fopen(fname, "rb");
  if (in == NULL)
  {
    return FALSE32;
  }
  tbank.Init(sizeof(float)*3);

  // scan each star
  while (!feof(in))
  {
    if (fscanf(in, "%f %f %f",  &data[0], &data[1], &data[2]) < 3)
    {
      if (feof(in))
        break;
      else
        return FALSE32;
    }
    tbank.AddNewData(data);
  }
  fclose(in);
  
  // build the final image data
  im->Init(tbank.GetCount());
  tenum = tbank.NewEnum(FORWARD);
  i = 0;
  im->RealCount = 0;
  im->FakeCount = 0;
  while (tenum->GetEnumOK() == TRUE32)
  {
    tptr = (float*)tenum->GetEnumNext();
    if ((tptr[0] == 0) || (tptr[0] == -1))
    {
      im->TrueIndex[i] = DIST_STAR_INDEX;
      im->FakeCount++;
    } else
    {
      im->TrueIndex[i] = (DWORD)tptr[0]-1;
      im->RealCount++;
    }
#if (REVERSE_X == 1)
    im->Stars[0][i]  = -tptr[1]*0.5;
#else
    im->Stars[0][i]  = tptr[1]*0.5;
#endif
    im->Stars[1][i]  = tptr[2]*0.5;
    im->Stars[2][i]  = 0;
    i++;
  }
  delete tenum;

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * ReadFileList: Reads a list of image file names from the file 'name_file', placing the
 *               number of names into 'nameCount', the names one after the other into 'nameList',
 *               and the starting offsets of each name into 'offsetList'. Returns TRUE32 if the process
 *               succeeds, FALSE32 if it fails.
 * Assumes: 'name_file' refers to a valid name file, 'nameCount' is a valid DWORD pointer, 
 *          'nameList' is a valid character pointer-pointer, and 'offList' is a valid
 *           DWORD pointer-pointer.
 */
BOOL32 ReadFileList(char *name_file, DWORD *nameCount, char **nameList, DWORD **offsetList)
{
  FILE *in;
  char tspace[1024];
  DWORD i, goal;
  DWORD len;
  char *tlist, *tptr;
  DWORD *toff;
  BlockMemBank tbank;
  Enumeration *tenum;

  // open the name file
  in = fopen(name_file, "rb");
  if (in == NULL)
  {
    return FALSE32;
  }
  tbank.Init(sizeof(char)*1024);

  // get all the file names
  len = 0;
  while (!feof(in))
  {
    if (fgets(tspace, 1024, in) == NULL)
    {
      if (feof(in))
        break;

      return FALSE32;
    }
    len += Chop(tspace);
    tbank.AddNewData(tspace);
  }

  // create the final list of names
  *nameCount = goal = tbank.GetCount();
  if (goal > 0)
  {
    *nameList   = tlist = new char[len+tbank.GetCount()];
    *offsetList = toff  = new DWORD[tbank.GetCount()];

    len = 0;
    tenum = tbank.NewEnum(FORWARD);
    for (i = 0; i < goal; i++)
    {
      toff[i] = len;
      tptr    = (char*)tenum->GetEnumNext();
      strcpy(&tlist[len], tptr);
      len    += strlen(tptr)+1;
    }
    delete tenum;
  }

  fclose(in);

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * ReadIndexFileList: Reads in the names of the indices and their associated 
 *                    matching tolerances from 'name_file', returning the number of 
 *                    names in 'nameCount', all the names one after the other in 'nameList', 
 *                    the tolerances in 'paramList', and the name offsets within 'nameList'
 *                    in 'offsetList'. Returns TRUE32 if the process succeeds, FALSE32 if it fails.
 * Assumes: 'name_file' refers to a valid list of indices, 'nameCount' refers to a valid DWORD pointer,
 *          'nameList' refers to avalid character pointer-pointer, paramList refers to a valid double
 *          pointer-pointer, and offsetList refers to a valid DWORD pointer-pointer.
 *
 * Each line in the name_file is of the form:
 * <index_name> <min_angle_tol> <max_angle_tol> <min_distance_tol> <max_distance_tol>
 * Ex: index.01.v3.1.lines 0.01 0.1 0.0001 0.001
 */
BOOL32 ReadIndexFileList(char *name_file, DWORD *nameCount, char **nameList, double **paramList, DWORD **offsetList)
{
  FILE *in;
  char tspace[1024];
  DWORD i, goal;
  DWORD len;
  char *tlist, *tptr;
  double *pList;
  DWORD *toff;
  BlockMemBank tbank;
  Enumeration *tenum;

  // open the file
  in = fopen(name_file, "rb");
  if (in == NULL)
  {
    return FALSE32;
  }
  tbank.Init(sizeof(char)*1024);

  // read each line
  len = 0;
  while (!feof(in))
  {
    if (fgets(tspace, 1024, in) == NULL)
    {
      if (feof(in))
        break;

      return FALSE32;
    }
    len += Chop(tspace);
    tbank.AddNewData(tspace);
  }
  fclose(in);

  // parse each line of the file
  *nameCount = goal = tbank.GetCount();
  if (goal > 0)
  {
    *nameList   = tlist = new char[len+tbank.GetCount()];
    *offsetList = toff  = new DWORD[tbank.GetCount()];
    *paramList  = pList = new double[tbank.GetCount()*4*sizeof(double)];

    len = 0;
    tenum = tbank.NewEnum(FORWARD);
    for (i = 0; i < goal; i++)
    {
      toff[i] = len;
      tptr    = (char*)tenum->GetEnumNext();
      if (sscanf(tptr, "%s %lg %lg %lg %lg", tspace, &pList[i*4+0], &pList[i*4+1], &pList[i*4+2], &pList[i*4+3]) < 4)
      {
        delete [] tlist;
        delete [] toff;
        delete [] pList;
        return FALSE32;
      }
      strcpy(&tlist[len], tspace);
      len    += strlen(tspace)+1;
    }
    delete tenum;
  }

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * cmpPoints: A function used by qsort to sort a set of floating point values.
 */
int cmpPoints(const void *elem1, const void *elem2)
{
  float e1, e2;

  e1 = *((float*)elem1);
  e2 = *((float*)elem2);
  // sort by view angle (largest to smallest)
  if (e1 > e2)
  {
    return -1;
  } else if (e1 < e2)
  {
    return 1;
  }

  e1 = *(((float*)elem1)+1);
  e2 = *(((float*)elem2)+1);
  // sort by points per shape (largest to smallest)
  if (e1 > e2)
  {
    return -1;
  } else if (e1 < e2)
  {
    return 1;
  }

  e1 = *(((float*)elem1)+2);
  e2 = *(((float*)elem2)+2);
  // sort be wedge angle (smallest to largest)
  if (e1 < e2)
  {
    return -1;
  } else if (e1 > e2)
  {
    return 1;
  }

  return 0;
}

/* -------------------------------------------------- */

/* 
 * ReadIndex: Reads the list of index names and parameters from 'index_file', returns the number of
 *            systems read via 'sysCount', returns the actual indexes via 'shapeSysList', and the
 *            prefered use order via 'sysOrder'. The prefered use order sorts the shape systems by increasing
 *            field of view.
 * Assumes: 'index_file' refers to valid list of indexes and match parameters, 'sysCount' is a valid DWORD pointer,
 *          'shapeSysList' refers to a valid StarShapeSys pointer-pointer-pointer, and 'sysOrder' refers to valid
 *          DWORD pointer-pointer.
 */
BOOL32 ReadIndex(char *index_file, DWORD *sysCount, StarShapeSys ***shapeSysList, DWORD **sysOrder)
{
  DWORD nameCount;
  char *nameList;
  DWORD *nameOff;
  StarShapeSys **tlist;
  double *paramList;
  DWORD i;
  BOOL32 ret;
  float *minPoints;
  StarShapeMatchParams params;

  printf("  Reading Index Names...\n");
  fflush(stdout);

  // read the index information
  if (ReadIndexFileList(index_file, &nameCount, &nameList, &paramList, &nameOff) == FALSE32)
  {
    return FALSE32;
  }

  // read each index from the index file
  *sysCount = nameCount;
  if (nameCount > 0)
  {
    printf("  Opening Indexes...\n");
    fflush(stdout);

    *shapeSysList = tlist = new StarShapeSys*[nameCount];
    minPoints = new float[nameCount*4];
    ret = TRUE32;
    // read each file
    for (i = 0; i < nameCount; i++)
    {
      printf("     Opening %s\n", &nameList[nameOff[i]]);
      fflush(stdout);

      tlist[i] = new StarShapeSys();
      // open the index
      if (tlist[i]->OpenIndex(&nameList[nameOff[i]]) == FALSE32)
      {
        printf("        Unable to open index: %s\n", &nameList[nameOff[i]]);
        ret = FALSE32;
        minPoints[i*4]   = 0;
        minPoints[i*4+1] = 0;
        minPoints[i*4+2] = 0;
      } else
      {
        minPoints[i*4]   = (float)tlist[i]->GetMaxImAngle();
        minPoints[i*4+1] = (float)tlist[i]->GetMinShapePoints();
        minPoints[i*4+2] = (float)tlist[i]->GetWedgeAngle();
      }
      // set the match parameters
      params.AngMatchTolMin  = paramList[i*4+0];
      params.AngMatchTolMax  = paramList[i*4+1];
      params.DistMatchTolMin = paramList[i*4+2];
      params.DistMatchTolMax = paramList[i*4+3];
      printf("       Params: %lg %lg %lg %lg\n", 
             params.AngMatchTolMin, params.AngMatchTolMax, params.DistMatchTolMin, params.DistMatchTolMax);
      tlist[i]->InitMatchParams(&params);
      minPoints[i*4+3] = (float)i;
    }

    if (ret == TRUE32)
    {
      printf("  Sorting Indexes...\n");
      fflush(stdout);

      // order the indices by increasing field of view
      qsort(minPoints, nameCount, sizeof(float)*4, cmpPoints);
      *sysOrder = new DWORD[nameCount];
      for (i = 0; i < nameCount; i++)
      {
        (*sysOrder)[i] = (DWORD)minPoints[i*4+3];
      }
    } else
    {
      printf("  Error Opening Indexes!\n");
      fflush(stdout);
    }

    printf("  Clean Temp Index Space...\n");
    fflush(stdout);

    // cleanup
    delete [] paramList;
    delete [] nameList;
    delete [] nameOff;
    delete [] minPoints;

    if (ret == FALSE32)
    {
      for (i = 0; i < nameCount; i++)
      {
        delete tlist[i];
      }
      delete [] tlist;
    }
  }

  return ret;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * Given a list of image names and a list of indexes, the program reads each
 * image and then attempts to solve the images useing the indexes.
 *
 * Usage: <output_results> <cat_file> <index_file> <name_file>
 * <output_results>: output file for results
 * <cat_file>      : catalog star file
 * <index_desc>    : index file descriptor
 * <name_file>     : a file containing a list of star images to solve
 * Ex: out.txt cat.rand.10000.bin index.lst im__image.lst
 *
 * <index_desc> contains lines of the form:
 * <index_name> <min_angle_tol> <max_angle_tol> <min_distance_tol> <max_distance_tol>
 * Ex: index.01.v3.1.lines 0.01 0.1 0.0001 0.001
 * 
 * <name_file> contains a list of image file names
 *
 * Each image file contains lines of the form:
 * <id> <x> <y>
 * where <x> and <y> are in [-1, 1] x [-1, 1].
 */
int main(int argc, char *argv[])
{
  StarList  cList;
  StarList  dList;
  StarShapeSys **shapeSysList;
  DWORD     sysCount;
  DWORD     *sysOrder;
  StarImage im;
  StarShapeMatch *solution;
  DWORD   solCnt;
  float   jitter;
  char *output_file;
  char *cat_file;
  char *index_file;
  char *name_file;
  FILE *out;
  DWORD i, j, k;
  DWORD imCount;
  BOOL32 iGood;
  DWORD totSuccess;
  double avgStars;
  double ra, dec;
  StarCamera cam;
  char  *nameList;
  DWORD *nameOff;
  DWORD err1, err2, err3;
  DWORD gCnt, totGCnt, totSolCnt;
  DWORD *listHist;

  // output usage
  if (argc < 5)
  {
    printf("\n");
    printf("Usage: <output_results> <cat_file> <index_file> <name_file>\n");
    printf("<output_results>: output file for results\n");
    printf("<cat_file>      : catalog star file\n");
    printf("<index_desc>    : index file descriptor\n");
    printf("<name_file>     : a file containing a list of star images to solve\n");
    printf("\n");
    return -1;
  } else
  {
    output_file = argv[1];
    cat_file    = argv[2];
    index_file  = argv[3];
    name_file   = argv[4];
  }

  // output solve info
  printf("\n");
  printf("Output         : %s\n", output_file);
  printf("Catalog File   : %s\n", cat_file);
  printf("Index File     : %s\n", index_file);
  printf("Image File     : %s\n", name_file);
  printf("\n");
  fflush(stdout);

  printf("Reading Image Names...\n");
  fflush(stdout);

  // get image file list
  if (ReadFileList(name_file, &imCount, &nameList, &nameOff) == FALSE32)
  {
    printf("Error reading image information file\n");
    return -1;
  }

  printf("Reading Indexes...\n");
  fflush(stdout);

  // read indexes
  if (ReadIndex(index_file, &sysCount, &shapeSysList, &sysOrder) == FALSE32)
  {
    printf("Error reading index descriptor\n");
    return -1;
  }

  // output index info summary
  printf("\nImage Count: %u\n\n", imCount);
  printf("Indexes to use: %u\n", sysCount);
  printf("Index Use Order:\n");
  for (i = 0; i < sysCount; i++)
  {
    printf("%u\n", sysOrder[i]);
  }
  printf("\n");
  fflush(stdout);

  printf("Reading Star Catalog...\n");
  fflush(stdout);

  // open catalog to match with
  if (cList.Open(cat_file) == FALSE32)
  {
    printf("Error opening catalog file: %s\n", cat_file);
    DeleteNameList(nameList, nameOff);
    DeleteShapeSys(sysCount, shapeSysList, sysOrder);
    return -1;
  }
  printf("Updating Line Solver Params...\n");
  fflush(stdout);
  // init index matching params
  for (i = 0; i < sysCount; i++)
  {
    shapeSysList[i]->SetStarList(&cList);
  }

  printf("Prepping Output File...\n");
  fflush(stdout);

  // prep output file
  out = fopen(output_file, "wt");
  if (out == NULL)
  {
    printf("Unable to open output file\n");
    DeleteNameList(nameList, nameOff);
    DeleteShapeSys(sysCount, shapeSysList, sysOrder);
    return -1;
  }
  fprintf(out, "%u\n", imCount);
  fflush(out);

  jitter = 0;
  totSuccess = 0;
  avgStars = 0;

  totGCnt = totSolCnt = 0;
  listHist = new DWORD[sysCount];
  for (i = 0; i < sysCount; i++)
  {
    listHist[i] = 0;
  }
  // try to solve all images
  for (i = 0; i < imCount; i++)
  {
    printf("Reading Image %s\n", &nameList[nameOff[i]]);
    fflush(stdout);

    // read image
    if (ReadAsciiImage(&nameList[nameOff[i]], &im) == FALSE32)
    {
      printf("Unable to open image: %s\n   Skipping...", nameList[nameOff[i]]);
      continue;
    }
   
    printf("Image %u\nStarting: %s\n", i+1, &nameList[nameOff[i]]);
    printf("  Starcount: %u    Real Stars: %u    Fake Stars: %u\n\n", im.StarCount, im.RealCount, im.FakeCount);
    fflush(stdout);

    avgStars += im.StarCount;

    // try to solve using all systems
    for (k = 0; k < sysCount; k++)
    {
      printf("Trying index: %u -- Min %u points, Max Radius: %lf, Wedge Angle: %lf\n", 
              sysOrder[k], shapeSysList[sysOrder[k]]->GetMinShapePoints(), 
              shapeSysList[sysOrder[k]]->GetMaxImAngle(), shapeSysList[sysOrder[k]]->GetWedgeAngle());
      shapeSysList[sysOrder[k]]->SolveImage(&im, &solCnt, &solution);
      iGood = FALSE32;
      gCnt = 0;
      totSolCnt += solCnt;
      // see if solution was found
      if (solCnt > 0)
      {
        printf("Solutions:\n");
        for (j = 0; j < solCnt; j++)
        {
          printf("Solution %u/%u:\n       Angle: %g\n       i: %g %g %g\n       j: %g %g %g\n       k: %g %g %g\n", 
                  j+1, solCnt,
                  solution[j].viewAngle*DEG_CONV,
                  solution[j].imI[0], solution[j].imI[1], solution[j].imI[2],
                  solution[j].imJ[0], solution[j].imJ[1], solution[j].imJ[2],
                  solution[j].imK[0], solution[j].imK[1], solution[j].imK[2]);

          ra  = atan(solution[j].imK[1]/solution[j].imK[0]);
          dec = acos(solution[j].imK[2]);
          printf("\n       RA: %g   DEC: %g\n\n", ra*DEG_CONV, dec*DEG_CONV-90);
          err1 = (solution[j].starNum      == im.TrueIndex[solution[j].imStar]);
          err2 = (solution[j].nearStarNum  == im.TrueIndex[solution[j].imNearStar]);
          err3 = (solution[j].thirdStarNum == im.TrueIndex[solution[j].imThirdStar]);
          printf("       Match: %u   %u   %u\n", err1, err2, err3);
          if (((err1) && (err2)) && (err3))
          {
            iGood = TRUE32;
            gCnt++;
            printf("       [Solution GOOD]");
          } else
          {
            printf("       [Solution BAD]");
          }
          printf("\n");
        }
        delete [] solution;
      }
      totGCnt += gCnt;
      printf("Success Rate: %u/%u\n", gCnt, solCnt);
      fflush(out);
      if (iGood == TRUE32)
      {
        listHist[sysOrder[k]]++;
        break;
      }
    }
    if (iGood == TRUE32)
    {
      printf("Solution found\n");
      fprintf(out, "Y\n");
      fflush(out);
      totSuccess++;
    } else
    {
      printf("No solution found\n");
      fprintf(out, "N\n");
      fflush(out);
    }
    printf("\n");
    fflush(stdout);
  }

  // summarize
  printf("\n-------------\nFinal Result:\n");
  printf("Success: %u\n", totSuccess);
  printf("Failure: %u\n", imCount-totSuccess);
  printf("Success Rate: %g\n", (double)totSuccess/(double)imCount);
  printf("Average Stars: %g\n", avgStars/(double)imCount);
  printf("Total Solutions: %u\n", totSolCnt);
  printf("Total Good Solutions: %u\n", totGCnt);
  printf("Good Solution Rate: %g\n", (double)totGCnt/(double)totSolCnt);
  printf("-------------\n");

  printf("\n-------------\nList History:\n");
  for (i = 0; i < sysCount; i++)
  {
    printf("Sys %u: %u\n", i, listHist[i]);
  }
  printf("-------------\n");

  delete [] listHist;

  fprintf(out, "\nFinal Result:\n");
  fprintf(out, "Success: %u\n", totSuccess);
  fprintf(out, "Failure: %u\n", imCount-totSuccess);
  fprintf(out, "Success Rate: %g\n", (double)totSuccess/(double)imCount);
  fprintf(out, "Average Stars: %g\n", avgStars/(double)imCount);
  fprintf(out, "\n");

  printf("\n");
  printf("Output         : %s\n", output_file);
  printf("Catalog File   : %s\n", cat_file);
  printf("Index File     : %s\n", index_file);
  printf("Image File     : %s\n", name_file);
  printf("\n");
  fflush(stdout);

  fclose(out);

  exit(1);

  return 0;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
