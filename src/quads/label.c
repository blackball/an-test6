#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "label.h"

#define check_bounds(x, y)   ( ((x)>=0) && ((x)<W) && ((y)>=0) && ((y)<(H)) )

inline float roundness(label_info* label, int* labelim, int WW) {
	int y,ymin,ymax,x,xmin,xmax,l;
	float cx,cy,a,b,cc,d,sin2t,cos2t,Imin,Imax;
	ymin = label->ylow;
	ymax = label->yhigh;
	xmin = label->xlow;
	xmax = label->xhigh;
	l    = label->label;
	cx = label->xcenter;
	cy = label->ycenter;
	a = b = cc = 0.0;
	for (y=ymin; y<=ymax; y++)
		for (x=xmin; x<=xmax; x++)
			if (labelim[y*WW + x] == l) {
				a += (x - cx) * (x - cx);
				b += 2.0 * (x - cx) * (y - cy);
				cc += (y - cy) * (y - cy);
			}
	d = b*b + (a-cc)*(a-cc);
	if (d == 0.0) // square objects.
		return 1.0;
	sin2t = b / sqrt(d);
	cos2t = (a-cc) / sqrt(d);
	Imin = 0.5 * ( (cc+a) - (a-cc)*cos2t - b*sin2t );
	Imax = 0.5 * ( (cc+a) + (a-cc)*cos2t + b*sin2t );
	/*
	  printf("a=%f, b=%f, cc=%f, d=%f, sin2t=%f, cos2t=%f, Imin=%f, Imax=%f, rnd=%f.\n",
	  a, b, cc, d, sin2t, cos2t, Imin, Imax, Imin/Imax);
	*/
	return Imin / Imax;
}



unsigned char label_image_char(unsigned char* source, int W, int H, unsigned char* dest) {
	int n1, n2, n3, n4;
	int ln1=0, ln2=0, ln3=0, ln4=0;
	int label;
	int firstpix[256];
	int outoflabels;
	int i,j,k,p,m,n;

	label = 0;
	outoflabels = 0;
	bzero(dest, W*H);

	for (i=0; i<H; i++) {
		if (outoflabels)
			break;
		for (j=0; j<W; j++) {
			// if this pixel is not marked, do nothing.
			if (!source[i*W+j])
				continue;

			// count labelled neighbours (above & left).
			n1 = n2 = n3 = n4 = 0;
			if (check_bounds(j-1, i)   && dest[i*W+(j-1)]) {
				n1 = 1;
				ln1 = dest[i*W+(j-1)];
			}
			if (check_bounds(j-1, i-1) && dest[(i-1)*W+(j-1)]) {
				n2 = 1;
				ln2 = dest[(i-1)*W+(j-1)];
			}
			if (check_bounds(j, i-1)   && dest[(i-1)*W+j]) {
				n3 = 1;
				ln3 = dest[(i-1)*W+j];
			}
			if (check_bounds(j+1, i-1) && dest[(i-1)*W+(j+1)]) {
				n4 = 1;
				ln4 = dest[(i-1)*W+(j+1)];
			}
			n = n1 + n2 + n3 + n4;

			if (n == 0) {
				// no neighbours: give a new label.
				label++;
				if (label == 256) {
					printf("ran out of labels!\n");
					outoflabels = 1;
					break;
				}
				dest[i*W+j] = label;
				firstpix[label] = i*W+j;

			} else if (n == 1) {

				dest[i*W+j] = (n1 ? ln1 : (n2 ? ln2 : (n3 ? ln3 : ln4)));

			} else {
				int l[4];
				int smallest;
				m=0;
				if (n2) l[m++] = ln2;
				if (n3) l[m++] = ln3;
				if (n4) l[m++] = ln4;
				if (n1) l[m++] = ln1;

				// give the new pixel the smallest of its neighbours' values.
				smallest = l[0];
				for (k=1; k<m; k++)
					if (l[k] < smallest) smallest = l[k];

				dest[i*W+j] = smallest;

				// several neighbours; set all previous values to be the smallest value.
				for (k=0; k<m; k++)
					if (l[k] != smallest)
						for (p=firstpix[l[k]]; p<(i*W+j); p++)
							if (dest[p] == l[k])
								dest[p] = smallest;
			}
		}
	}
	return label;
}



