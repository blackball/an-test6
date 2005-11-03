#ifndef fileutil_H
#define fileutil_H
#include "starutil.h"
#include "KD/kdtree.h"

#define COMMENT_CHAR 35 // #
#define FOPEN_ERR -301
#define READ_FAIL -1

#define fopenout(n,f) {if(n){f=fopen(n,"w");} if(!n||!f){fprintf(stderr,"ERROR OPENING FILE %s for writing.\n",n); return(FOPEN_ERR);}}
#define fopenin(n,f)  {if(n){f=fopen(n,"r");} if(!n||!f){fprintf(stderr,"ERROR OPENING FILE %s for reading.\n",n); return(FOPEN_ERR);}}
#define free_fn(n) {if(n) free(n);}

#define mk_catfn(s)    mk_filename(s,".objs")
#define mk_streefn(s)  mk_filename(s,".skdt")
#define mk_ctreefn(s)  mk_filename(s,".ckdt")
#define mk_quadfn(s)   mk_filename(s,".quad")
#define mk_quad0fn(s)  mk_filename(s,".quad_")
#define mk_codefn(s)   mk_filename(s,".code")
#define mk_code0fn(s)  mk_filename(s,".code_")
#define mk_qidxfn(s)   mk_filename(s,".qidx")
#define mk_hitfn(s)    mk_filename(s,".hits")
#define mk_fieldfn(s)  mk_filename(s,".xyls")
#define mk_field0fn(s) mk_filename(s,".xyl0")
#define mk_idlistfn(s) mk_filename(s,".ids0")
#define mk_qlistfn(s)  mk_filename(s,".qds0")
#define mk_qmapfn(s)   mk_filename(s,".qmap")

#define freadcode(c,f) fread(c->farr,sizeof(double),DIM_CODES,f)
#define freadstar(s,f) fread(s->farr,sizeof(double),DIM_STARS,f)
#define fwritestar(s,f) fwrite((double *)s->farr,sizeof(double),DIM_STARS,f)
#define fprintfstar(s,f) fprintf(f,"%lf,%lf,%lf\n",star_ref(s,0),star_ref(s,1),star_ref(s,2))
#define fseekocat(i,p,f) fseeko(f,p+i*(DIM_STARS*sizeof(double)),SEEK_SET)

void readonequad(FILE *fid, qidx *iA, qidx *iB, qidx *iC, qidx *iD);
void writeonequad(FILE *fid, qidx iA, qidx iB, qidx iC, qidx iD);
void readonecode(FILE *fid, double *Cx, double *Cy, double *Dx, double *Dy);
void writeonecode(FILE *fid, double Cx, double Cy, double Dx, double Dy);

stararray *readcat(FILE *fid, sidx *numstars, dimension *Dim_Stars,
                  double *ramin, double *ramax, double *decmin, double *decmax,
				   int nkeep);

quadarray *readidlist(FILE *fid, qidx *numpix);

char read_objs_header(FILE *fid, sidx *numstars, dimension *DimStars,
                double *ramin, double *ramax, double *decmin, double *decmax);
char read_code_header(FILE *fid, qidx *numcodes, sidx *numstars,
                      dimension *DimCodes, double *index_scale);
char read_quad_header(FILE *fid, qidx *numquads, sidx *numstars,
                      dimension *DimQuads, double *index_scale);
void write_objs_header(FILE *fid, sidx numstars,
							  dimension DimStars, double ramin, double ramax, 
							  double decmin, double decmax);
void write_code_header(FILE *codefid, qidx numCodes,
                       sidx numstars, dimension DimCodes, double index_scale);
void write_quad_header(FILE *quadfid, qidx numQuads, sidx numstars,
                       dimension DimQuads, double index_scale);
void fix_code_header(FILE *codefid, qidx numCodes, size_t len);
void fix_quad_header(FILE *quadfid, qidx numQuads, size_t len);

xyarray *readxy(FILE *fid, char ParityFlip);

kdtree *read_starkd(FILE *treefid, double *ramin, double *ramax,
                    double *decmin, double *decmax);
kdtree *read_codekd(FILE *treefid, double *index_scale);
void write_starkd(FILE *treefid, kdtree *starkd,
                  double ramin, double ramax, double decmin, double decmax);
void write_codekd(FILE *treefid, kdtree *codekd, double index_scale);

char *mk_filename(const char *basename, const char *extension);

sidx readquadidx(FILE *fid, sidx **starlist, qidx **starnumq,
                 qidx ***starquads);
signed int compare_sidx(const void *x, const void *y);

typedef struct file_struct
{
	FILE *fid;
	char *fname;
	char ASCII;
	char itemSize;
}
FileObj;


typedef unsigned short int magicval;
#define MAGIC_VAL 0xFF00
#define ASCII_VAL 0x754E


#endif
