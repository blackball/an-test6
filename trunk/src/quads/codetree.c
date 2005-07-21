#include <unistd.h>
#include <stdio.h>
#include "starutil.h"

#define OPTIONS "hR:f:B:"
#define MEM_LOAD 1000000000
extern char *optarg;
extern int optind, opterr, optopt;

#define mk_codekdtree(c,r) mk_kdtree_from_points((dyv_array *)c,r)

codearray *readcodes(FILE *fid, qidx *numcodes, dimension *Dim_Codes, 
		     char *ASCII,double *index_scale,qidx buffsize);

dimension readonecode(FILE *codefid, code *tmpcode, 
		 dimension Dim_Codes, char ASCII);

char *treefname=NULL;
char *codefname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;
  int kd_Rmin=50;
  qidx buffsize=(qidx)floor(MEM_LOAD/(sizeof(double)+sizeof(int))*DIM_CODES);
     
  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'R':
	kd_Rmin = (int)strtoul(optarg,NULL,0);
	break;
      case 'B':
	buffsize = strtoul(optarg,NULL,0);
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
	"codetree [-f fname] [-B buffer_length] [-R KD_RMIN]\n");
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  for (argidx = optind; argidx < argc; argidx++)
    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);

  FILE *codefid=NULL,*treefid=NULL;
  qidx numcodes,ii;
  code *tmpcode;
  dimension Dim_Codes;
  double index_scale;
  char ASCII;

  fprintf(stderr,"codetree: building KD tree for %s\n",codefname);

  fprintf(stderr,"  Reading codes...");fflush(stderr);
  fopenin(codefname,codefid); fnfree(codefname);
  codearray *thecodes=readcodes(codefid,&numcodes,&Dim_Codes,
				&ASCII,&index_scale,buffsize);
  if(thecodes==NULL) return(1);
  fprintf(stderr,"got %d codes (dim %hu).\n",thecodes->size,Dim_Codes);

  fprintf(stderr,"  Building code KD tree...\n");fflush(stderr);
  kdtree *codekd = mk_codekdtree(thecodes,kd_Rmin);
  free_codearray(thecodes);
  if(codekd==NULL) return(2);

  if(numcodes>buffsize) {
    fprintf(stderr,"    buffer codes (%d nodes, depth %d).\n",
	    codekd->num_nodes,codekd->max_depth);
    tmpcode=mk_coded(Dim_Codes);
    for(ii=0;ii<(numcodes-buffsize);ii++) {
      readonecode(codefid,tmpcode,Dim_Codes,ASCII);
      add_point_to_kdtree(codekd,(dyv *)tmpcode);
      if(is_power_of_two(ii+1)) 
	fprintf(stderr,"    %lu / %lu of rest done\r",ii+1,numcodes-buffsize);
    }
    free_code(tmpcode);
  }
  fprintf(stderr,"    done (%d nodes, depth %d).\n",
	  codekd->num_nodes,codekd->max_depth);
  fclose(codefid);

  fprintf(stderr,"  Writing code KD tree to ");
  if(treefname) fprintf(stderr,"%s...",treefname); 
  else fprintf(stderr,"stdout..."); 
  fflush(stderr);
  fopenout(treefname,treefid); fnfree(treefname);
  fwrite_kdtree(codekd,treefid);
  fwrite(&index_scale,sizeof(index_scale),1,treefid);
  fprintf(stderr,"done.\n");
  fclose(treefid);


  free_kdtree(codekd);

  //basic_am_malloc_report();
  return(0);

}



codearray *readcodes(FILE *fid, qidx *numcodes, dimension *Dim_Codes, 
		     char *ASCII,double *index_scale,qidx buffsize)
{
  qidx ii;
  sidx numstars;
  magicval magic;
  fread(&magic,sizeof(magic),1,fid);
  if(magic==ASCII_VAL) {
    *ASCII=1;
    fscanf(fid,"mCodes=%lu\n",numcodes);
    fscanf(fid,"DimCodes=%hu\n",Dim_Codes);
    fscanf(fid,"IndexScale=%lf\n",index_scale);
    fscanf(fid,"NumStars=%lu\n",&numstars);
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (codetree) -- bad magic value in %s\n",codefname);
      return((codearray *)NULL);
    }
    *ASCII=0;
    fread(numcodes,sizeof(*numcodes),1,fid);
    fread(Dim_Codes,sizeof(*Dim_Codes),1,fid);
    fread(index_scale,sizeof(*index_scale),1,fid);
  }
  if(*numcodes< buffsize) buffsize=*numcodes;
  codearray *thecodes = mk_codearray(buffsize);
  
  for(ii=0;ii<buffsize;ii++) {
    thecodes->array[ii] = mk_coded(*Dim_Codes);
    if(thecodes->array[ii]==NULL) {
      fprintf(stderr,"ERROR (codetree) -- out of memory at code %lu\n",ii);
      free_codearray(thecodes);
      return (codearray *)NULL;
    }
    if(*ASCII) {
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


dimension readonecode(FILE *codefid, code *tmpcode, 
		      dimension Dim_Codes, char ASCII)
{
  dimension rez=0;
  if(ASCII) {
    if(Dim_Codes==4)
      rez=(dimension)fscanf(codefid,"%lf,%lf,%lf,%lf\n",tmpcode->farr,
	     tmpcode->farr+1,tmpcode->farr+2,tmpcode->farr+3   );
    else
      fprintf(stderr,"ERROR (codetree) -- only DimCodes=4 supported.\n");
  }
  else
    rez=(dimension)fread(tmpcode->farr,sizeof(double),Dim_Codes,codefid);

  if(rez!=Dim_Codes) 
    fprintf(stderr,"ERROR (codetree) -- can't read next code\n"); 

  return rez;
}