int label_image_int(unsigned char* source, int W, int H, int* dest) {
	int n1, n2, n3, n4;
	int ln1=0, ln2=0, ln3=0, ln4=0;
	int label;
	int firstpix[65536];
	int outoflabels;
	int i,j,k,p,m,n;

	label = 0;
	outoflabels = 0;
	bzero(dest, sizeof(int)*W*H);

	for (i=0; i<H; i++) {
		if (outoflabels)
			break;
		for (j=0; j<W; j++) {
			// if this pixel is not marked, do nothing.
			if (!source[i*W+j])
				continue;

			// count labelled neighbours (above & left).
			n1 = n2 = n3 = n4 = 0;
			if (check_bounds(j-1, i)   && dest[i*W+(j-1)]) {
				n1 = 1;
				ln1 = dest[i*W+(j-1)];
			}
			if (check_bounds(j-1, i-1) && dest[(i-1)*W+(j-1)]) {
				n2 = 1;
				ln2 = dest[(i-1)*W+(j-1)];
			}
			if (check_bounds(j, i-1)   && dest[(i-1)*W+j]) {
				n3 = 1;
				ln3 = dest[(i-1)*W+j];
			}
			if (check_bounds(j+1, i-1) && dest[(i-1)*W+(j+1)]) {
				n4 = 1;
				ln4 = dest[(i-1)*W+(j+1)];
			}
			n = n1 + n2 + n3 + n4;

			if (n == 0) {
				// no neighbours: give a new label.
				label++;
				if (label == 65536) {
					printf("ran out of labels!\n");
					outoflabels = 1;
					break;
				}
				dest[i*W+j] = label;
				firstpix[label] = i*W+j;

			} else if (n == 1) {

				dest[i*W+j] = (n1 ? ln1 : (n2 ? ln2 : (n3 ? ln3 : ln4)));

			} else {
				int l[4];
				int smallest;
				m=0;
				if (n2) l[m++] = ln2;
				if (n3) l[m++] = ln3;
				if (n4) l[m++] = ln4;
				if (n1) l[m++] = ln1;

				// give the new pixel the smallest of its neighbours' values.
				smallest = l[0];
				for (k=1; k<m; k++)
					if (l[k] < smallest) smallest = l[k];

				dest[i*W+j] = smallest;

				// several neighbours; set all previous values to be the smallest value.
				for (k=0; k<m; k++)
					if (l[k] != smallest)
						for (p=firstpix[l[k]]; p<(i*W+j); p++)
							if (dest[p] == l[k])
								dest[p] = smallest;
			}
		}
	}
	return label;
}




















inline int new_label(int x, int y, label_info* labels,
					 int* nextlabel, int MAXLABELS,
					 int* dest, int W) {

	// find a "label_info" struct with "label" == -1, starting from "nextlabel".

	while ((*nextlabel < MAXLABELS) && (labels[*nextlabel].label != -1))
		(*nextlabel)++;

	if (*nextlabel >= MAXLABELS)
		return -1;

	// set the label number and initialize the other struct members...

	labels[*nextlabel].label   = *nextlabel;
	labels[*nextlabel].xlow    = x;
	labels[*nextlabel].xhigh   = x;
	labels[*nextlabel].ylow    = y;
	labels[*nextlabel].yhigh   = y;
	labels[*nextlabel].mass    = 1;
	labels[*nextlabel].xcenter = (float)x;
	labels[*nextlabel].ycenter = (float)y;

	dest[y*W+x] = *nextlabel;

	return 0;
}


inline void add_pixel(int x, int y, label_info* labels, int label, int* dest, int W) {

	dest[y*W+x] = label;

	if (x < labels[label].xlow)
		labels[label].xlow = x;
	if (x > labels[label].xhigh)
		labels[label].xhigh = x;
	if (y < labels[label].ylow)
		labels[label].ylow = y;
	if (y > labels[label].yhigh)
		labels[label].yhigh = y;

	labels[label].mass++;
	labels[label].xcenter += (float)x;
	labels[label].ycenter += (float)y;

}

