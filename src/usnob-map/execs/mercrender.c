#include "mercrender.h"
#include "tilerender.h"

struct mercargs {
	render_args_t* args;
	float* fluximg;
	merctree* merc;
};
typedef struct mercargs mercargs;

static void leaf_node(kdtree_t* kd, int node, mercargs* args);
static void expand_node(kdtree_t* kd, int node, mercargs* args);
static void addstar(double xp, double yp, merc_flux* flux, mercargs* args);

float* mercrender_file(char* fn, render_args_t* args) {
	float* fluximg;
	merctree* merc = merctree_open(fn);
	if (!merc) {
		fprintf(stderr, "Failed to open merctree %s\n", fn);
		return NULL;
	}

	fluximg = calloc(args->W*args->H*3, sizeof(float));
	if (!fluximg) {
		fprintf(stderr, "Failed to allocate flux image.\n");
		return NULL;
	}
	mercrender(merc, args, fluximg);
	merctree_close(merc);
	return fluximg;
}

void mercrender(merctree* merc, render_args_t* args,
				float* fluximg) {
	double querylow[2], queryhigh[2];
	double xmargin, ymargin;
	mercargs margs;

	margs.args = args;
	margs.fluximg = fluximg;
	margs.merc = merc;

	// Magic number 2: number of pixels around a star.
	xmargin = 2.0 / args->xpixelpermerc;
	ymargin = 2.0 / args->ypixelpermerc;

	querylow [0] = max(0.0, args->xmercmin - xmargin);
	queryhigh[0] = min(1.0, args->xmercmax + xmargin);
	querylow [1] = max(0.0, args->ymercmin - ymargin);
	queryhigh[1] = min(1.0, args->ymercmax + ymargin);

	kdtree_nodes_contained(merc->tree, querylow, queryhigh,
						   expand_node, leaf_node, &margs);
}

static void expand_node(kdtree_t* kd, int node, mercargs* margs) {
	int xp0, xp1, yp0, yp1;
	int D = 2;
	double bblo[D], bbhi[D];

	if (KD_IS_LEAF(kd, node)) {
		leaf_node(kd, node, margs);
		return;
	}

	// check if this whole box fits inside a pixel.
	if (!kdtree_get_bboxes(kd, node, bblo, bbhi)) {
		fprintf(stderr, "Error, node %i does not have bounding boxes.\n", node);
		exit(-1);
	}
	xp0 = xmerc2pixel(bblo[0], margs->args);
	xp1 = xmerc2pixel(bbhi[0], margs->args);
	if (xp1 == xp0) {
		yp0 = ymerc2pixel(bblo[1], margs->args);
		yp1 = ymerc2pixel(bbhi[1], margs->args);
		if (yp1 == yp0) {
			// This node fits inside a single pixel of the output image.
			addstar(bblo[0], bblo[1], &(margs->merc->stats[node].flux), margs);
			return;
		}
	}

	expand_node(kd,  KD_CHILD_LEFT(node), margs);
	expand_node(kd, KD_CHILD_RIGHT(node), margs);
}

static void leaf_node(kdtree_t* kd, int node, mercargs* margs) {
	int k;
	int L, R;

	L = kdtree_left(kd, node);
	R = kdtree_right(kd, node);

	for (k=L; k<=R; k++) {
		double pt[2];
		merc_flux* flux;

		kdtree_copy_data_double(kd, k, 1, pt);
		flux = margs->merc->flux + k;
		addstar(pt[0], pt[1], flux, margs);
	}
}

static void addstar(double xp, double yp,
					merc_flux* flux, mercargs* margs) {
	/*
	  // How to create the "scale" array in Matlab:
	  x=[-2:2];
	  y=[-2:2];
	  xx=repmat(x, [5,1]);
	  yy=repmat(y', [1,5]);
	  E=exp(-(xx.^2 + yy.^2)/(2 * 0.5));
	  E./sum(sum(E))
	*/
	int dx[] = { -2, -1,  0,  1,  2, 
				 -2, -1,  0,  1,  2, 
				 -2, -1,  0,  1,  2, 
				 -2, -1,  0,  1,  2, 
				 -2, -1,  0,  1,  2 };
	int dy[] = {  -2, -2, -2, -2, -2,
				  -1, -1, -1, -1, -1,
				  0,  0,  0,  0,  0,
				  1,  1,  1,  1,  1,
				  2,  2,  2,  2,  2 };
	float scale[] = {
		0.00010678874539,  0.00214490928858,   0.00583046794284,  0.00214490928858,  0.00010678874539,
		0.00214490928858,  0.04308165471265,  0.11710807914534,  0.04308165471265,  0.00214490928858,
		0.00583046794284,  0.11710807914534,  0.31833276350651,  0.11710807914534,  0.00583046794284,
		0.00214490928858,  0.04308165471265,  0.11710807914534,  0.04308165471265,  0.00214490928858,
		0.00010678874539,  0.00214490928858,  0.00583046794284,  0.00214490928858,  0.00010678874539 };
	int mindx = -2;
	int maxdx = 2;
	int mindy = -2;
	int maxdy = 2;

	int i;
	int x, y;
	int w = margs->args->W;
	int h = margs->args->H;

	x = xmerc2pixel(xp, margs->args);
	if (x+maxdx < 0 || x+mindx >= w) {
		return;
	}
	y = ymerc2pixel(yp, margs->args);
	if (y+maxdy < 0 || y+mindy >= h) {
		return;
	}

	for (i=0; i<sizeof(dx)/sizeof(int); i++) {
		int ix, iy;
		ix = x + dx[i];
		if ((ix < 0) || (ix >= w)) continue;
		iy = y + dy[i];
		if ((iy < 0) || (iy >= h)) continue;
		margs->fluximg[3*(iy*w + ix) + 0] += flux->rflux * scale[i];
		margs->fluximg[3*(iy*w + ix) + 1] += flux->bflux * scale[i];
		margs->fluximg[3*(iy*w + ix) + 2] += flux->nflux * scale[i];
	}
}

