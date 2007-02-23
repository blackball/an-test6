#ifndef MERCRENDER_H
#define MERCRENDER_H

#include "tilerender.h"
#include "merctree.h"

float* mercrender_file(char* fn, render_args_t* args);

void mercrender(merctree* m, render_args_t* args, float* fluximg);

#endif