inline void relabel(label_info* oldlabel, int newlabel, int* dest, int W) {
	// relabel all pixels that used to belong to the other labels...
	int xlo, xhi, ylo, yhi, oldl;
	int xx, yy;

	xlo  = oldlabel->xlow;
	xhi  = oldlabel->xhigh;
	ylo  = oldlabel->ylow;
	yhi  = oldlabel->yhigh;
	oldl = oldlabel->label;

	for (yy=ylo; yy<=yhi; yy++)
		for (xx=xlo; xx<=xhi; xx++)
			if (dest[yy*W + xx] == oldl)
				dest[yy*W + xx] = newlabel;

}

inline void merge_labels(int x, int y, label_info* labels, int* lnums, int N,
						 int* nextlabel, int MAXLABELS, int* dest, int W) {

	// set all the labels to the smallest label.

	int smallest = MAXLABELS;
	int i;
	/*
	  float cx, cy;
	  int mass;
	*/

	for (i=0; i<N; i++)
		if (lnums[i] < smallest)
			smallest = lnums[i];

	// sum up the struct members...
	/*
	  mass = labels[smallest].mass;
	  cx = mass * labels[smallest].xcenter;
	  cy = mass * labels[smallest].ycenter;
	*/
	for (i=0; i<N; i++) {
		if (lnums[i] == smallest)
			continue;
		if (labels[lnums[i]].xlow  < labels[smallest].xlow)
			labels[smallest].xlow = labels[lnums[i]].xlow;
		if (labels[lnums[i]].ylow  < labels[smallest].ylow)
			labels[smallest].ylow = labels[lnums[i]].ylow;
		if (labels[lnums[i]].xhigh > labels[smallest].xhigh)
			labels[smallest].xhigh = labels[lnums[i]].xhigh;
		if (labels[lnums[i]].yhigh > labels[smallest].yhigh)
			labels[smallest].yhigh = labels[lnums[i]].yhigh;

		labels[smallest].mass += labels[lnums[i]].mass;
		labels[smallest].xcenter += labels[lnums[i]].xcenter;
		labels[smallest].ycenter += labels[lnums[i]].ycenter;
		/*
		  mass += labels[lnums[i]].mass;
		  cx   += labels[lnums[i]].mass * labels[lnums[i]].xcenter;
		  cy   += labels[lnums[i]].mass * labels[lnums[i]].ycenter;
		*/
	}

	/*
	  labels[smallest].mass = mass;
	  labels[smallest].xcenter = cx / (float)mass;
	  labels[smallest].ycenter = cy / (float)mass;
	*/

	// relabel all pixels that used to belong to the other labels...
	for (i=0; i<N; i++) {
		if (lnums[i] == smallest)
			continue;
		relabel(labels+lnums[i], smallest, dest, W);
		// mark the merged labels as free to be reused...
		labels[lnums[i]].label = -1;
	}

	dest[y*W + x] = smallest;

	*nextlabel = smallest;
}



