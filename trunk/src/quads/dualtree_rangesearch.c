#include <string.h>
#include "dualtree_rangesearch.h"
#include "dualtree.h"
#include "mathutil.h"

double RANGESEARCH_NO_LIMIT = 1.12345e308;

struct rs_params {
	kdtree_t* xtree;
	kdtree_t* ytree;

    // radius-squared of the search range.
    double mindistsq;
    double maxdistsq;

    // are we using the min/max limit?
    int usemin:1;
    int usemax:1;

	// for "search"
    result_callback user_callback;
    void* user_callback_param;

	// for "count"
	int* counts;
};
typedef struct rs_params rs_params;

bool rs_within_range(void* params, kdtree_node_t* search, kdtree_node_t* query);
void rs_handle_result(void* params, kdtree_node_t* search, kdtree_node_t* query);

bool rc_should_recurse(void* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode);
void rc_handle_result(void* params, kdtree_node_t* search, kdtree_node_t* query);

void rc_self_handle_result(void* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode);
bool rc_self_should_recurse(void* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode);


void dualtree_rangecount(kdtree_t* xtree, kdtree_t* ytree,
						 double mindist, double maxdist,
						 int* counts) {
    dualtree_callbacks callbacks;
    rs_params params;

    memset(&callbacks, 0, sizeof(dualtree_callbacks));
	if (xtree == ytree) {
		callbacks.decision = rc_self_should_recurse;
		callbacks.result = rc_self_handle_result;
	} else {
		callbacks.decision = rc_should_recurse;
		callbacks.result = rc_handle_result;
	}
    callbacks.decision_extra = &params;
    callbacks.result_extra = &params;

    // set search params
    if (mindist == RANGESEARCH_NO_LIMIT) {
		params.usemin = 0;
    } else {
		params.usemin = 1;
		params.mindistsq = mindist * mindist;
    }
    if (maxdist == RANGESEARCH_NO_LIMIT) {
		params.usemax = 0;
    } else {
		params.usemax = 1;
		params.maxdistsq = maxdist * maxdist;
    }
	params.xtree = xtree;
	params.ytree = ytree;
	params.counts = counts;

    dualtree_search(xtree, ytree, &callbacks);

	if (xtree == ytree) {
		// we'll double-count 
	}
}


void dualtree_rangesearch(kdtree_t* xtree, kdtree_t* ytree,
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
    if (mindist == RANGESEARCH_NO_LIMIT) {
		params.usemin = 0;
    } else {
		params.usemin = 1;
		params.mindistsq = mindist * mindist;
    }

    if (maxdist == RANGESEARCH_NO_LIMIT) {
		params.usemax = 0;
    } else {
		params.usemax = 1;
		params.maxdistsq = maxdist * maxdist;
    }

    params.user_callback = callback;
    params.user_callback_param = param;
	params.xtree = xtree;
	params.ytree = ytree;

    dualtree_search(xtree, ytree, &callbacks);
}

bool rs_within_range(void* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode) {
    rs_params* p = (rs_params*)vparams;
    double mindistsq;
    double maxdistsq;

    if (p->usemax) {
		mindistsq = kdtree_node_node_mindist2(p->xtree, xnode, p->ytree, ynode);
		if (mindistsq > p->maxdistsq)
			return FALSE;
    }
    if (p->usemin) {
		maxdistsq = kdtree_node_node_maxdist2(p->xtree, xnode, p->ytree, ynode);
		if (maxdistsq < p->mindistsq)
			return FALSE;
    }
    return TRUE;
}

void rs_handle_result(void* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode) {
    // go through all pairs of points in this pair of nodes, checking
    // that each pair's distance lies within the required range.  Call the
    // user's callback function on each satisfying pair.
	int xl, xr, yl, yr;
	int x, y;
    rs_params* p = (rs_params*)vparams;
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
			if ((p->usemax) && (d2 > p->maxdistsq))
				continue;
			if ((p->usemin) && (d2 < p->mindistsq))
				continue;
			ix = p->xtree->perm[x];
			p->user_callback(p->user_callback_param, ix, iy, d2);
		}
	}
}

