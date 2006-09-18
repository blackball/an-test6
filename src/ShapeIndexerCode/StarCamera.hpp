#ifndef __STARCAMERA_HPP__
#define __STARCAMERA_HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * These defines should not require any adjustment. Only the view angle
 * is of any real relevance here.
 */

// Default view angle for the camera.
#define DEF_VIEWANGLE   45
// Default aspect ratio.
#define DEF_ASPECT      1.0
// Default near distance.
#define DEF_NEAR        1.0
// Default far distance.
#define DEF_FAR         10.0

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/* 
 * StarCamera: A class reprsenting a standard perspective camera. Used for generating
 *             simulated images of stars.
 */
class StarCamera
{
protected:
  // Camera view data.
  double i[3], j[3], k[3]; // Camera basis
  double eye[3];           // Eye position
  double ref[3];           // View reference point
  double up[3];            // Up direction for camera
  double refDist;          // Distance from eye to reference point

  // camera perspective data.
  double viewAngle, aspectRatio, nearDist, farDist;
  double top, bottom, left, right;

public:
  // Initializes the camera to a base state. 
  StarCamera();

  // Sets the lens view angle.
  void   setLens(double angle = DEF_VIEWANGLE);
  // Sets the camera orientation. Takes the eye position, a reference direction, and a vector
  // indicating the general 'up' direction of the camera.
  void   set(double *neye, double *nref, double *nup);
  // Same as the previous 'set' function, except values are passes individually rather than via
  // 3-vectors.
  void   set(double eyex, double eyey, double eyez,
             double refx, double refy, double refz,
             double upx,  double upy,  double upz);
  // Set the eye position directly.
  void   setEye(double x, double y, double z);
  // Set the reference point directly.
  void   setRef(double x, double y, double z);
  // Set the 'up' vector direcly.
  void   setUp(double x, double y, double z);

  // Gets the eye position.
  void   getEye(double *eye);
  // Gets the reference point.
  void   getRef(double *ref);
  // Gets the 'up' direction
  void   getUp(double *up);
  // Gets the normalized directino of sight.
  void   getDirection(double *dir);
  // Gets the camera basis.
  void   getBasis(double *pi, double *pj, double *pk);
  // Gets the camera view angle.
  double getAngle();

  // Determines the image position of a 3-vector in the 2D view of the camera. Returned
  // values are in [0,1]. Returns FALSE32 if the point does not appear in the view, and TRUE32
  // otherwise.
  BOOL32 findScreenPos(double *pos, double *px, double *py);

protected:
  void   updateLens();
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * Extra math functions used by the camera, made available here for general usage.
 */

// Assumes a and b are 3-vectors. Sets dest to (a - b).
void   setDiff(double *dest, double *a, double*b);
// Assumes a is a 3-vector. Normalizes a.
void   normalize(double *a);
// Assumes a and b are 3-vectors. Sets dest to (a x b).
void   cross(double *dest, double *a, double *b);
// Assumes a and b are 3-vectors. Returns (a . b).
double dot(double *a, double *b);

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