label_info* label_image_super(unsigned char* source, int W, int H, int* dest,
							  int* N) {
	int n1, n2, n3, n4;
	int ln1, ln2, ln3, ln4;
	int i,j,m,n;

	label_info* labelinfos;
	int MAXLABELS = 65536;
	int minl, maxl;

	int nextlabel = 1;
	labelinfos = (label_info*)malloc(sizeof(label_info) * MAXLABELS);
	for (i=0; i<MAXLABELS; i++)
		labelinfos[i].label = -1;
	labelinfos--;

	bzero(dest, sizeof(int)*W*H);

	/* top-left pixel. */
	i = 0;
	j = 0;
	n = 0;
	if (source[0])
		new_label(0, 0, labelinfos, &nextlabel, MAXLABELS, dest, W);

	/* top row. */
	for (j=1; j<W; j++) {
		// if this pixel is not marked, do nothing.
		if (!source[j])
			continue;
		if (dest[j-1])
			// one neighbour.  add to that label.
			add_pixel(j, 0, labelinfos, dest[j-1], dest, W);
		else
			// zero neighbours.  new label.
			new_label(j, 0, labelinfos, &nextlabel, MAXLABELS, dest, W);
	}

	/* left edge. */
	j=0;
	for (i=1; i<H; i++) {
		// if this pixel is not marked, do nothing.
		if (!source[i*W])
			continue;
		if (dest[(i-1)*W])
			// one neighbour.  add to that label.
			add_pixel(0, i, labelinfos, dest[(i-1)*W], dest, W);
		else
			// zero neighbours.  new label.
			new_label(0, i, labelinfos, &nextlabel, MAXLABELS, dest, W);
	}

	/* the rest of the image... */
	for (i=1; i<H; i++) {
		for (j=1; j<W; j++) {
			int ind = i*W + j;
			int allsame = 1;
			int l=0;
			if (!source[ind])
				continue;
			n1  = n2  = n3  = n4  = 0;
			ln1 = ln2 = ln3 = ln4 = 0;
			n = 0;
			if (dest[ind-1]) {
				n1++;
				n++;
				ln1 = dest[ind-1];
				l = ln1;
			}
			if (dest[ind-W-1]) {
				ln2 = dest[ind-W-1];
				if (n && (ln2 != l))
					allsame = 0;
				n2++;
				n++;
				l = ln2;
			}
			if (dest[ind-W]) {
				ln3 = dest[ind-W];
				if (n && (ln3 != l))
					allsame = 0;
				n3++;
				n++;
				l = ln3;
			}
			if (dest[ind-W+1]) {
				ln4 = dest[ind-W+1];
				if (n && (ln4 != l))
					allsame = 0;
				n4++;
				n++;
				l = ln4;
			}

			if (n == 0)
				// no neighbours.  new label.
				new_label(j, i, labelinfos, &nextlabel, MAXLABELS, dest, W);

			else if (n == 1)
				// one neighbour.  easy...
				add_pixel(j, i, labelinfos, ln1+ln2+ln3+ln4, dest, W);

			else {
				// multiple neighbours.

				if (allsame)
					// the neighbours are all part of the same label.  just add.
					add_pixel(j, i, labelinfos, l, dest, W);
				else {
					int l[4];
					m=0;
					if (n1) l[m++] = ln1;
					if (n2) l[m++] = ln2;
					if (n3) l[m++] = ln3;
					if (n4) l[m++] = ln4;

					// merge them...
					merge_labels(j, i, labelinfos, l, m, &nextlabel, MAXLABELS, dest, W);
				}
			}
		}
	}


	// defrag the label numbers.

	maxl = MAXLABELS-1;
	minl = 1;

	for (;;) {
		// find the largest used label.
		while ((maxl > minl) && (labelinfos[maxl].label == -1))
			maxl--;

		// find the smallest unused label.
		while ((minl < maxl) && (labelinfos[minl].label != -1))
			minl++;

		if (maxl == minl)
			break;

		//printf("defragging %i, %i.\n", minl, maxl);

		// relabel "maxl" as "minl".
		relabel(labelinfos + maxl, minl, dest, W);
		memcpy(labelinfos + minl, labelinfos + maxl, sizeof(label_info));

		// mark "maxl" as unused.
		labelinfos[maxl].label = -1;

	}

	// realloc and return.

	for (i=1; i<MAXLABELS; i++)
		if (labelinfos[i].label == -1)
			break;
	n = i - 1;

	labelinfos++;

	labelinfos = (label_info*)realloc(labelinfos, n * sizeof(label_info));

	*N = n;

	// calculate centroids.

	for (i=0; i<n; i++) {
		labelinfos[i].xcenter /= (float)labelinfos[i].mass;
		labelinfos[i].ycenter /= (float)labelinfos[i].mass;
	}

	return labelinfos;
}
























static inline int new_label_2(int x, int y, label_info* labels,
							  int* nextlabel, int MAXLABELS,
							  int* dest, int W, unsigned char* source) {

	// find a "label_info" struct with "label" == -1, starting from "nextlabel".

	while ((*nextlabel < MAXLABELS) && (labels[*nextlabel].label != -1))
		(*nextlabel)++;

	if (*nextlabel >= MAXLABELS)
		return -1;

	// set the label number and initialize the other struct members...

	labels[*nextlabel].label   = *nextlabel;
	labels[*nextlabel].xlow    = x;
	labels[*nextlabel].xhigh   = x;
	labels[*nextlabel].ylow    = y;
	labels[*nextlabel].yhigh   = y;
	labels[*nextlabel].mass    = 1;
	labels[*nextlabel].xcenter = (float)x;
	labels[*nextlabel].ycenter = (float)y;
	labels[*nextlabel].value   = source[y*W+x];

	dest[y*W+x] = *nextlabel;

	return 0;
}

