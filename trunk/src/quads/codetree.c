#include <unistd.h>
#include <stdio.h>
#include "starutil.h"

#define OPTIONS "hR:f:"
extern char *optarg;
extern int optind, opterr, optopt;

#define mk_codekdtree(c,r) mk_kdtree_from_points((dyv_array *)c,r)

codearray *readcodes(FILE *fid,qidx *numcodes, dimension *Dim_Codes);


char *treefname=NULL;
char *codefname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;
  dimension Dim_Codes;
  int kd_Rmin=50;
     
  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'R':
	kd_Rmin = (int)strtoul(optarg,NULL,0);
	break;
      case 'f':
	treefname = malloc(strlen(optarg)+6);
	codefname = malloc(strlen(optarg)+6);
	sprintf(treefname,"%s.ckdt",optarg);
	sprintf(codefname,"%s.code",optarg);
	break;
      case '?':
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      case 'h':
	fprintf(stderr, 
	"codetree [-f fname] [-R KD_RMIN]\n");
	return(1);
      default:
	return(2);
      }

  for (argidx = optind; argidx < argc; argidx++)
    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);

  FILE *codefid=NULL,*treefid=NULL;
  qidx numcodes;

  fprintf(stderr,"codetree: building KD tree for %s\n",codefname);

  fprintf(stderr,"  Reading codes...");fflush(stderr);
  fopenin(codefname,codefid); fnfree(codefname);
  codearray *thecodes = readcodes(codefid,&numcodes,&Dim_Codes);
  fclose(codefid);
  if(thecodes==NULL) return(1);
  fprintf(stderr,"got %lu codes (dim %hu).\n",numcodes,Dim_Codes);

  fprintf(stderr,"  Building code KD tree...");fflush(stderr);
  kdtree *codekd = mk_codekdtree(thecodes,kd_Rmin);
  if(codekd==NULL) return(2);
  fprintf(stderr,"done (%d nodes, depth %d).\n",
	  codekd->num_nodes,codekd->max_depth);

  fprintf(stderr,"  Writing code KD tree to ");
  if(treefname) fprintf(stderr,"%s...",treefname); 
  else fprintf(stderr,"stdout..."); 
  fflush(stderr);
  fopenout(treefname,treefid); fnfree(treefname);
  fwrite_kdtree(codekd,treefid);
  fprintf(stderr,"done.\n");
  fclose(treefid);


  free_codearray(thecodes);
  free_kdtree(codekd);

  //basic_am_malloc_report();
  return(0);

}



codearray *readcodes(FILE *fid,qidx *numcodes,dimension *Dim_Codes)
{
  char ASCII = 0;
  qidx ii;
  magicval magic;
  fread(&magic,sizeof(magic),1,fid);
  if(magic==ASCII_VAL) {
    ASCII=1;
    fscanf(fid,"mCodes=%lu\n",numcodes);
    fscanf(fid,"DimCodes=%hu\n",Dim_Codes);
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (codetree) -- bad magic value in %s\n",codefname);
      return((codearray *)NULL);
    }
    ASCII=0;
    fread(numcodes,sizeof(*numcodes),1,fid);
    fread(Dim_Codes,sizeof(*Dim_Codes),1,fid);
  }
  codearray *thecodes = mk_codearray(*numcodes);
  for(ii=0;ii<*numcodes;ii++) {
    thecodes->array[ii] = mk_coded(*Dim_Codes);
    if(thecodes->array[ii]==NULL) {
      fprintf(stderr,"ERROR (codetree) -- out of memory at code %lu\n",ii);
      free_codearray(thecodes);
      return (codearray *)NULL;
    }
    if(ASCII) {
      if(*Dim_Codes==4)
	fscanf(fid,"%lf,%lf,%lf,%lf\n",thecodes->array[ii]->farr,
  	       thecodes->array[ii]->farr+1,
  	       thecodes->array[ii]->farr+2,
  	       thecodes->array[ii]->farr+3   );
    }
    else
      fread(thecodes->array[ii]->farr,sizeof(double),*Dim_Codes,fid);
  }
  return thecodes;
}







