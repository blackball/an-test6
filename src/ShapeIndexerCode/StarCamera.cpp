/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <math.h>
#include "StarCamera.hpp"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/* 
 * setDiff: Sets 'dest' to (a - b).
 * Assumes: 'a' and 'b' are 3-vectors.
 */
void setDiff(double *dest, double *a, double*b)
{
  dest[0] = a[0] - b[0];
  dest[1] = a[1] - b[1];
  dest[2] = a[2] - b[2];
}

/* -------------------------------------------------- */

/* 
 * normalize: Normalizes 'a'.
 * Assumes: 'a' is a 3-vector.
 */
void normalize(double *a)
{
  double bot;
  
  if ((!a[0]) && ((!a[1]) && (!a[2])))
  {
    return;
  }

  bot = 1.0/sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
  a[0] *= bot;
  a[1] *= bot;
  a[2] *= bot;
}

/* -------------------------------------------------- */

/* 
 * cross: Sets 'dest' to (a x b).
 * Assumes: 'a' and 'b' are 3-vectors.
 */
void cross(double *dest, double *a, double *b)
{
  dest[0] = (a[1]*b[2]) - (a[2]*b[1]);
  dest[1] = -(a[0]*b[2]) + (a[2]*b[0]);
  dest[2] = (a[0]*b[1]) - (a[1]*b[0]);
}

/* -------------------------------------------------- */

/* 
 * cross: Returns (a . b).
 * Assumes: 'a' and 'b' are 3-vectors.
 */
double dot(double *a, double *b)
{
  return (a[0]*b[0] + a[1]*b[1] + a[2]*b[2]);
}

/* -------------------------------------------------- */

/* 
 * StarCamera: Initializes the camera to a base state.
 * Assumes: <none>
 */
StarCamera::StarCamera()
{
  viewAngle   = DEF_VIEWANGLE;
  aspectRatio = DEF_ASPECT;
  nearDist    = DEF_NEAR;
  farDist     = DEF_FAR;

  set(0, 0, 0, 0, 0, 1, 0, 1, 0);
  updateLens();
}

/* -------------------------------------------------- */

/* 
 * setLens: Sets the lens view angle.
 * Assumes: 0 < angle <= 180.
 */
void StarCamera::setLens(double angle)
{
  viewAngle   = angle;
  updateLens();
}

/* -------------------------------------------------- */

/* 
 * set: Sets the camera orientation. Takes the eye position, 
 *      a reference direction, and a vector indicating the general 
 *     'up' direction of the camera.
 * Assumes: 'neye', 'nref', 'nup' are 3-vectors.
 */
void StarCamera::set(double *neye, double *nref, double *nup)
{
  set(neye[0], neye[1], neye[2], nref[0], nref[1], nref[2], nup[0], nup[1], nup[2]);
}

/* -------------------------------------------------- */

/* 
 * set: Sets the camera orientation. Takes the eye position, 
 *      a reference direction, and a vector indicating the general 
 *     'up' direction of the camera.
 * Assumes: <none>
 */
void StarCamera::set(double eyex, double eyey, double eyez, 
                     double refx, double refy, double refz,
                     double upx,  double upy,  double upz)
{
  double x, y, z;

  // record camera info
  eye[0] = eyex; eye[1] = eyey; eye[2] = eyez;
  ref[0] = refx; ref[1] = refy; ref[2] = refz;
  up[0]  = upx;  up[1]  = upy;  up[2]  = upz;

  // set distance from eye to reference point to 
  // avoid sqrt calculations later when we update
  // the reference point
  x = eyex-refx;
  y = eyey-refy;
  z = eyez-refz;
  refDist = sqrt(x*x+y*y+z*z);

  // construct camera basis.
  setDiff(k, ref, eye);
  cross(i, up, k);
  normalize(k);
  normalize(i);
  cross(j, k, i);
}

/* -------------------------------------------------- */

/* 
 * setEye: Set the eye position directly.
 * Assumes: <none>
 */
void StarCamera::setEye(double x, double y, double z)
{
  set(x, y, z, ref[0], ref[1], ref[2], up[0], up[1], up[2]);
}

/* -------------------------------------------------- */

/* 
 * setRef: Set the reference point directly.
 * Assumes: <none>
 */
void StarCamera::setRef(double x, double y, double z)
{
  set(eye[0], eye[1], eye[2], x, y, z, up[0], up[1], up[2]);
}

/* -------------------------------------------------- */

