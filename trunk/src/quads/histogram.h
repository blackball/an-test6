#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <stdio.h>

struct histogram {
	double zero;
	double binsize;
	int Nbins;
	int* hist;
};
typedef struct histogram histogram;

histogram* histogram_new_nbins(double zero, double maximum, int Nbins);

histogram* histogram_new_binsize(double zero, double maximum, double binsize);

void histogram_free(histogram* h);

int histogram_add(histogram* h, double val);

void histogram_print_matlab(histogram* h, FILE* fid);

// assumes each count is on the left side of the bin.
double histogram_mean(histogram* h);

#endif
