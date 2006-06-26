#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "histogram.h"

static histogram* hist_new(int nbins) {
	histogram* h = calloc(1, sizeof(histogram));
	if (!h) {
		fprintf(stderr, "Couldn't allocate a histogram.");
		return NULL;
	}
	h->hist = calloc(nbins, sizeof(int));
	if (!h->hist) {
		fprintf(stderr, "Couldn't allocate a histogram with %i bins.", nbins);
		free(h);
		return NULL;
	}
	h->Nbins = nbins;
	return h;
}


histogram* histogram_new_nbins(double zero, double maximum, int Nbins) {
	double binsize = (maximum - zero) / (double)(Nbins - 1);
	histogram* h = hist_new(Nbins);
	h->binsize = binsize;
	return h;
}

histogram* histogram_new_binsize(double zero, double maximum, double binsize) {
	int Nbins = (int)ceil((maximum - zero) / binsize) + 1;
	histogram* h = hist_new(Nbins);
	h->binsize = binsize;
	return h;
}


void histogram_free(histogram* h) {
	free(h->hist);
	free(h);
}

int histogram_add(histogram* h, double val) {
	int bin = (val - h->zero) / h->binsize;
	if (bin < 0)
		bin = 0;
	if (bin >= h->Nbins)
		bin = h->Nbins - 1;
	h->hist[bin]++;
	return bin;
}

