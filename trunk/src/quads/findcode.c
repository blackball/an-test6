#include <errno.h>
#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "hf:ixrt:k:"
const char HelpString[]=
"findcode -f fname [-t dist | -k kNN] c1 c2 c3 c4  OR\n"
"findcode -f fname [-t dist | -k kNN] then read 4-tuples from stdin\n";

extern char *optarg;
extern int optind, opterr, optopt;

void output_code(FILE *fid, qidx i, codearray *ca);

char *treefname=NULL;
FILE *treefid=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  if(argc<=2) {fprintf(stderr,HelpString); return(OPT_ERR);}

  char kset=1,dtolset=0;
  sidx K=1;
  double dtol=0.0;
     
  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'k':
	K = strtoul(optarg,NULL,0);
	if(K<1) K=1;
	kset=1;	dtolset=0;
	break;
      case 't':
	dtol = strtod(optarg,NULL);
	if(dtol<0.0) dtol=0.0;
	dtolset=1; kset=0;
	break;
      case 'f':
	treefname = mk_ctreefn(optarg);
	break;
      case '?':
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      case 'h':
	fprintf(stderr,HelpString);
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  //  if(??) {
  //    fprintf(stderr,HelpString); return(OPT_ERR);}

  qidx numcodes,ii;
  double c1,c2,c3,c4;
  kquery *kq=NULL;
  kresult *krez=NULL;
  code *thequery=NULL;
  kdtree *codekd=NULL;
  codearray *thecodes=NULL;
  double index_scale;


 {
    fprintf(stderr,"findcode: getting codes from %s\n",treefname);
    fprintf(stderr,"  Reading code KD tree...");  fflush(stderr);
    fopenin(treefname,treefid); free_fn(treefname);
    codekd=read_codekd(treefid,&index_scale);
    fclose(treefid);
    if(codekd==NULL) return(2);
    numcodes=codekd->root->num_points;
    fprintf(stderr,"done\n");
    fprintf(stderr,"(%lu codes, index_scale %lf, %d nodes, depth %d).\n",
	    numcodes,index_scale,codekd->num_nodes,codekd->max_depth);
    thecodes = (codearray *)mk_dyv_array_from_kdtree(codekd);
    thequery = mk_code();
  }

  if(kset)
    kq = mk_kquery("knn","",K,KD_UNDEF,codekd->rmin);
  else if(dtolset)
    kq = mk_kquery("rangesearch","",KD_UNDEF,dtol,codekd->rmin);

  //fprintf(stderr,"optind=%d,argc=%d\n",optind,argc);

  if(optind<argc){
    argidx=optind;
    while(argidx<argc) {
      //fprintf(stderr,"argv[%d]=%s\n",argidx,argv[argidx]);
      //argidx++;
      { 
	if(argidx<(argc-3)) {
	  c1=strtod(argv[argidx++],NULL);
	  c2=strtod(argv[argidx++],NULL);
	  c3=strtod(argv[argidx++],NULL);
	  c4=strtod(argv[argidx++],NULL);
	  code_set(thequery,0,c1); 
	  code_set(thequery,1,c2); 
	  code_set(thequery,2,c3); 
	  code_set(thequery,3,c4); 
	  if(kset)
           fprintf(stderr,"  getting %lu codes closest to (%lf,%lf,%lf,%lf)\n",
		   K,c1,c2,c3,c4);
	  else if(dtolset)
           fprintf(stderr,"  codes within %lf of (%lf,%lf,%lf,%lf)\n",
		   dtol,c1,c2,c3,c4);

	  krez =  mk_kresult_from_kquery(kq,codekd,thequery);
	  if(krez!=NULL) {
	    for(ii=0;ii<krez->count;ii++)
	      output_code(stdout,krez->pindexes->iarr[ii],thecodes);
	    free_kresult(krez);
	  }
	}
      }
    }
  }
  else {
    char scanrez=1;
    while(!feof(stdin) && scanrez) {
      {
	scanrez=fscanf(stdin,"%lf %lf %lf %lf",&c1,&c2,&c3,&c4);
	if(scanrez==4) {
	  //fprintf(stderr,"read x=%lf,y=%lf,z=%lf\n",xx,yy,zz);
	  code_set(thequery,0,c1); 
	  code_set(thequery,1,c2); 
	  code_set(thequery,2,c3); 
	  code_set(thequery,3,c4); 

	  if(kset)
           fprintf(stderr,"  getting %lu codes closest to (%lf,%lf,%lf,%lf)\n",
		   K,c1,c2,c3,c4);
	  else if(dtolset)
           fprintf(stderr,"  codes within %lf of (%lf,%lf,%lf,%lf)\n",
		   dtol,c1,c2,c3,c4);

	  krez =  mk_kresult_from_kquery(kq,codekd,thequery);
	  if(krez!=NULL) {
	    for(ii=0;ii<krez->count;ii++)
	      output_code(stdout,krez->pindexes->iarr[ii],thecodes);
	    free_kresult(krez);
	  }
	}
      }
    }
  }

  if(kq) free_kquery(kq);
  if(thequery) free_code(thequery);
  free_dyv_array_from_kdtree((dyv_array *)thecodes);
  free_kdtree(codekd); 

  //basic_am_malloc_report();
  return(0);
}


void output_code(FILE *fid, qidx i, codearray *ca)
{
  code *tmpc=NULL;

  tmpc=ca->array[i];

  fprintf(fid,"%lu: %lf,%lf,%lf,%lf\n",i,code_ref(tmpc,0),
	  code_ref(tmpc,1),code_ref(tmpc,2),code_ref(tmpc,3));

  return;
}


