
%module util

%include <typemaps.i>


%{
/*
#include "index.h"
#include "codekd.h"
#include "starkd.h"
#include "qidxfile.h"
*/

// numpy.
#include <numpy/arrayobject.h>

#include "log.h"
#include "healpix.h"
//#include "anwcs.h"
#include "sip.h"
#include "sip_qfits.h"

#define true 1
#define false 0

%}

%init %{
      // numpy
      import_array();
%}

// Things in keywords.h (used by healpix.h)
#define Const
#define WarnUnusedResult
#define ASTROMETRY_KEYWORDS_H

void log_init(int level);

%apply double *OUTPUT { double *dx, double *dy };
%apply double *OUTPUT { double *ra, double *dec };

%include "healpix.h"

%apply double *OUTPUT { double *p_x, double *p_y, double *p_z };
%apply double *OUTPUT { double *p_ra, double *p_dec };
//%apply double *OUTPUT { double *xyz };

%typemap(in) double [ANY] (double temp[$1_dim0]) {
  int i;
  if (!PySequence_Check($input)) {
    PyErr_SetString(PyExc_ValueError,"Expected a sequence");
    return NULL;
  }
  if (PySequence_Length($input) != $1_dim0) {
    PyErr_SetString(PyExc_ValueError,"Size mismatch. Expected $1_dim0 elements");
    return NULL;
  }
  for (i = 0; i < $1_dim0; i++) {
    PyObject *o = PySequence_GetItem($input,i);
    if (PyNumber_Check(o)) {
      temp[i] = PyFloat_AsDouble(o);
    } else {
      PyErr_SetString(PyExc_ValueError,"Sequence elements must be numbers");      
      return NULL;
    }
  }
  $1 = temp;
}
%typemap(out) double [ANY] {
  int i;
  $result = PyList_New($1_dim0);
  for (i = 0; i < $1_dim0; i++) {
    PyObject *o = PyFloat_FromDouble($1[i]);
    PyList_SetItem($result,i,o);
  }
}

%typemap(in) double flatmatrix[ANY][ANY] (double temp[$1_dim0][$1_dim1]) {
    int i;
    if (!PySequence_Check($input)) {
        PyErr_SetString(PyExc_ValueError,"Expected a sequence");
        return NULL;
    }
    if (PySequence_Length($input) != ($1_dim0 * $1_dim1)) {
        PyErr_SetString(PyExc_ValueError,"Size mismatch. Expected $1_dim0*$1_dim1 elements");
        return NULL;
    }
    for (i = 0; i < ($1_dim0*$1_dim1); i++) {
        PyObject *o = PySequence_GetItem($input,i);
        if (PyNumber_Check(o)) {
            // FIXME -- is it dim0 or dim1?
            temp[i / $1_dim0][i % $1_dim0] = PyFloat_AsDouble(o);
        } else {
            PyErr_SetString(PyExc_ValueError,"Sequence elements must be numbers");      
            return NULL;
        }
    }
    $1 = temp;
}
%typemap(out) double flatmatrix[ANY][ANY] {
  int i;
  $result = PyList_New($1_dim0 * $1_dim1);
  for (i = 0; i < ($1_dim0)*($1_dim1); i++) {
      // FIXME -- dim0 or dim1?
      PyObject *o = PyFloat_FromDouble($1[i / $1_dim0][i % $1_dim0]);
      PyList_SetItem($result,i,o);
  }
 }


%apply double [ANY] { double crval[2] };
%apply double [ANY] { double crpix[2] };
%apply double flatmatrix[ANY][ANY] { double cd[2][2] };

%include "sip.h"
%include "sip_qfits.h"

%extend tan_t {
    tan_t(char* fn=NULL, int ext=0) {
        if (fn)
            return tan_read_header_file_ext(fn, ext, NULL);
        tan_t* t = (tan_t*)calloc(1, sizeof(tan_t));
        return t;
    }
    ~tan_t() { free($self); }
    double pixel_scale() { return tan_pixel_scale($self); }
	void xyzcenter(double *p_x, double *p_y, double *p_z) {
		double xyz[3];
		tan_pixelxy2xyzarr($self, 0.5+$self->imagew/2.0, 0.5+$self->imageh/2.0, xyz);
		*p_x = xyz[0];
		*p_y = xyz[1];
		*p_z = xyz[2];
		//pixelxy2xyz(0.5+$self->imagew/2.0, 0.5+$self->imageh/2.0,p_x, p_y, p_z);
	}
    void pixelxy2xyz(double x, double y, double *p_x, double *p_y, double *p_z) {
        double xyz[3];
        tan_pixelxy2xyzarr($self, x, y, xyz);
        *p_x = xyz[0];
        *p_y = xyz[1];
        *p_z = xyz[2];
    }
    void pixelxy2radec(double x, double y, double *p_ra, double *p_dec) {
        tan_pixelxy2radec($self, x, y, p_ra, p_dec);
    }
    int radec2pixelxy(double ra, double dec, double *p_x, double *p_y) {
        return tan_radec2pixelxy($self, ra, dec, p_x, p_y);
    }
    int xyz2pixelxy(double x, double y, double z, double *p_x, double *p_y) {
        double xyz[3];
        xyz[0] = x;
        xyz[1] = y;
        xyz[2] = z;
        return tan_xyzarr2pixelxy($self, xyz, p_x, p_y);
    }
	int write_to(const char* filename) {
		return tan_write_to_file($self, filename);
	}
	void set_crval(double ra, double dec) {
		$self->crval[0] = ra;
		$self->crval[1] = dec;
	}
	void set_crpix(double x, double y) {
		$self->crpix[0] = x;
		$self->crpix[1] = y;
	}
	void set_imagesize(double w, double h) {
		$self->imagew = w;
		$self->imageh = h;
	}


 };


