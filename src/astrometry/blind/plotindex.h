/*
  This file is part of the Astrometry.net suite.
  Copyright 2010 Dustin Lang.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/
#ifndef PLOTINDEX_H
#define PLOTINDEX_H

#include "plotstuff.h"
#include "bl.h"

struct plotindex_args {
	pl* indexes;
	bool stars;
	bool quads;
};
typedef struct plotindex_args plotindex_t;

void* plot_index_init(plot_args_t* args);

int plot_index_command(const char* command, const char* cmdargs,
					plot_args_t* args, void* baton);

int plot_index_plot(const char* command, cairo_t* cr,
					plot_args_t* args, void* baton);

void plot_index_free(plot_args_t* args, void* baton);

extern const plotter_t plotter_index;

#endif
