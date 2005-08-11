#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "hf:x:y:z:w:t:k:"
const char HelpString[]=
"findcode -f fname [-t dist | -k kNN] c1 c2 c3 c4  OR\n"
"findcode -f fname [-t dist | -k kNN] and read stdin\n";

extern char *optarg;
extern int optind, opterr, optopt;

void output_code(FILE *fid, qidx i, code *c);

char *treefname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  char kset=0,dtolset=0;
  sidx K=0;
  double dtol=0.0;

  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'k':
	K = strtoul(optarg,NULL,0);
	kset=1;
	break;
      case 't':
	dtol = strtod(optarg,NULL);
	dtolset=1;
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

  FILE *treefid=NULL;
  qidx ii;
  double index_scale;
  kquery *kq=NULL;
  kresult *krez=NULL;
  code *thequery=mk_code();

  if((argc-optind)<DIM_CODES) {
    fprintf(stderr,HelpString);
    return(HELP_ERR);
  }    

  fprintf(stderr,"findcode: getting codes from %s\n",treefname);
  fprintf(stderr,"  Reading code KD tree...");  fflush(stderr);
  fopenin(treefname,treefid); free_fn(treefname);
  kdtree *codekd = read_codekd(treefid,&index_scale);
  fclose(treefid);
  if(codekd==NULL) return(2);
  fprintf(stderr,"done\n    (%d codes, %d nodes, depth %d).\n",
	  kdtree_num_points(codekd),kdtree_num_nodes(codekd),
	  kdtree_max_depth(codekd));
  fprintf(stderr,"    (index scale = %f arcmin)\n",rad2arcmin(index_scale));

  codearray *thecodes = (codearray *)mk_dyv_array_from_kdtree(codekd);

  if(kset) 
    kq = mk_kquery("knn","",K,KD_UNDEF,codekd->rmin);
  else if(dtolset) 
    kq = mk_kquery("rangesearch","",KD_UNDEF,dtol,codekd->rmin);

  if(kset)
    fprintf(stderr,"  getting %lu codes nearest query\n",K);
  else if(dtolset)
    fprintf(stderr,"  getting codes within %lf of query\n",dtol);
  else 
    fprintf(stderr,"  ERROR (findcode)\n");


  while((argc-optind)>=4) {
    code_set(thequery,0,strtod(argv[optind++],NULL));
    code_set(thequery,1,strtod(argv[optind++],NULL));
    code_set(thequery,2,strtod(argv[optind++],NULL));
    code_set(thequery,3,strtod(argv[optind++],NULL));
 
    krez =  mk_kresult_from_kquery(kq,codekd,thequery);
    
    for(ii=0;ii<krez->count;ii++)
      output_code(stdout,krez->pindexes->iarr[ii],
		  thecodes->array[(krez->pindexes->iarr[ii])]);
    
    if(krez) free_kresult(krez);
  }

  if(kq) free_kquery(kq);
  if(thequery) free_code(thequery);

  free_dyv_array_from_kdtree((dyv_array *)thecodes);
  free_kdtree(codekd); 

  //basic_am_malloc_report();
  return(0);
}


void output_code(FILE *fid, qidx i, code *c)
{
  fprintf(fid,"%lu: %lf,%lf,%lf,%lf\n",
	  i,code_ref(c,0),code_ref(c,1),code_ref(c,2),code_ref(c,3));
  return;
}
