/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <stdio.h>
#include <MyTypes.h>
#include <MathExt.h>
#include <StringExt.h>
#include "StarCamera.hpp"
#include "StarImage.hpp"
#include "StarList.hpp"
#include "StarShapeSys.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// if defined, images are centered on the first k stars
#define CENTER_ON_STAR

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * Produces a set of random images from the provided catalog.
 * Usage: <im_count> <prefix> <dest_dir> <cat_file> <dist_file>
 * <view_angle_min> <view_angle_max> <drop-out rate> <jitter>
 * <im_count>      : number of image to generate
 * <prefix>        : image file name prefix
 * <dest_dir>      : destination directory for all files
 * <cat_file>      : catalog file to use
 * <dist_file>     : distractor file to use
 * <view_angle_min>: minimum view angle to use
 * <view_angle_max>: maximum view angle to use
 * <drop-out rate> : percentage of stars to drop out
 * <jitter>        : max jitter in [0,1]
 * Ex: 100 im_ TempDat catalog2.bin dist1.000.bin 1.0 1.4 0 0
 */
int main(int argc, char *argv[])
{
  DWORD im_count;
  char *dest_dir;
  char *prefix;
  char *cat_file;
  char *dist_file;
  char  out_file[4096];
  double view_angle_min;
  double view_angle_max;
  double dir[3];
  double up[3];
  double ti[3], tj[3], tk[3];
  double dif;
  double jitter;
  double drop_out;
  BOOL32 iGood;
  StarCamera cam;
  StarImage  im;
  StarShapeSys shapeSys;
  StarList cList, dList;
  FILE *out_list, *out_data;
  char fname[4096];
  DWORD i;

  // output usage as needed
  if (argc < 10)
  {
    printf("\n");
    printf("Usage: <im_count> <prefix> <dest_dir> <cat_file> <dist_file>\n");
    printf("       <view_angle_min> <view_angle_max> <drop-out rate> <jitter>\n");
    printf("<im_count>      : number of image to generate\n");
    printf("<prefix>        : image file name prefix\n");
    printf("<dest_dir>      : destination directory for all files\n");
    printf("<cat_file>      : catalog file to use\n");
    printf("<dist_file>     : distractor file to use\n");
    printf("<view_angle_min>: minimum view angle to use\n");
    printf("<view_angle_max>: maximum view angle to use\n");
    printf("<drop-out rate> : percentage of stars to drop out\n");
    printf("<jitter>        : max jitter in [0,1]\n");
    printf("\n");
    return 0;
  }

  // read params
  iGood = TRUE32;
  if (sscanf(argv[1], "%u", &im_count) < 1)
  {
    iGood = FALSE32;
  }
  prefix    = argv[2];
  dest_dir  = argv[3];
  cat_file  = argv[4];
  dist_file = argv[5];
  if ((sscanf(argv[6], "%lf", &view_angle_min) < 1) || (view_angle_min <= 0))
  {
    iGood = FALSE32;
  }
  if ((sscanf(argv[7], "%lf", &view_angle_max) < 1) || (view_angle_max <= 0))
  {
    iGood = FALSE32;
  }
  if ((sscanf(argv[8], "%lf", &drop_out) < 1) || ((drop_out < 0) || (drop_out > 1)))
  {
    iGood = FALSE32;
  }
  if ((sscanf(argv[9], "%lf", &jitter) < 1) || ((jitter < 0) || (jitter > 1)))
  {
    iGood = FALSE32;
  }

  // quit on param error
  if (iGood == FALSE32)
  {
    printf("Error parsing command line\n");
    return -1;
  }

  // open star data
  if (cList.Open(cat_file) == FALSE32)
  {
    printf("Error opening catalog file: %s\n", cat_file);
    return -1;
  }
  if (dList.Open(dist_file) == FALSE32)
  {
    printf("Error opening distractor file: %s\n", dist_file);
    return -1;
  }

  // build image list file name and create the file
  sprintf(fname, "%s_image.lst", prefix);
  if (CombineFilePath(out_file, dest_dir, fname, 4096) == FALSE32)
  {
    printf("Error: unable to create image list name -- too long\n");
    return -1;
  }
  out_list = fopen(out_file, "wb");
  if (out_list == NULL)
  {
    printf("Error creating file: %s\n", fname);
    return -1;
  }
  // build image summary file name and create the file
  sprintf(fname, "%s_image.dat", prefix);
  if (CombineFilePath(out_file, dest_dir, fname, 4096) == FALSE32)
  {
    printf("Error: unable to create image list name -- too long\n");
    return -1;
  }
  out_data = fopen(out_file, "wb");
  if (out_data == NULL)
  {
    printf("Error creating file: %s\n", fname);
    return -1;
  }

  dif = view_angle_max - view_angle_min;
  // build all images
  for (i = 0; i < im_count; i++)
  {
    // set random lens
    cam.setLens(view_angle_min + randD()*dif);

#ifdef CENTER_ON_STAR
    // use actual star positions
    dir[0] = cList.List3D[0][i];
    dir[1] = cList.List3D[1][i];
    dir[2] = cList.List3D[2][i];
#else
    // set random camera orientation
    randSphereD(dir);
#endif

    // make sure the up orientation is valid
    do
    {
      randSphereD(up);
      cross(ti, up, dir);
    } while (((ti[0] == 0) && (ti[1] == 0)) && (ti[2] == 0));

    // set orientation
    cam.set(0, 0, 0,
            dir[0], dir[1], dir[2],
            up[0],  up[1],  up[2]);
    
    // save image, record name
    sprintf(fname, "%s%.9u.im", prefix, i);
    if (CombineFilePath(out_file, dest_dir, fname, 4096) == FALSE32)
    {
      printf("Error: complete file name too long\n");
    } else
    {
      // generate image
      printf("\nGenerating: %s\n", out_file);
      shapeSys.StarsGenIm(&im, &cList, &dList, &cam, jitter, drop_out);

      // save image
      im.SaveAsText(out_file);
      fprintf(out_list, "%s\n", out_file);

      cam.getBasis(ti, tj, tk);
      fprintf(out_data, "%u %f %f %f    %f %f %f    %f %f %f    %f\n", 
                        i, 
                        ti[0], ti[1], ti[2],
                        tj[0], tj[1], tj[2],
                        tk[0], tk[1], tk[2],
                        (float)cam.getAngle()
              );
    }
  }

  fclose(out_list);
  fclose(out_data);

  return 0;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