bool rc_should_recurse(void* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode) {
    rs_params* p = (rs_params*)vparams;
    double mindistsq;
    double maxdistsq;
	//bool allinrange = TRUE;

	// does the bounding box partly overlap the desired range?
    if (p->usemax) {
		mindistsq = kdtree_node_node_mindist2_bailout(p->xtree, xnode, p->ytree, ynode, p->maxdistsq);
		if (mindistsq > p->maxdistsq)
			return FALSE;
    }
    if (p->usemin) {
		maxdistsq = kdtree_node_node_maxdist2(p->xtree, xnode, p->ytree, ynode);
		if (maxdistsq < p->mindistsq)
			return FALSE;
    }

	/*
	  ;
	  // HACK - it's not clear that it's advantageous to do this here...
	  // (NOTE, if you decide to uncomment this, be sure to fix
	  /    rc_self_should_recurse, since the action to take is different.)
	  // is the bounding box fully within the desired range?
	  if (p->usemin) {
	  // compute min bound if it hasn't already been...
	  if (!p->usemax)
	  mindistsq = kdtree_node_node_mindist2(p->xtree, xnode, p->ytree, ynode);
	  if (mindistsq < p->mindistsq)
	  allinrange = FALSE;
	  }
	  if (allinrange && p->usemax) {
	  if (!p->usemin)
	  maxdistsq = kdtree_node_node_maxdist2(p->xtree, xnode, p->ytree, ynode);
	  if (maxdistsq > p->maxdistsq)
	  allinrange = FALSE;
	  }
	  if (allinrange) {
	  // we can stop at this pair of nodes; no need to recurse any further.
	  // for each Y point, increment its counter by the number of points in the X node.
	  int NX, yl, yr, y;
	  NX = kdtree_node_npoints(xnode);
	  yl = ynode->l;
	  yr = ynode->r;
	  for (y=yl; y<=yr; y++) {
	  int iy = p->ytree->perm[y];
	  p->counts[iy] += NX;
	  }
	  return FALSE;
	  }
	*/

    return TRUE;
}

void rc_handle_result(void* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode) {
    // go through all pairs of points in this pair of nodes, checking
    // that each pair's distance lies within the required range.
	int xl, xr, yl, yr;
	int x, y;
    rs_params* p = (rs_params*)vparams;
	int D = p->ytree->ndim;
	bool allinrange = TRUE;
	
	// is the bounding box fully within the desired range?
	if (p->usemin) {
		double mindistsq = kdtree_node_node_mindist2(p->xtree, xnode, p->ytree, ynode);
		if (mindistsq < p->mindistsq)
			allinrange = FALSE;
	}
	if (allinrange && p->usemax) {
		double maxdistsq = kdtree_node_node_maxdist2_bailout(p->xtree, xnode, p->ytree, ynode, p->maxdistsq);
		if (maxdistsq > p->maxdistsq)
			allinrange = FALSE;
	}
	if (allinrange) {
		// for each Y point, increment its counter by the number of points in the X node.
		int NX, yl, yr, y;
		NX = kdtree_node_npoints(xnode);
		yl = ynode->l;
		yr = ynode->r;
		for (y=yl; y<=yr; y++) {
			int iy = p->ytree->perm[y];
			p->counts[iy] += NX;
		}
		return;
	}

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
			px = p->xtree->data + x * D;
			d2 = distsq(px, py, D);
			if ((p->usemax) && (d2 > p->maxdistsq))
				continue;
			if ((p->usemin) && (d2 < p->mindistsq))
				continue;
			p->counts[iy]++;
		}
	}
}





bool rc_self_should_recurse(void* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode) {
	if (xnode > ynode)
		return FALSE;
	return rc_should_recurse(vparams, xnode, ynode);
}

void rc_self_handle_result(void* vparams, kdtree_node_t* xnode, kdtree_node_t* ynode) {
	int xl, xr, yl, yr;
	int x, y;
    rs_params* p = (rs_params*)vparams;
	int D = p->ytree->ndim;

	if (xnode > ynode)
		return;

	if (xnode == ynode) {
		int x2;
		xl = xnode->l;
		xr = xnode->r;

		for (x=xl; x<=xr; x++) {
			double* px = p->xtree->data + x * D;
			int ix = p->xtree->perm[x];
			for (x2=x+1; x2<=xr; x2++) {
				double d2;
				double* px2;
				int ix2 = p->xtree->perm[x2];
				px2 = p->xtree->data + x2 * D;
				d2 = distsq(px, px2, D);
				if ((p->usemax) && (d2 > p->maxdistsq))
					continue;
				if ((p->usemin) && (d2 < p->mindistsq))
					continue;
				p->counts[ix]++;
				p->counts[ix2]++;
			}
			// the diagonal...
			if ((p->usemin) && (0.0 < p->mindistsq))
				continue;
			p->counts[ix]++;
		}
		return;
	}

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
			int ix = p->xtree->perm[x];
			px = p->xtree->data + x * D;
			d2 = distsq(px, py, D);
			if ((p->usemax) && (d2 > p->maxdistsq))
				continue;
			if ((p->usemin) && (d2 < p->mindistsq))
				continue;
			p->counts[iy]++;
			p->counts[ix]++;
		}
	}
}

