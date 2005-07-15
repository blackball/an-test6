#include <unistd.h>
#include <stdio.h>
#include "starutil.h"

#define OPTIONS "hf:o:"
extern char *optarg;
extern int optind, opterr, optopt;

quadarray *readidlist(FILE *fid,qidx *numpix,sizev **pixsizes);
quadarray *readquadlist(FILE *fid,qidx *numquads);
void find_pixquads(FILE *qlistfid, quadarray *thepids,quadarray *thequads);

char *pixfname=NULL;
char *quadfname=NULL;
char *qlistfname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'f':
	quadfname = malloc(strlen(optarg)+6);
	sprintf(quadfname,"%s.quad",optarg);
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
	fprintf(stderr, 
	"pixquads [-f fname] [-o pixname]\n");
	return(1);
      default:
	return(2);
      }

  for (argidx = optind; argidx < argc; argidx++)
    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);


  qidx numpix,numquads;
  sizev *pixsizes;
  FILE *pixfid=NULL,*quadfid=NULL,*qlistfid=NULL;

  fprintf(stderr,"pixquads: finding quads from %s appearing in %s\n",
	  quadfname,pixfname);

  fprintf(stderr,"  Reading pix ids...");fflush(stderr);
  fopenin(pixfname,pixfid); fnfree(pixfname);
  quadarray *thepids = readidlist(pixfid,&numpix,&pixsizes);
  fclose(pixfid);
  if(thepids==NULL) return(1);
  fprintf(stderr,"got %lu pix.\n",numpix);

  fprintf(stderr,"  Reading quads...");fflush(stderr);
  fopenin(quadfname,quadfid); fnfree(quadfname);
  quadarray *thequads = readquadlist(quadfid,&numquads);
  fclose(quadfid);
  if(thequads==NULL) return(2);
  fprintf(stderr,"got %lu quads.\n",numquads);

  fprintf(stderr,"  Finding quads in pix (slow)...");fflush(stderr);
  fopenout(qlistfname,qlistfid); fnfree(qlistfname);
  find_pixquads(qlistfid,thepids,thequads);
  fclose(qlistfid);
  fprintf(stderr,"done.\n");

  free_quadarray(thepids); 
  free_quadarray(thequads); 
  free_sizev(pixsizes);

  //basic_am_malloc_report();
  return(0);
}




quadarray *readidlist(FILE *fid,qidx *numpix,sizev **pixsizes)
{
  char ASCII = 0;
  qidx ii,jj,numS;
  magicval magic;
  fread(&magic,sizeof(magic),1,fid);
  if(magic==ASCII_VAL) {
    ASCII=1;
    fscanf(fid,"mFields=%lu\n",numpix);
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (pixquads) -- bad magic value in %s\n",pixfname);
      return((quadarray *)NULL);
    }
    ASCII=0;
    fread(numpix,sizeof(*numpix),1,fid);
  }
  quadarray *thepids = mk_quadarray(*numpix);
  *pixsizes = mk_sizev(*numpix);
  for(ii=0;ii<*numpix;ii++) {
    // read in how many stars in this pic
    if(ASCII)
      fscanf(fid,"%lu",&numS);
    else
      fread(&numS,sizeof(numS),1,fid);
    sizev_set(*pixsizes,ii,numS);
    thepids->array[ii] = mk_quadd(numS);
    if(thepids->array[ii]==NULL) {
      fprintf(stderr,"ERROR (pixquads) -- out of memory at pix %lu\n",ii);
      free_quadarray(thepids);
      free_sizev(*pixsizes);
      return (quadarray *)NULL;
    }
    if(ASCII) {
      for(jj=0;jj<numS;jj++)
	fscanf(fid,",%d",thepids->array[ii]->iarr+jj);
      fscanf(fid,"\n");
    }
    else
      fread(thepids->array[ii]->iarr,sizeof(int),numS,fid);
  }
  return thepids;
}




quadarray *readquadlist(FILE *fid,qidx *numquads)
{
  char ASCII = 0;
  qidx ii;
  dimension Dim_Quads;
  magicval magic;
  fread(&magic,sizeof(magic),1,fid);
  if(magic==ASCII_VAL) {
    ASCII=1;
    fscanf(fid,"mQuads=%lu\n",numquads);
    fscanf(fid,"DimQuads=%hu\n",&Dim_Quads);
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (pixquads) -- bad magic value in %s\n",pixfname);
      return((quadarray *)NULL);
    }
    ASCII=0;
    fread(numquads,sizeof(*numquads),1,fid);
    fread(&Dim_Quads,sizeof(Dim_Quads),1,fid);
  }
  quadarray *thequads = mk_quadarray(*numquads);
  for(ii=0;ii<*numquads;ii++) {
    thequads->array[ii] = mk_quad();
    if(thequads->array[ii]==NULL) {
      fprintf(stderr,"ERROR (pixquads) -- out of memory at pix %lu\n",ii);
      free_quadarray(thequads);
      return (quadarray *)NULL;
    }
    if(ASCII) {
      fscanf(fid,"%d,%d,%d,%d\n",
	     (thequads->array[ii]->iarr),
	     (thequads->array[ii]->iarr)+1,
	     (thequads->array[ii]->iarr)+2,
	     (thequads->array[ii]->iarr)+3);
    }
    else
      fread(thequads->array[ii]->iarr,sizeof(int),DIM_QUADS,fid);
  }
  return thequads;
}


void find_pixquads(FILE *qlistfid, quadarray *thepids,quadarray *thequads)
{
  qidx ii,jj;
  sidx kk;
  quad *checkquad=mk_quad();
  quad *thispids,*thisquad;
  for(ii=0;ii<thepids->size;ii++) {
    thispids=thepids->array[ii];
    fprintf(qlistfid,"%lu:",ii);
    for(jj=0;jj<thequads->size;jj++) {
      thisquad=thequads->array[jj];
      quad_set(checkquad,0,0); quad_set(checkquad,1,0);
      quad_set(checkquad,2,0); quad_set(checkquad,3,0);
      for(kk=0;kk<ivec_size(thispids);kk++) {
       if(quad_ref(thispids,kk)==quad_ref(thisquad,0)) quad_set(checkquad,0,1);
       if(quad_ref(thispids,kk)==quad_ref(thisquad,1)) quad_set(checkquad,1,1);
       if(quad_ref(thispids,kk)==quad_ref(thisquad,2)) quad_set(checkquad,2,1);
       if(quad_ref(thispids,kk)==quad_ref(thisquad,3)) quad_set(checkquad,3,1);
      }
      if(quad_ref(checkquad,0) && quad_ref(checkquad,1) &&
	 quad_ref(checkquad,2) && quad_ref(checkquad,3) )
	fprintf(qlistfid,",%lu",jj);
    }
    fprintf(qlistfid,"\n");
  }
  free_quad(checkquad);
  return;
}