inline void add_pixel_2(int x, int y, label_info* labels, int label, int* dest, int W, unsigned char* source) {
	dest[y*W+x] = label;
	if (x < labels[label].xlow)
		labels[label].xlow = x;
	if (x > labels[label].xhigh)
		labels[label].xhigh = x;
	if (y < labels[label].ylow)
		labels[label].ylow = y;
	if (y > labels[label].yhigh)
		labels[label].yhigh = y;
	labels[label].mass++;
	labels[label].xcenter += (float)x;
	labels[label].ycenter += (float)y;

	assert(source[y*W + x] == labels[label].value);
}

inline void merge_labels_2(int x, int y, label_info* labels, int* lnums, int N,
						   int* nextlabel, int MAXLABELS, int* dest, int W,
						   unsigned char* source) {

	// set all the labels to the smallest label.

	int smallest = MAXLABELS;
	int i;
	/*
	  float cx, cy;
	  int mass;
	*/
	for (i=0; i<N; i++)
		if (lnums[i] < smallest)
			smallest = lnums[i];

	// sum up the struct members...
	/*
	  mass = labels[smallest].mass;
	  cx = mass * labels[smallest].xcenter;
	  cy = mass * labels[smallest].ycenter;
	*/
	for (i=0; i<N; i++) {
		if (lnums[i] == smallest)
			continue;

		assert(labels[lnums[i]].value == labels[smallest].value);

		if (labels[lnums[i]].xlow  < labels[smallest].xlow)
			labels[smallest].xlow = labels[lnums[i]].xlow;
		if (labels[lnums[i]].ylow  < labels[smallest].ylow)
			labels[smallest].ylow = labels[lnums[i]].ylow;
		if (labels[lnums[i]].xhigh > labels[smallest].xhigh)
			labels[smallest].xhigh = labels[lnums[i]].xhigh;
		if (labels[lnums[i]].yhigh > labels[smallest].yhigh)
			labels[smallest].yhigh = labels[lnums[i]].yhigh;

		labels[smallest].mass += labels[lnums[i]].mass;
		labels[smallest].xcenter += labels[lnums[i]].xcenter;
		labels[smallest].ycenter += labels[lnums[i]].ycenter;
		/*
		  mass += labels[lnums[i]].mass;
		  cx   += labels[lnums[i]].mass * labels[lnums[i]].xcenter;
		  cy   += labels[lnums[i]].mass * labels[lnums[i]].ycenter;
		*/
	}

	/*
	  labels[smallest].mass = mass;
	  labels[smallest].xcenter = cx / (float)mass;
	  labels[smallest].ycenter = cy / (float)mass;
	*/
	// relabel all pixels that used to belong to the other labels...
	for (i=0; i<N; i++) {
		if (lnums[i] == smallest)
			continue;
		relabel(labels+lnums[i], smallest, dest, W);
		// mark the merged labels as free to be reused...
		labels[lnums[i]].label = -1;
	}

	dest[y*W + x] = smallest;

	*nextlabel = smallest;
}

