#ifndef fileutil_H
#define fileutil_H
#include "starutil.h"

#define FOPEN_ERR -301
#define fopenout(n,f) {if(n){f = fopen(n,"w"); if(!f) {fprintf(stderr,"ERROR OPENING FILE %s for writing.\n",n);return(FOPEN_ERR);}} else f=stdout;}
#define fopenin(n,f) {if(n){f = fopen(n,"r"); if(!f) {fprintf(stderr,"ERROR OPENING FILE %s for reading.\n",n);return(FOPEN_ERR);}} else f=stdin;}
#define fnfree(n) {if(n) free(n);}
#define fopenoutplus(n,f) {if(n){f = fopen(n,"w+"); if(!f) {fprintf(stderr,"ERROR OPENING FILE %s for writing+.\n",n);return(FOPEN_ERR);}} else f=stdout;}

typedef unsigned short int magicval;

#define MAGIC_VAL 0xFF00
#define ASCII_VAL 0x754E

#define READ_FAIL -1

stararray *readcat(FILE *fid,sidx *numstars, dimension *Dim_Stars,
	   double *ramin, double *ramax, double *decmin, double *decmax);

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




#endif
