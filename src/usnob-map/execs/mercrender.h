#ifndef MERCRENDER_H
#define MERCRENDER_H

#include "tilerender.h"
#include "merctree.h"

float* mercrender_file(char* fn, render_args_t* args);

void mercrender(merctree* m, render_args_t* args, float* fluximg);

#define RENDERSYMBOL_x   0
#define RENDERSYMBOL_o   1
#define RENDERSYMBOL_psf 2

int add_star(double xp, double yp, double rflux, double bflux, double nflux,
		float* fluximg, int rendersymbol, render_args_t* args);

#endif
