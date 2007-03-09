#ifndef TILERENDER_H
#define TILERENDER_H

#include "starutil.h"

#define max(a, b)  ((a)>(b)?(a):(b))
#define min(a, b)  ((a)<(b)?(a):(b))

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

	// Image size in pixels.
	int W;
	int H;

	// Args for render_image:
	char* imagefn;
	// Shared by render_boundary:
	char* wcsfn;

	// Args for render_tycho:
	double colorcor;
	bool arc;
	bool arith;
	double gain;

	// Args for render_rdls
	char* rdlsfn;
	int Nstars;
	int fieldnum;
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

int ra2pixel(double ra, render_args_t* args);

int dec2pixel(double dec, render_args_t* args);

int in_image(int x, int y, render_args_t* args);

// void put_pixel(int x, int y, uchar r, uchar g, uchar b, uchar a, render_args_t* args, uchar* img);

uchar* pixel(int x, int y, uchar* img, render_args_t* args);

#endif
