/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

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

#include "kdtree.h"
#include "bl.h"

typedef void (*max_bounds_function)(void* extra, kdtree_node_t* ynode, kdtree_node_t* xnode,
									double pruning_thresh, double* lower, double* upper);
typedef void (*max_start_of_results_function)(void* extra, kdtree_node_t* ynode,
											  double* pruning_thresh);
typedef void (*max_result_function)(void* extra, kdtree_node_t* ynode, kdtree_node_t* xnode,
									double* pruning_threshold, double lower, double upper);
typedef void (*max_end_of_results_function)(void* extra, kdtree_node_t* query);

struct dualtree_max_callbacks {
    max_bounds_function            bounds;
    void*                          bounds_extra;
    max_start_of_results_function  start_results;
    void*                          start_extra;
    max_result_function            result;
    void*                          result_extra;
    max_end_of_results_function    end_results;
    void*                          end_extra;
};
typedef struct dualtree_max_callbacks dualtree_max_callbacks;

void dualtree_max(kdtree_t* xtree, kdtree_t* ytree,
				  dualtree_max_callbacks* callbacks,
				  int go_to_leaves, int sort_on_upper_bound);