label_info* label_image_super_2(unsigned char* source, int W, int H, int* dest,
								int* N) {
	int n1, n2, n3, n4;
	int ln1, ln2, ln3, ln4;
	int i,j,m,n;

	label_info* labelinfos;
	int MAXLABELS = 65536;
	int minl, maxl;

	int nextlabel = 1;
	labelinfos = (label_info*)malloc(sizeof(label_info) * MAXLABELS);
	for (i=0; i<MAXLABELS; i++)
		labelinfos[i].label = -1;
	labelinfos--;

	bzero(dest, sizeof(int)*W*H);

	for (i=0; i<H; i++) {
		for (j=0; j<W; j++) {
			int ind = i*W + j;
			int allsame = 1;
			int l=0;
			int s1, s2, s3, s4, s;
			if (!source[ind])
				continue;
			n1  = n2  = n3  = n4  = 0;
			ln1 = ln2 = ln3 = ln4 = 0;
			s = s1 = s2 = s3 = s4 = 0;
			n = 0;
			if (check_bounds(j-1, i) && dest[ind-1]) {
				ln1 = dest[ind-1];
				s = s1 = source[ind-1];
				assert(s1 == labelinfos[ln1].value);
				n1++;
				n++;
				l = ln1;
			}
			if (check_bounds(j-1, i-1) && dest[ind-W-1]) {
				ln2 = dest[ind-W-1];
				s = s2 = source[ind-W-1];
				assert(s2 == labelinfos[ln2].value);
				if (n && (ln2 != l))
					allsame = 0;
				n2++;
				n++;
				l = ln2;
			}
			if (check_bounds(j, i-1) && dest[ind-W]) {
				ln3 = dest[ind-W];
				s = s3 = source[ind-W];
				assert(s3 == labelinfos[ln3].value);
				if (n && (ln3 != l))
					allsame = 0;
				n3++;
				n++;
				l = ln3;
			}
			if (check_bounds(j+1, i-1) && dest[ind-W+1]) {
				ln4 = dest[ind-W+1];
				s = s4 = source[ind-W+1];
				assert(s4 == labelinfos[ln4].value);
				if (n && (ln4 != l))
					allsame = 0;
				n4++;
				n++;
				l = ln4;
			}

			if (n == 0)
				// no neighbours.  new label.
				new_label_2(j, i, labelinfos, &nextlabel, MAXLABELS, dest, W, source);

			else if (n == 1) {
				// one neighbour.  easy...
				if (s == source[ind])
					add_pixel_2(j, i, labelinfos, l, dest, W, source);
				else
					// different value than its neighbour: new label.
					new_label_2(j, i, labelinfos, &nextlabel, MAXLABELS, dest, W, source);

			} else {
				// multiple neighbours.

				if (allsame)
					// the neighbours are all part of the same label.
					if (source[ind] == s)
						// if the pixel value is correct, just add it...
						add_pixel_2(j, i, labelinfos, l, dest, W, source);
					else
						// otherwise, create a new label.
						new_label_2(j, i, labelinfos, &nextlabel, MAXLABELS, dest, W, source);
				else {
					// the neighbours are not all part of the same label.
					// if this pixel is joining the merged label, then merge all labels
					// with the same value.
					int oldl[4];
					m=0;
					if (n1 && (source[ind] == s1)) oldl[m++] = ln1;
					if (n2 && (source[ind] == s2)) oldl[m++] = ln2;
					if (n3 && (source[ind] == s3)) oldl[m++] = ln3;
					if (n4 && (source[ind] == s4)) oldl[m++] = ln4;

					if (m)
						// merge them...
						merge_labels_2(j, i, labelinfos, oldl, m, &nextlabel, MAXLABELS, dest, W, source);
					else
						// new label.
						new_label_2(j, i, labelinfos, &nextlabel, MAXLABELS, dest, W, source);
				}
			}


			/// DEBUG
			{
				int j;
				for (j=0; j<(W*H); j++) {
					if (dest[j])
						assert(source[j] == labelinfos[dest[j]].value);
				}
			}

		}
	}


	// defrag the label numbers.

	maxl = MAXLABELS-1;
	minl = 1;

	for (;;) {
		// find the largest used label.
		while ((maxl > minl) && (labelinfos[maxl].label == -1))
			maxl--;
		// find the smallest unused label.
		while ((minl < maxl) && (labelinfos[minl].label != -1))
			minl++;
		if (maxl == minl)
			break;
		// relabel "maxl" as "minl".
		relabel(labelinfos + maxl, minl, dest, W);
		memcpy(labelinfos + minl, labelinfos + maxl, sizeof(label_info));
		// mark "maxl" as unused.
		labelinfos[maxl].label = -1;
	}

	// realloc and return.
	for (i=1; i<MAXLABELS; i++)
		if (labelinfos[i].label == -1)
			break;
	n = i - 1;
	labelinfos++;
	labelinfos = (label_info*)realloc(labelinfos, n * sizeof(label_info));
	*N = n;

	// calculate centroids.
	for (i=0; i<n; i++) {
		labelinfos[i].xcenter /= (float)labelinfos[i].mass;
		labelinfos[i].ycenter /= (float)labelinfos[i].mass;
	}

	return labelinfos;
}

