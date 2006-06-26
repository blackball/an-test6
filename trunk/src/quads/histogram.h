#ifndef HISTOGRAM_H
#define HISTOGRAM_H

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

#endif
