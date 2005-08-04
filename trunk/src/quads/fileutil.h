#ifndef fileutil_H
#define fileutil_H
#include "starutil.h"
#include "KD/kdtree.h"

#define FOPEN_ERR -301
#define fopenout(n,f) {if(n){f = fopen(n,"w"); if(!f) {fprintf(stderr,"ERROR OPENING FILE %s for writing.\n",n);return(FOPEN_ERR);}} else f=stdout;}
#define fopenin(n,f) {if(n){f = fopen(n,"r"); if(!f) {fprintf(stderr,"ERROR OPENING FILE %s for reading.\n",n);return(FOPEN_ERR);}} else f=stdin;}
#define fnfree(n) {if(n) free(n);}
#define fopenoutplus(n,f) {if(n){f = fopen(n,"w+"); if(!f) {fprintf(stderr,"ERROR OPENING FILE %s for writing+.\n",n);return(FOPEN_ERR);}} else f=stdout;}

typedef unsigned short int magicval;

#define MAGIC_VAL 0xFF00
#define ASCII_VAL 0x754E

#define READ_FAIL -1

#define freadcode(c,f) fread(c->farr,sizeof(double),DIM_CODES,f)
#define freadstar(s,f) fread(s->farr,sizeof(double),DIM_STARS,f)
#define fwritestar(s,f) fwrite((double *)s->farr,sizeof(double),DIM_STARS,f)
#define fscanfcode(c,f) fscanf(f,"%lf,%lf,%lf,%lf\n",c->farr,c->farr+1,c->farr+2,c->farr+3)

void readonequad(FILE *fid,qidx *iA,qidx *iB,qidx *iC,qidx *iD);
void writeonequad(FILE *fid,qidx iA,qidx iB,qidx iC,qidx iD);
void readonecode(FILE *fid,double *Cx, double *Cy, double *Dx, double *Dy);
void writeonecode(FILE *fid,double Cx, double Cy, double Dx, double Dy);

stararray *readcat(FILE *fid,sidx *numstars, dimension *Dim_Stars,
	   double *ramin, double *ramax, double *decmin, double *decmax);

quadarray *readidlist(FILE *fid,qidx *numpix);

char read_objs_header(FILE *fid, sidx *numstars, dimension *DimStars, 
             double *ramin,double *ramax,double *decmin,double *decmax);
char read_code_header(FILE *fid, qidx *numcodes, sidx *numstars,
		      dimension *DimCodes, double *index_scale);
char read_quad_header(FILE *fid, qidx *numquads, sidx *numstars,
		      dimension *DimQuads, double *index_scale);
void write_objs_header(FILE *fid, char ASCII, sidx numstars,
    dimension DimStars, double ramin,double ramax,double decmin,double decmax);
void write_code_header(FILE *codefid, char ASCII, qidx numCodes, 
		       sidx numstars, dimension DimCodes, double index_scale);
void write_quad_header(FILE *quadfid, char ASCII, qidx numQuads, sidx numstars,
		       dimension DimQuads, double index_scale);
void fix_code_header(FILE *codefid, char ASCII, qidx numCodes, size_t len);
void fix_quad_header(FILE *quadfid, char ASCII, qidx numQuads, size_t len);

xyarray *readxy(FILE *fid, char ParityFlip);

kdtree *read_starkd(FILE *treefid, double *ramin, double *ramax, 
		    double *decmin, double *decmax);
kdtree *read_codekd(FILE *treefid,double *index_scale);
void write_starkd(FILE *treefid, kdtree *starkd,
		  double ramin, double ramax, double decmin, double decmax);
void write_codekd(FILE *treefid, kdtree *codekd,double index_scale);


sidx readquadidx(FILE *fid, sidx **starlist, qidx **starnumq, 
		 qidx ***starquads);
signed int compare_sidx(const void *x,const void *y);




#endif