%inline %{

    static int tan_numpy_pixelxy2radec(tan_t* tan, PyObject* npx, PyObject* npy, 
                                       PyObject* npra, PyObject* npdec, int reverse) {
                                        
        int i, N;
        double *x, *y, *ra, *dec;
        
        if (PyArray_NDIM(npx) != 1) {
            PyErr_SetString(PyExc_ValueError, "arrays must be one-dimensional");
            return -1;
        }
        if (PyArray_TYPE(npx) != PyArray_DOUBLE) {
            PyErr_SetString(PyExc_ValueError, "array must contain doubles");
            return -1;
        }
        N = PyArray_DIM(npx, 0);
        if ((PyArray_DIM(npy, 0) != N) ||
            (PyArray_DIM(npra, 0) != N) ||
            (PyArray_DIM(npdec, 0) != N)) {
            PyErr_SetString(PyExc_ValueError, "arrays must be the same size");
            return -1;
        }
        x = PyArray_GETPTR1(npx, 0);
        y = PyArray_GETPTR1(npy, 0);
        ra = PyArray_GETPTR1(npra, 0);
        dec = PyArray_GETPTR1(npdec, 0);
        if (!reverse) {
                for (i=0; i<N; i++)
                    tan_pixelxy2radec(tan, x[i], y[i], ra+i, dec+i);
        } else {
                for (i=0; i<N; i++)
                    tan_radec2pixelxy(tan, ra[i], dec[i], x+i, y+i);
        }
        return 0;
    }

    static int tan_numpy_xyz2pixelxy(tan_t* tan, PyObject* npxyz,
		   PyObject* npx, PyObject* npy) {
        int i, N;
        double *x, *y; //, *xyz;
        
        if (PyArray_NDIM(npx) != 1) {
            PyErr_SetString(PyExc_ValueError, "arrays must be one-dimensional");
            return -1;
        }
        if (PyArray_TYPE(npx) != PyArray_DOUBLE) {
            PyErr_SetString(PyExc_ValueError, "array must contain doubles");
            return -1;
        }
        N = PyArray_DIM(npx, 0);
        if ((PyArray_DIM(npy, 0) != N) ||
            (PyArray_DIM(npxyz, 0) != N) ||
            (PyArray_DIM(npxyz, 1) != 3)) {
            PyErr_SetString(PyExc_ValueError, "arrays must be the same size");
            return -1;
        }
        x = PyArray_GETPTR1(npx, 0);
        y = PyArray_GETPTR1(npy, 0);
        //xyz = PyArray_GETPTR2(npxyz, 0, 0);
		for (i=0; i<N; i++)
		   tan_xyzarr2pixelxy(tan, PyArray_GETPTR2(npxyz, i, 0),
		   						   x+i, y+i);
        return 0;
    }




%}

%pythoncode %{
import numpy as np

def tan_t_tostring(self):
    return ('Tan: crpix (%.1f, %.1f), crval (%g, %g), cd (%g, %g, %g, %g), image %g x %g' %
            (self.crpix[0], self.crpix[1], self.crval[0], self.crval[1],
             self.cd[0], self.cd[1], self.cd[2], self.cd[3],
             self.imagew, self.imageh))
tan_t.__str__ = tan_t_tostring

def tan_t_pixelxy2radec_any(self, x, y):
    if np.iterable(x) or np.iterable(y):
        x = np.atleast_1d(x).astype(float)
        y = np.atleast_1d(y).astype(float)
        r = np.empty(len(x))
        d = np.empty(len(x))
        tan_numpy_pixelxy2radec(self.this, x, y, r, d, 0)
        return r,d
    else:
        return self.pixelxy2radec_single(x, y)
tan_t.pixelxy2radec_single = tan_t.pixelxy2radec
tan_t.pixelxy2radec = tan_t_pixelxy2radec_any

def tan_t_radec2pixelxy_any(self, r, d):
    if np.iterable(r) or np.iterable(d):
        r = np.atleast_1d(r).astype(float)
        d = np.atleast_1d(d).astype(float)
        x = np.empty(len(r))
        y = np.empty(len(r))
        # This looks like a bug (pixelxy2radec rather than radec2pixel)
        # but it isn't ("reverse = 1")
        tan_numpy_pixelxy2radec(self.this, x, y, r, d, 1)
        return x,y
    else:
        good,x,y = self.radec2pixelxy_single(r, d)
        return x,y
tan_t.radec2pixelxy_single = tan_t.radec2pixelxy
tan_t.radec2pixelxy = tan_t_radec2pixelxy_any


def tan_t_radec_bounds(self):
    W,H = self.imagew, self.imageh
    r,d = self.pixelxy2radec([1, W, W, 1], [1, 1, H, H])
    return (r.min(), r.max(), d.min(), d.max())
tan_t.radec_bounds = tan_t_radec_bounds    

def tan_t_xyz2pixelxy_any(self, xyz):
    if np.iterable(xyz[0]):
        xyz = np.atleast_2d(xyz).astype(float)
        (N,three) = xyz.shape
        assert(three == 3)
        x = np.empty(N)
        y = np.empty(N)
        # This looks like a bug (pixelxy2radec rather than radec2pixel)
        # but it isn't ("reverse = 1")
        tan_numpy_xyz2pixelxy(self.this, xyz, x, y)
        return x,y
    else:
        good,x,y = self.xyz2pixelxy_single(*xyz)
        return x,y
tan_t.xyz2pixelxy_single = tan_t.xyz2pixelxy
tan_t.xyz2pixelxy = tan_t_xyz2pixelxy_any

Tan = tan_t
%} 


 /*
%include "anwcs.h"
  %extend anwcs_t {
  %}
  */
