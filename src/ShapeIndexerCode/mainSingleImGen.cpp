/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <stdio.h>
#include <math.h>
#include "StarShapeSys.hpp"
#include "StarList.hpp"
#include "StarImage.hpp"
#include "StarCamera.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * Generate a single image from the provided catalogs.
 * Usage: <catalog> <dist_cat> <image_output> <RA> <DEC> <view_angle>
 *        <drop_outs> <jitter>
 * <catalog>      : the catalog to generate the image from
 * <dist_cat>     : the catalog of distractor stars
 * <image_output> : file to write the image to
 * <RA>           : RA of the image
 * <DEC>          : Dec of the image
 * <view_angle>   : field of view for the image
 * <drop_out>     : precentage star drop-out
 * <jitter>       : star position jitter
 * Ex: cat.part1.bin dist1.000.bin im.tychopart1.txt 0 90 0.5 0 0
 */
int main(int argc, char *argv[])
{
  StarShapeSys lsys;
  StarList    sList, dList;
  StarImage   im;
  StarCamera  cam;
  double ra, dec, view_angle, jitter, drop_out;
  char  *cat_name, *dist_name, *im_name;
  double dir[3], up[3];

  // output usage
  if (argc < 9)
  {
    printf("\n");
    printf("Usage: <catalog> <dist_cat> <image_output> <RA> <DEC> <view_angle>\n");
    printf("       <drop_outs> <jitter>\n");
    printf("<catalog>      : the catalog to generate the image from\n");
    printf("<dist_cat>     : the catalog of distractor stars\n");
    printf("<image_output> : file to write the image to\n");
    printf("<RA>           : RA of the image\n");
    printf("<DEC>          : Dec of the image\n");
    printf("<view_angle>   : field of view for the image\n");
    printf("<drop_out>     : precentage star drop-out\n");
    printf("<jitter>       : star position jitter\n");
    printf("\n");
    return 0;
  }
  // grab parameters
  cat_name  = argv[1];
  dist_name = argv[2];
  im_name   = argv[3];
  if (sscanf(argv[4], "%lg", &ra) < 1)
  {
    printf("Unable to read RA value\n");
    return -1;
  }
  if (sscanf(argv[5], "%lg", &dec) < 1)
  {
    printf("Unable to read DEC value\n");
    return -1;
  }
  if (sscanf(argv[6], "%lg", &view_angle) < 1)
  {
    printf("Unable to read field of view value\n");
    return -1;
  }
  if (sscanf(argv[7], "%lg", &drop_out) < 1)
  {
    printf("Unable to read drop-out value\n");
    return -1;
  }
  if (sscanf(argv[8], "%lg", &jitter) < 1)
  {
    printf("Unable to read jitter value\n");
    return -1;
  }

  if (sList.Open(cat_name) == FALSE32)
  {
    printf("Unable to open catalog: %s\n", cat_name);
    return -1;
  }
  if (dList.Open(dist_name) == FALSE32)
  {
    printf("Unable to open distractor catalog: %s\n", dist_name);
    return -1;
  }

  // set view direction 
  dir[0] = cos(ra*RAD_CONV)*sin(dec*RAD_CONV);
  dir[1] = sin(ra*RAD_CONV)*sin(dec*RAD_CONV);
  dir[2] = cos(dec*RAD_CONV);
  up[0]  = 0;
  up[1]  = 0;
  up[2]  = -1;

  // set orientation
  cam.set(0, 0, 0,
          dir[0], dir[1], dir[2],
          up[0],  up[1],  up[2]);
  cam.setLens(view_angle);

  // generate image
  lsys.StarsGenIm(&im, &sList, &dList, &cam, jitter, drop_out);

  // save image
  if (im.SaveAsText(im_name) == FALSE32)
  {
    printf("Unable to save image: %s\n", im_name);
    return -1;
  }

  // finished
  printf("Done\n");

  return 0;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
