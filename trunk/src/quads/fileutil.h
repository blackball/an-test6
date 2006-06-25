#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <stdio.h>
#include "starutil.h"
#include "xylist.h"

#define COMMENT_CHAR 35 // #
#define FOPEN_ERR -301
#define READ_FAIL -1

bool file_exists(char* fn);

void fopenout(char* fn, FILE** pfid);

#define fopenin(n,f)  {if(n){f=fopen(n,"r");} if(!n||!f){fprintf(stderr,"ERROR OPENING FILE %s for reading.\n",n); return(FOPEN_ERR);}}
#define free_fn(n) {free(n);}

#define mk_catfn(s)    mk_filename(s,".objs.fits")
#define mk_idfn(s)    mk_filename(s,".id.fits")
#define mk_streefn(s)  mk_filename(s,".skdt.fits")
#define mk_ctreefn(s)  mk_filename(s,".ckdt.fits")
#define mk_quadfn(s)   mk_filename(s,".quad.fits")
#define mk_codefn(s)   mk_filename(s,".code.fits")
#define mk_qidxfn(s)   mk_filename(s,".qidx.fits")
#define mk_hitfn(s)    mk_filename(s,".hits")
#define mk_fieldfn(s)  mk_filename(s,".fits")
#define mk_field0fn(s) mk_filename(s,".xyl0")
#define mk_idlistfn(s) mk_filename(s,".ids0")
#define mk_qlistfn(s)  mk_filename(s,".qds0")
#define mk_rdlsfn(s)  mk_filename(s,".rdls")

char *mk_filename(const char *basename, const char *extension);

#endif
