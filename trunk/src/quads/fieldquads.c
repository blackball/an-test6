#include <unistd.h>
#include <stdio.h>
#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hf:o:"
extern char *optarg;
extern int optind, opterr, optopt;

quadarray *readidlist(FILE *fid,qidx *numpix,sizev **pixsizes);
void find_fieldquads(FILE *qlistfid, quadarray *thepids,
		     sidx *starlist, qidx *starnumq, qidx **starquads);
sidx readquadidx(FILE *fid, sidx **starlist, qidx **starnumq, 
		 qidx ***starquads);

char *pixfname=NULL;
char *qidxfname=NULL;
char *qlistfname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

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
	fprintf(stderr, 
	"fieldquads [-f fname] [-o fieldname]\n");
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  for (argidx = optind; argidx < argc; argidx++)
    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);


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

  fprintf(stderr,"  Finding quads in fields (slow)...");fflush(stderr);
  fopenout(qlistfname,qlistfid); fnfree(qlistfname);
  find_fieldquads(qlistfid,thepids,starlist,starnumq,starquads);
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
      fprintf(stderr,"ERROR (fieldquads) -- bad magic value in %s\n",pixfname);
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
      fprintf(stderr,"ERROR (fieldquads) -- out of memory at pix %lu\n",ii);
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




void find_fieldquads(FILE *qlistfid, quadarray *thepids,
		     sidx *starlist, qidx *starnumq, qidx **starquads)
{
  qidx ii,jj;
  sidx kk,thisstar;
  quad *thispids,*thisquad;
  for(ii=0;ii<thepids->size;ii++) {
    thispids=thepids->array[ii];
    fprintf(qlistfid,"%lu:",ii);
    for(kk=0;kk<ivec_size(thispids);kk++) {
      /*
      thisstar=find(ivec_ref(thispids,kk),starlist);
      if(found) {add all starnumq[thisstar] quads in starquads[thisstar][?]
		   to the running quadlist;}
      foreach(item on quadlist) {
	check if all stars in this pic have that item on their list;
      }
      for(jj=0;jj<???;jj++) {

      }
      */
    }

    //	if(???) fprintf(qlistfid,",%lu",jj);

    fprintf(qlistfid,"\n");
  }
  return;
}



sidx readquadidx(FILE *fid, sidx **starlist, qidx **starnumq, 
		 qidx ***starquads)
{
  char ASCII=READ_FAIL;
  magicval magic;
  sidx numStars,thisstar,jj;
  qidx thisnumq,ii;

  fread(&magic,sizeof(magic),1,fid);
  if(magic==ASCII_VAL) {
    ASCII=1;
    fscanf(fid,"mIndexedStars=%lu\n",&numStars);
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (fieldquads) -- bad magic value in quad index\n");
      return(0);
    }
    ASCII=0;
    fread(&numStars,sizeof(numStars),1,fid);
  }
  *starlist=malloc(numStars*sizeof(sidx)); 
  if(*starlist==NULL) return(0);
  *starnumq=malloc(numStars*sizeof(qidx)); 
  if(*starnumq==NULL) {free(*starlist); return(0);}
  *starquads=malloc(numStars*sizeof(qidx *));
  if(*starquads==NULL) {free(*starlist); free(*starnumq); return(0);}

  for(jj=0;jj<numStars;jj++) {
    if(ASCII) {
      fscanf(fid,"%lu:%lu",&thisstar,&thisnumq);
    }
    else {
      fread(&thisstar,sizeof(thisstar),1,fid);
      fread(&thisnumq,sizeof(thisnumq),1,fid);
    }
    (*starlist)[jj]=thisstar;
    (*starnumq)[jj]=thisnumq;
    (*starquads)[jj]=malloc(thisnumq*sizeof(qidx));
    if((*starquads)[jj]==NULL) return(0);
    for(ii=0;ii<thisnumq;ii++) {
      if(ASCII)
	fscanf(fid,",%lu",((*starquads)[jj])+ii);
      else
	fread(((*starquads)[jj])+ii,sizeof(qidx),1,fid);
    }
    if(ASCII) fscanf(fid,"\n");
  }

  return(numStars);
}
