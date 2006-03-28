#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <stdio.h>
#include "starutil.h"
#include "xylist.h"

#define COMMENT_CHAR 35 // #
#define FOPEN_ERR -301
#define READ_FAIL -1

#define fopenout(n,f) {if(n){f=fopen(n,"w");} if(!n||!f){fprintf(stderr,"ERROR OPENING FILE %s for writing.\n",n); return(FOPEN_ERR);}}
#define fopenin(n,f)  {if(n){f=fopen(n,"r");} if(!n||!f){fprintf(stderr,"ERROR OPENING FILE %s for reading.\n",n); return(FOPEN_ERR);}}
#define free_fn(n) {if(n) free(n);}

#define mk_catfn(s)    mk_filename(s,".objs")
#define mk_fits_streefn(s)  mk_filename(s,".skdt.fits")
#define mk_streefn(s)  mk_filename(s,".skdt")
#define mk_ctreefn(s)  mk_filename(s,".ckdt")
#define mk_quadfn(s)   mk_filename(s,".quad")
#define mk_fits_quadfn(s)   mk_filename(s,".quad.fits")
#define mk_codefn(s)   mk_filename(s,".code")
#define mk_fits_codefn(s)   mk_filename(s,".code.fits")
#define mk_qidxfn(s)   mk_filename(s,".qidx")
#define mk_hitfn(s)    mk_filename(s,".hits")
#define mk_fieldfn(s)  mk_filename(s,".xyls")
#define mk_field0fn(s) mk_filename(s,".xyl0")
#define mk_idlistfn(s) mk_filename(s,".ids0")
#define mk_qlistfn(s)  mk_filename(s,".qds0")
#define mk_rdlsfn(s)  mk_filename(s,".rdls")

uint read_quadidx(FILE *fid, uint **starlist, uint **starnumq,
                  uint ***starquads);

char *mk_filename(const char *basename, const char *extension);

typedef unsigned short int magicval;
#define MAGIC_VAL 0xFF00
#define ASCII_VAL 0x754E

#endif