/* 
 * setUp: Set the 'up' direction directly.
 * Assumes: <none>
 */
void StarCamera::setUp(double x, double y, double z)
{
  set(eye[0], eye[1], eye[2], ref[0], ref[1], ref[2], x, y, z);
}

/* -------------------------------------------------- */

/* 
 * getEye: Gets the eye position of the camera.
 * Assumes: 'dest' is a 3-vector.
 */
void StarCamera::getEye(double *dest)
{
  dest[0] = eye[0];
  dest[1] = eye[1];
  dest[2] = eye[2];
}

/* -------------------------------------------------- */

/* 
 * getRef: Gets the reference point of the camera.
 * Assumes: 'dest' is a 3-vector.
 */
void StarCamera::getRef(double *dest)
{
  dest[0] = ref[0];
  dest[1] = ref[1];
  dest[2] = ref[2];
}

/* -------------------------------------------------- */

/* 
 * getUp: Gets the 'up' direction of the camera.
 * Assumes: 'dest' is a 3-vector.
 */
void StarCamera::getUp(double *dest)
{
  dest[0] = up[0];
  dest[1] = up[1];
  dest[2] = up[2];
}

/* -------------------------------------------------- */

/* 
 * getDirection: Gets the line of sight direction of the camera.
 * Assumes: 'dest' is a 3-vector.
 */
void StarCamera::getDirection(double *dest)
{
  dest[0] = k[0];
  dest[1] = k[1];
  dest[2] = k[2];
}

/* -------------------------------------------------- */

/* 
 * getBasis: Gets the basis for the camera.
 * Assumes: 'pi', 'pj', 'pk' and 3-vectors.
 */
void StarCamera::getBasis(double *pi, double *pj, double *pk)
{
  pi[0] = i[0];
  pi[1] = i[1];
  pi[2] = i[2];

  pj[0] = j[0];
  pj[1] = j[1];
  pj[2] = j[2];

  pk[0] = k[0];
  pk[1] = k[1];
  pk[2] = k[2];
}

/* -------------------------------------------------- */

/* 
 * getAngle: Returns the view angle of the camera.
 * Assumes: <none>
 */
double StarCamera::getAngle()
{
  return viewAngle;
}

/* -------------------------------------------------- */

/* 
 * findScreenPos: Finds the screen position of the point 'pos'
 *                in the camera's field of view. Returns FALSE32 and fills 
 *                'px and 'py' with MAX_DOUBLE if the point does not appear. 
 *                Otherwise, returns TRUE32 and places the x and y values 
 *                into 'px' and 'py' respectively.
 * Assumes: 'pos' is a 3-vector.
 */
BOOL32 StarCamera::findScreenPos(double *pos, double *px, double *py)
{
  double proj[3];
  double ray[3];
  double result;

  ray[0] = pos[0];
  ray[1] = pos[1];
  ray[2] = pos[2];

  // calc direction vector
  ray[0] -= eye[0];
  ray[1] -= eye[1];
  ray[2] -= eye[2];

  // determine ray direction rel. to camera by projecting
  // onto the orthonormal-basis for the camera
  proj[0] = ray[0]*i[0] + ray[1]*i[1] + ray[2]*i[2];
  proj[1] = ray[0]*j[0] + ray[1]*j[1] + ray[2]*j[2];
  // flip y-axis since the basis has this flipped
  proj[1] = -proj[1];
  proj[2] = ray[0]*k[0] + ray[1]*k[1] + ray[2]*k[2];

  // scale ray into the near plane
  if (proj[2] == 0)
  {
    result = -1;
  } else
  {
    result = nearDist/proj[2];
  }
  proj[0] *= result;
  proj[1] *= result;

  if (result >= 0)
  {
    // determine the final coords of the point on screen
    *px = 1.0 - ((right+proj[0])/(2.0*right));
    *py = 1.0 - ((top+proj[1])/(2.0*top));
    if (((*px < 0) || (*px > 1)) || ((*py < 0) || (*py > 1)))
    {
      result = -1;
    }
  } else
  {
    *px = MAX_DOUBLE;
    *py = MAX_DOUBLE;
  }

  return (result >= 0) ? TRUE32:FALSE32;
}

/* -------------------------------------------------- */

/* 
 * updateLens: Updates parameters concerning the lens.
 */
void StarCamera::updateLens()
{
  // create bounding info
  top    = nearDist*tan(RAD_CONV*viewAngle*0.5f);
  bottom = -top;
  right  = top*aspectRatio;
  left   = -right;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
