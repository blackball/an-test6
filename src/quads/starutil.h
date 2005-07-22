#ifndef starutil_H
#define starutil_H
#include "KD/ambs.h"
#include "KD/amdyv_array.h"

#define PLANAR_GEOMETRY 0

#if PLANAR_GEOMETRY==1
  #define DIM_STARS 2
#else
  #define DIM_STARS 3
#endif

#define DIM_CODES 4
#define DIM_QUADS 4
#define DIM_XY 2

#define SIDX_MAX ULONG_MAX
#define QIDX_MAX ULONG_MAX

typedef unsigned long int qidx;
typedef unsigned long int sidx;
typedef unsigned short int dimension;

typedef ivec quad;
typedef dyv code;
typedef dyv star;
typedef dyv xy;
typedef ivec sizev;

typedef ivec_array quadarray;
typedef dyv_array codearray;
typedef dyv_array stararray;
typedef dyv_array xyarray;

#define mk_star() (star *)mk_dyv(DIM_STARS)
#define mk_stard(d) (star *)mk_dyv(d)
#define mk_code() (code *)mk_dyv(DIM_CODES)
#define mk_coded(d) (code *)mk_dyv(d)
#define mk_quad() (quad *)mk_ivec(DIM_CODES)
#define mk_quadd(d) (quad *)mk_ivec(d)
#define mk_xy(n) (xy *)mk_dyv(DIM_XY*n)
#define mk_sizev(q) (sizev *)mk_ivec(q)
#define free_star(s) free_dyv((dyv *)s)
#define free_code(c) free_dyv((dyv *)c)
#define free_quad(q) free_ivec((ivec *)q)
#define free_xy(s) free_dyv((dyv *)s)
#define free_sizev(s) free_ivec((ivec *)s)

#define star_ref(s,i) dyv_ref((dyv *)s,i)
#define code_ref(c,i) dyv_ref((dyv *)c,i)
#define quad_ref(q,i) ivec_ref((ivec *)q,i)
#define xy_ref(s,i) dyv_ref((dyv *)s,i)
#define xy_refx(s,i) dyv_ref((dyv *)s,DIM_XY*i)
#define xy_refy(s,i) dyv_ref((dyv *)s,DIM_XY*i+1)
#define sizev_ref(s,i) ivec_ref((ivec *)s,i)

#define star_set(s,i,v) dyv_set((dyv *)s,i,v)
#define code_set(c,i,v) dyv_set((dyv *)c,i,v)
#define quad_set(q,i,j) ivec_set((ivec *)q,i,j)
#define xy_set(s,i,v) dyv_set((dyv *)s,i,v)
#define xy_setx(s,i,v) dyv_set((dyv *)s,DIM_XY*i,v)
#define xy_sety(s,i,v) dyv_set((dyv *)s,DIM_XY*i+1,v)
#define sizev_set(s,i,v) ivec_set((ivec *)s,i,v)

#define mk_stararray(n) (stararray *)mk_dyv_array(n)
#define mk_quadarray(n) (quadarray *)mk_ivec_array(n)
#define mk_codearray(n) (codearray *)mk_dyv_array(n)
#define mk_xyarray(n) (xyarray *)mk_dyv_array(n)
#define free_stararray(s) free_dyv_array((dyv_array *)s)
#define free_quadarray(q) free_ivec_array((ivec_array *)q)
#define free_codearray(c) free_dyv_array((dyv_array *)c)
#define free_xyarray(s) free_dyv_array((dyv_array *)s)

#define mk_starcopy(s) (star *)mk_copy_dyv((const dyv *)s)
#define copy_codeptr(cc,dd,i) dyv_array_set_no_copy(cc,i,dyv_array_ref((dyv_array *)dd,i))

stararray *readcat(FILE *fid,sidx *numstars, dimension *Dim_Stars,
	   double *ramin, double *ramax, double *decmin, double *decmax);

star *make_rand_star(double ramin, double ramax, 
		     double decmin, double decmax);

void star_coords(star *s,star *r,double *x,double *y);
void star_midpoint(star *M,star *A,star *B);

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



unsigned long int choose(unsigned int nn,unsigned int mm);

#define HELP_ERR -101
#define OPT_ERR -201
#define FOPEN_ERR -301
#define fopenout(n,f) {if(n){f = fopen(n,"w"); if(!f) {fprintf(stderr,"ERROR OPENING FILE %s for writing.\n",n);return(FOPEN_ERR);}} else f=stdout;}
#define fopenin(n,f) {if(n){f = fopen(n,"r"); if(!f) {fprintf(stderr,"ERROR OPENING FILE %s for reading.\n",n);return(FOPEN_ERR);}} else f=stdin;}
#define fnfree(n) {if(n) free(n);}
#define fopenoutplus(n,f) {if(n){f = fopen(n,"w+"); if(!f) {fprintf(stderr,"ERROR OPENING FILE %s for writing+.\n",n);return(FOPEN_ERR);}} else f=stdout;}

typedef unsigned short int magicval;

#define MAGIC_VAL 0xFF00
#define ASCII_VAL 0x754E

#define READ_FAIL -1

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define PIl          3.1415926535897932384626433832795029L  /* pi */


#endif 
