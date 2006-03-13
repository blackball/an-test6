#ifndef FILEUTIL_AM_H
#define FILEUTIL_AM_H

#include "starutil_am.h"
#include "fileutil.h"

#define freadcode(c,f) fread(c->farr,sizeof(double),DIM_CODES,f)
#define freadstar(s,f) fread(s->farr,sizeof(double),DIM_STARS,f)
#define fwritestar(s,f) fwrite((double *)s->farr,sizeof(double),DIM_STARS,f)
#define fprintfstar(s,f) fprintf(f,"%lf,%lf,%lf\n",star_ref(s,0),star_ref(s,1),star_ref(s,2))
#define fseekocat(i,p,f) fseeko(f,p+i*(DIM_STARS*sizeof(double)),SEEK_SET)

stararray *readcat(FILE *fid, sidx *numstars, dimension *Dim_Stars,
                  double *ramin, double *ramax, double *decmin, double *decmax,
				   int nkeep);

quadarray *readidlist(FILE *fid, qidx *numpix);

sidx readquadidx(FILE *fid, sidx **starlist, qidx **starnumq,
                 qidx ***starquads);
signed int compare_sidx(const void *x, const void *y);

#endif
