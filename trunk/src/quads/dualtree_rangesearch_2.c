#include <string.h>
#include "dualtree_rangesearch_2.h"
#include "dualtree_2.h"

double RANGESEARCH2_NO_LIMIT = 1.12345e308;

struct rs2_params {
	kdtree_t* xtree;
	kdtree_t* ytree;

    // radius-squared of the search range.
    double mindistsq;
    double maxdistsq;

    // are we using the min/max limit?
    int usemin:1;
    int usemax:1;

    result_callback_2 user_callback;
    void* user_callback_param;
};
typedef struct rs2_params rs2_params;

bool rs2_within_range(void* params, kdtree_node_t* search, kdtree_node_t* query);
void rs2_handle_result(void* params, kdtree_node_t* search, kdtree_node_t* query);
void rs2_handle_leaves(rs2_params* p, kdtree_node_t* search, kdtree_node_t* ynode);

void dualtree_rangesearch_2(kdtree_t* xtree, kdtree_t* ytree,
							double mindist, double maxdist,
							result_callback_2 callback,
							void* param) {
    // dual-tree search callback functions
    dualtree_callbacks_2 callbacks;
    rs2_params params;

    memset(&callbacks, 0, sizeof(dualtree_callbacks_2));
    callbacks.decision = rs2_within_range;
    callbacks.decision_extra = &params;
    callbacks.result = rs2_handle_result;
    callbacks.result_extra = &params;

    // set search params
    if (mindist == RANGESEARCH2_NO_LIMIT) {
		params.usemin = 0;
    } else {
		params.usemin = 1;
		params.mindistsq = mindist * mindist;
    }

    if (maxdist == RANGESEARCH2_NO_LIMIT) {
		params.usemax = 0;
    } else {
		params.usemax = 1;
		params.maxdistsq = maxdist * maxdist;
    }

    params.user_callback = callback;
    params.user_callback_param = param;
	params.xtree = xtree;
	params.ytree = ytree;

    dualtree_search_2(xtree, ytree, &callbacks);
}

bool rs2_within_range(void* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode) {
    rs2_params* p = (rs2_params*)vparams;
    double mindistsq;
    double maxdistsq;

    if (p->usemin) {
		maxdistsq = kdtree_node_node_maxdist2(p->xtree, xnode, p->ytree, ynode);
		if (maxdistsq < p->mindistsq)
			return FALSE;
    }
    if (p->usemax) {
		mindistsq = kdtree_node_node_mindist2(p->xtree, xnode, p->ytree, ynode);
		if (mindistsq > p->maxdistsq)
			return FALSE;
    }
    return TRUE;
}

void rs2_handle_leaves(rs2_params* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode) {
    // go through all pairs of points in this pair of leaf nodes, checking
    // that each pair's distance lies within the required range.  Call the
    // user's callback function on each satisfying pair.
	int xl, xr, yl, yr;
	int x, y;
    rs2_params* p = (rs2_params*)vparams;
	int D = p->ytree->ndim;

	xl = xnode->l;
	xr = xnode->r;
	yl = ynode->l;
	yr = ynode->r;

	for (y=yl; y<=yr; y++) {
		double* py = p->ytree->data + y * D;
		int iy = p->ytree->perm[y];
		for (x=xl; x<=xr; x++) {
			double d2;
			double* px;
			int ix;
			px = p->xtree->data + x * D;
			d2 = distsq(px, py, D);
			if ((p->usemin) && (d2 < p->mindistsq))
				continue;
			if ((p->usemax) && (d2 > p->maxdistsq))
				continue;
			ix = p->xtree->perm[x];
			p->user_callback(p->user_callback_param, ix, iy, d2);
		}
	}
}

void rs2_handle_result(void* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode) {
    // this function gets called on the children of accepted nodes, so this
    // pair of nodes may not be acceptable.
    rs2_params* p = (rs2_params*)vparams;

    if (!rs2_within_range(vparams, xnode, ynode)) {
		return;
    }

    // either node can be a non-leaf.  in this case, recurse until we
    // hit leaves.
    if (!kdtree_node_is_leaf(p->xtree, xnode)) {
		rs2_handle_result(vparams, kdtree_get_child1(p->xtree, xnode), ynode);
		rs2_handle_result(vparams, kdtree_get_child2(p->xtree, xnode), ynode);
		return;
    }

    if (!kdtree_node_is_leaf(p->ytree, ynode)) {
		rs2_handle_result(vparams, xnode, kdtree_get_child1(p->ytree, ynode));
		rs2_handle_result(vparams, xnode, kdtree_get_child2(p->ytree, ynode));
		return;
    }

    // okay, here we are, we've got a pair of valid leaf nodes.
    rs2_handle_leaves((rs2_params*)vparams, xnode, ynode);
}

