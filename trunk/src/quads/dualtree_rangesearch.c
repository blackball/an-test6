#include "dualtree_rangesearch.h"
#include "dualtree.h"

double RANGESEARCH_NO_LIMIT = 1.12345e308;

struct rs_params {
    // radius-squared of the search range.
    double mindistsq;
    double maxdistsq;

    // are we using the min/max limit?
    int usemin:1;
    int usemax:1;

    result_callback user_callback;
    void* user_callback_param;
};
typedef struct rs_params rs_params;

bool rs_within_range(void* params, node* search, node* query);
void rs_handle_result(void* params, node* search, node* query);
void rs_handle_leaves(rs_params* p, node* search, node* ynode);

void dualtree_rangesearch(kdtree* xtree, kdtree* ytree,
			  double mindist, double maxdist,
			  result_callback callback,
			  void* param) {
    // dual-tree search callback functions
    dualtree_callbacks callbacks;
    rs_params params;

    memset(&callbacks, 0, sizeof(dualtree_callbacks));
    callbacks.decision = rs_within_range;
    callbacks.decision_extra = &params;
    callbacks.result = rs_handle_result;
    callbacks.result_extra = &params;

    // set search params
    //if (isnan(mindist)) {
    if (mindist == RANGESEARCH_NO_LIMIT) {
	params.usemin = 0;
    } else {
	params.usemin = 1;
	params.mindistsq = mindist * mindist;
    }

    //if (isnan(maxdist)) {
    if (maxdist == RANGESEARCH_NO_LIMIT) {
	params.usemax = 0;
    } else {
	params.usemax = 1;
	params.maxdistsq = maxdist * maxdist;
    }

    params.user_callback = callback;
    params.user_callback_param = param;

    dualtree_search(xtree, ytree, &callbacks);
}

bool rs_within_range(void* vparams, node* xnode, node* ynode) {
    rs_params* p = (rs_params*)vparams;
    double mindistsq;
    double maxdistsq;

    if (p->usemin) {
	maxdistsq = hrect_hrect_max_dsqd(xnode->box, ynode->box);
	if (maxdistsq < p->mindistsq)
	    return FALSE;
    }
    if (p->usemax) {
	mindistsq = hrect_hrect_min_dsqd(xnode->box, ynode->box);
	if (mindistsq > p->maxdistsq)
	    return FALSE;
    }
    return TRUE;
}

void rs_handle_leaves(rs_params* p, node* xnode, node* ynode) {
    // go through all pairs of points in this pair of leaf nodes, checking
    // that each pair's distance lies within the required range.  Call the
    // user's callback function on each satisfying pair.
    int i, j, NX, NY;
    int ix, iy;

    dyv_array* ay = ynode->points;
    dyv_array* ax = xnode->points;
    ivec* ivecy = ynode->pindexes;
    ivec* ivecx = xnode->pindexes;
    NY = ynode->num_points;
    NX = xnode->num_points;

    for (i=0; i<NY; i++) {
	dyv* py = dyv_array_ref(ay, i);
	iy = ivec_ref(ivecy, i);
	for (j=0; j<NX; j++) {
	    double d2;
	    dyv* px = dyv_array_ref(ax, j);
	    ix = ivec_ref(ivecx, j);
	    d2 = dyv_dyv_dsqd(px, py);
	    if ((p->usemin) && (d2 < p->mindistsq))
		continue;
	    if ((p->usemax) && (d2 > p->maxdistsq))
		continue;
	    p->user_callback(p->user_callback_param, ix, iy, d2);
	}
    }
}

void rs_handle_result(void* vparams, node* xnode, node* ynode) {
    // this function gets called on the children of accepted nodes, so this
    // pair of nodes may not be acceptable.
    if (!rs_within_range(vparams, xnode, ynode)) {
	return;
    }

    // either node can be a non-leaf.  in this case, recurse until we
    // hit leaves.
    if (!node_is_leaf(xnode)) {
	rs_handle_result(vparams, xnode->child1, ynode);
	rs_handle_result(vparams, xnode->child2, ynode);
	return;
    }

    if (!node_is_leaf(ynode)) {
	rs_handle_result(vparams, xnode, ynode->child1);
	rs_handle_result(vparams, xnode, ynode->child2);
	return;
    }

    // okay, here we are, we've got a pair of valid leaf nodes.
    rs_handle_leaves((rs_params*)vparams, xnode, ynode);
}

