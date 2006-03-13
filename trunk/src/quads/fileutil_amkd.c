#include "fileutil_amkd.h"
#include "kdutil.h"

kdtree *read_starkd(FILE *treefid, double *ramin, double *ramax,
                    double *decmin, double *decmax)
{
	kdtree *starkd = fread_kdtree(treefid);
	fread(ramin, sizeof(double), 1, treefid);
	fread(ramax, sizeof(double), 1, treefid);
	fread(decmin, sizeof(double), 1, treefid);
	fread(decmax, sizeof(double), 1, treefid);
	return starkd;
}

kdtree *read_codekd(FILE *treefid, double *index_scale)
{
	kdtree *codekd = fread_kdtree(treefid);
	fread(index_scale, sizeof(double), 1, treefid);
	return codekd;
}

void write_starkd(FILE *treefid, kdtree *starkd,
                  double ramin, double ramax, double decmin, double decmax)
{
	fwrite_kdtree(starkd, treefid);
	fwrite(&ramin, sizeof(double), 1, treefid);
	fwrite(&ramax, sizeof(double), 1, treefid);
	fwrite(&decmin, sizeof(double), 1, treefid);
	fwrite(&decmax, sizeof(double), 1, treefid);
	return ;
}

void write_codekd(FILE *treefid, kdtree *codekd, double index_scale)
{
	fwrite_kdtree(codekd, treefid);
	fwrite(&index_scale, sizeof(double), 1, treefid);
	return ;
}

