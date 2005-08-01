#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hf:o:"
const char HelpString[]="fieldquads -f fname -o fieldname\n";


extern char *optarg;
extern int optind, opterr, optopt;

void find_fieldquads(FILE *qlistfid, quadarray *thepids, sidx numstars,
		     sidx *starlist, qidx *starnumq, qidx **starquads);

char *pixfname=NULL;
char *qidxfname=NULL;
char *qlistfname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  if(argc<=4) {fprintf(stderr,HelpString); return(OPT_ERR);}

  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'f':
	qidxfname = malloc(strlen(optarg)+6);
	sprintf(qidxfname,"%s.qidx",optarg);
	break;
      case 'o':
	pixfname = malloc(strlen(optarg)+6);
	qlistfname = malloc(strlen(optarg)+6);
	sprintf(pixfname,"%s.ids0",optarg);
	sprintf(qlistfname,"%s.qds0",optarg);
	break;
      case '?':
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      case 'h':
	fprintf(stderr,HelpString);
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  if(optind<argc) {
    for (argidx = optind; argidx < argc; argidx++)
      fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
    fprintf(stderr,HelpString);
    return(OPT_ERR);
  }

  qidx numpix;
  sidx ii,numstars;
  sizev *pixsizes;
  FILE *pixfid=NULL,*qidxfid=NULL,*qlistfid=NULL;
  sidx *starlist;
  qidx *starnumq;
  qidx **starquads;

  fprintf(stderr,"fieldquads: finding quads from %s appearing in %s\n",
	  qidxfname,pixfname);

  fprintf(stderr,"  Reading star ids...");fflush(stderr);
  fopenin(pixfname,pixfid); fnfree(pixfname);
  quadarray *thepids = readidlist(pixfid,&numpix,&pixsizes);
  fclose(pixfid);
  if(thepids==NULL) return(1);
  fprintf(stderr,"processed %lu fields.\n",numpix);

  fprintf(stderr,"  Reading quad index...");fflush(stderr);
  fopenin(qidxfname,qidxfid); fnfree(qidxfname);
  numstars=readquadidx(qidxfid,&starlist,&starnumq,&starquads);
  fclose(qidxfid);
  if(numstars==0) {
     fprintf(stderr,"ERROR (fieldquads) -- out of memory\n"); return(2);}
  fprintf(stderr,"got %lu star ids.\n",numstars);

  fprintf(stderr,"  Finding quads in fields...");fflush(stderr);
  fopenout(qlistfname,qlistfid); fnfree(qlistfname);
  find_fieldquads(qlistfid,thepids,numstars,starlist,starnumq,starquads);
  fclose(qlistfid);
  fprintf(stderr,"done.\n");

  free_quadarray(thepids); 
  free_sizev(pixsizes);
  for(ii=0;ii<numstars;ii++)
    free(starquads[ii]);
  free(starquads);
  free(starnumq);
  free(starlist);

  //basic_am_malloc_report();
  return(0);
}




void find_fieldquads(FILE *qlistfid, quadarray *thepids, sidx numstars,
		     sidx *starlist, qidx *starnumq, qidx **starquads)
{
  qidx ii,jj,newpos;
  sidx kk,*thisstar,starno,mm;
  quad *thispids;
  ivec *quadlist,*quadcount;

  for(ii=0;ii<thepids->size;ii++) {
    thispids=thepids->array[ii];
    fprintf(qlistfid,"%lu:",ii);
    quadlist=mk_ivec(0); quadcount=mk_ivec(0);
    for(kk=0;kk<ivec_size(thispids);kk++) {
      starno = (sidx)ivec_ref(thispids,kk);
      thisstar = (sidx *)bsearch(&starno,starlist,numstars,
				  sizeof(sidx *),compare_sidx);
      if(thisstar!=NULL) {
	mm=(sidx)(thisstar-starlist);
	for(jj=0;jj<starnumq[mm];jj++) {
	  newpos = add_to_ivec_unique2(quadlist,*(starquads[mm]+jj));
	  if(newpos>=quadcount->size) {
	    add_to_ivec(quadcount,1);
	  }
	  else 
	    quadcount->iarr[newpos]++;
	}
      }
    }

    for(jj=0;jj<quadlist->size;jj++) {
      if(quadcount->iarr[jj]>=DIM_QUADS) 
	fprintf(qlistfid," %d",quadlist->iarr[jj]);
    }
    fprintf(qlistfid,"\n");

    free_ivec(quadlist); 
    free_ivec(quadcount);

  }

  return;
}



