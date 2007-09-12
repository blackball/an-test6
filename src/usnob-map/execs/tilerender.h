#ifndef TILERENDER_H
#define TILERENDER_H

#include <cairo.h>

#include "starutil.h"
#include "mathutil.h"

struct render_args {
	// In degrees
	double ramin;
	double ramax;
	double decmin;
	double decmax;

	// In Mercator units: [0,1].
	double xmercmin;
	double xmercmax;
	double ymercmin;
	double ymercmax;

	// Mercator units per pixel in X,Y directions.
	double xmercperpixel;
	double ymercperpixel;

	double xpixelpermerc;
	double ypixelpermerc;

	int zoomlevel;

    // Don't render PNG; spit out raw floats.
    int makerawfloatimg;
    float* rawfloatimg;

	// Image size in pixels.
	int W;
	int H;

    // The current layer we're rendering.
    char* currentlayer;

	// Args for render_image:
	char* imagefn;
    char* imwcsfn;

	// Args for render_tycho:
	double colorcor;
	bool arc;
	bool arith;
	double gain;

	// Args for render_usnob:
	char* cmap;
	bool nopre; // don't use pre-rendered tiles (useful when you're trying to *make* pre-rendered tiles)

    char* version;

	// Args for render_rdls
	/*
	  char* rdlsfn;
	  int Nstars;
	  int fieldnum;
	*/
	pl* rdlsfns;
	pl* rdlscolors;
	il* Nstars;
	il* fieldnums;
	//dl* rdlslws;

	// Args for render_boundary
	char* wcsfn;
	double linewidth;
	double dashbox;
	bool zoomright;
	bool zoomdown;

	char* constfn;
};
typedef struct render_args render_args_t;

typedef int (*render_func_t)(unsigned char* dest_img, render_args_t* args);

//double xmerc2pixel()

// to RA in degrees
double pixel2ra(double pix, render_args_t* args);

// to DEC in degrees
double pixel2dec(double pix, render_args_t* args);

// Converts from RA in radians to Mercator X coordinate in [0, 1].
double ra2merc(double ra);

// Converts from Mercator X coordinate [0, 1] to RA in radians.
double merc2ra(double x);

// Converts from DEC in radians to Mercator Y coordinate in [0, 1].
double dec2merc(double dec);

// Converts from Mercator Y coordinate [0, 1] to DEC in radians.
double merc2dec(double y);

int xmerc2pixel(double x, render_args_t* args);

int ymerc2pixel(double y, render_args_t* args);

double xmerc2pixelf(double x, render_args_t* args);

double ymerc2pixelf(double y, render_args_t* args);

// RA in degrees
int ra2pixel(double ra, render_args_t* args);

// DEC in degrees
int dec2pixel(double dec, render_args_t* args);

// RA in degrees
double ra2pixelf(double ra, render_args_t* args);

// DEC in degrees
double dec2pixelf(double dec, render_args_t* args);

int in_image(int x, int y, render_args_t* args);

// Like in_image, but with a margin around the outside.
int in_image_margin(int x, int y, int margin, render_args_t* args);

// void put_pixel(int x, int y, uchar r, uchar g, uchar b, uchar a, render_args_t* args, uchar* img);

uchar* pixel(int x, int y, uchar* img, render_args_t* args);

void draw_segmented_line(double ra1, double dec1,
						 double ra2, double dec2,
						 int SEGS,
						 cairo_t* cairo, render_args_t* args);

// draw a line in Mercator space, handling wrap-around if necessary.
void draw_line_merc(double mx1, double my1, double mx2, double my2,
					cairo_t* cairo, render_args_t* args);

#endif
