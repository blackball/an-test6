#include "fileutil.h"
#include "kdutil.h"


void readonequad(FILE *fid,qidx *iA,qidx *iB,qidx *iC,qidx *iD)
{
  fread(iA,sizeof(*iA),1,fid);
  fread(iB,sizeof(*iB),1,fid);
  fread(iC,sizeof(*iC),1,fid);
  fread(iD,sizeof(*iD),1,fid);
  return;
}

void writeonequad(FILE *fid,qidx iA,qidx iB,qidx iC,qidx iD)
{
  fwrite(&iA,sizeof(iA),1,fid);
  fwrite(&iB,sizeof(iB),1,fid);
  fwrite(&iC,sizeof(iC),1,fid);
  fwrite(&iD,sizeof(iD),1,fid);
  return;
}

void readonecode(FILE *fid,double *Cx, double *Cy, double *Dx, double *Dy)
{
  fread(Cx,sizeof(*Cx),1,fid);
  fread(Cy,sizeof(*Cy),1,fid);
  fread(Dx,sizeof(*Dx),1,fid);
  fread(Dy,sizeof(*Dy),1,fid);
  return;
}

void writeonecode(FILE *fid,double Cx, double Cy, double Dx, double Dy)
{
  fwrite(&Cx,sizeof(Cx),1,fid);
  fwrite(&Cy,sizeof(Cy),1,fid);
  fwrite(&Dx,sizeof(Dx),1,fid);
  fwrite(&Dy,sizeof(Dy),1,fid);
  return;
}

stararray *readcat(FILE *fid,sidx *numstars, dimension *Dim_Stars,
	   double *ramin, double *ramax, double *decmin, double *decmax)
{
  char ASCII = READ_FAIL;
  sidx ii;

  ASCII=read_objs_header(fid,numstars,Dim_Stars,ramin,ramax,decmin,decmax);
  if(ASCII==READ_FAIL) return((stararray *)NULL);

  stararray *thestars = mk_stararray(*numstars);
  for(ii=0;ii<*numstars;ii++) {
    thestars->array[ii] = mk_stard(*Dim_Stars);
    if(thestars->array[ii]==NULL) {
      fprintf(stderr,"ERROR (readcat) -- out of memory at star %lu\n",ii);
      free_stararray(thestars);
      return (stararray *)NULL;
    }
    if(ASCII) {
      if(*Dim_Stars==2)
	fscanf(fid,"%lf,%lf\n",thestars->array[ii]->farr,
  	       thestars->array[ii]->farr+1   );
      else if(*Dim_Stars==3)
	fscanf(fid,"%lf,%lf,%lf\n",thestars->array[ii]->farr,
  	       thestars->array[ii]->farr+1,
  	       thestars->array[ii]->farr+2   );
    }
    else
      fread(thestars->array[ii]->farr,sizeof(double),*Dim_Stars,fid);
  }
  return thestars;
}

quadarray *readidlist(FILE *fid,qidx *numfields)
{
  char ASCII = 0;
  qidx ii,jj,numS;
  magicval magic;
  fread(&magic,sizeof(magic),1,fid);
  if(magic==ASCII_VAL) {
    ASCII=1;
    fscanf(fid,"mFields=%lu\n",numfields);
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (readidlist) -- bad magic value id list\n");
      return((quadarray *)NULL);
    }
    ASCII=0;
    fread(numfields,sizeof(*numfields),1,fid);
  }
  quadarray *thepids = mk_quadarray(*numfields);
  for(ii=0;ii<*numfields;ii++) {
    // read in how many stars in this pic
    if(ASCII)
      fscanf(fid,"%lu",&numS);
    else
      fread(&numS,sizeof(numS),1,fid);
    thepids->array[ii] = mk_quadd(numS);
    if(thepids->array[ii]==NULL) {
      fprintf(stderr,"ERROR (fieldquads) -- out of memory at field %lu\n",ii);
      free_quadarray(thepids);
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

void write_objs_header(FILE *fid, char ASCII, sidx numstars,
    dimension DimStars, double ramin,double ramax,double decmin,double decmax)
{
  if(ASCII) {
    fprintf(fid,"NumStars=%lu\n",numstars);
    fprintf(fid,"DimStars=%d\n",DimStars);
    fprintf(fid,"Limits=%f,%f,%f,%f\n",ramin,ramax,decmin,decmax);
  }
  else {
    magicval magic=MAGIC_VAL;
    fwrite(&magic,sizeof(magic),1,fid);
    fwrite(&numstars,sizeof(numstars),1,fid);
    fwrite(&DimStars,sizeof(DimStars),1,fid);
    fwrite(&ramin,sizeof(ramin),1,fid);
    fwrite(&ramax,sizeof(ramin),1,fid);
    fwrite(&decmin,sizeof(ramin),1,fid);
    fwrite(&decmax,sizeof(ramin),1,fid);
  }
  return;
}

void write_code_header(FILE *codefid, char ASCII, qidx numCodes, 
		       sidx numstars, dimension DimCodes, double index_scale)
{
  if(ASCII) {
    fprintf(codefid,"NumCodes=%lu\n",numCodes);
    fprintf(codefid,"DimCodes=%hu\n",DimCodes);
    fprintf(codefid,"IndexScale=%f\n",index_scale);
    fprintf(codefid,"NumStars=%lu\n",numstars);
  }
  else {
    magicval magic=MAGIC_VAL;
    fwrite(&magic,sizeof(magic),1,codefid);
    fwrite(&numCodes,sizeof(numCodes),1,codefid);
    fwrite(&DimCodes,sizeof(DimCodes),1,codefid);
    fwrite(&index_scale,sizeof(index_scale),1,codefid);
    fwrite(&numstars,sizeof(numstars),1,codefid);
  }
  return;

}

void write_quad_header(FILE *quadfid, char ASCII, qidx numQuads, sidx numstars,
		       dimension DimQuads, double index_scale)
{
  if(ASCII) {
    fprintf(quadfid,"NumQuads=%lu\n",numQuads);
    fprintf(quadfid,"DimQuads=%hu\n",DimQuads);
    fprintf(quadfid,"IndexScale=%f\n",index_scale);
    fprintf(quadfid,"NumStars=%lu\n",numstars);
  }
  else {
    magicval magic=MAGIC_VAL;
    fwrite(&magic,sizeof(magic),1,quadfid);
    fwrite(&numQuads,sizeof(numQuads),1,quadfid);
    fwrite(&DimQuads,sizeof(DimQuads),1,quadfid);
    fwrite(&index_scale,sizeof(index_scale),1,quadfid);
    fwrite(&numstars,sizeof(numstars),1,quadfid);
  }
  return;
}

void fix_code_header(FILE *codefid, char ASCII, qidx numCodes, size_t len)
{
  rewind(codefid);
  if(ASCII) {
    fprintf(codefid,"NumCodes=%2$*1$lu\n",len,numCodes);
  }
  else {
    magicval magic=MAGIC_VAL;
    fwrite(&magic,sizeof(magic),1,codefid);
    fwrite(&numCodes,sizeof(numCodes),1,codefid);
  }
  return;
}

void fix_quad_header(FILE *quadfid, char ASCII, qidx numQuads, size_t len)
{
  rewind(quadfid);
  if(ASCII) {
    fprintf(quadfid,"NumQuads=%2$*1$lu\n",len,numQuads);
  }
  else {
    magicval magic=MAGIC_VAL;
    fwrite(&magic,sizeof(magic),1,quadfid);
    fwrite(&numQuads,sizeof(numQuads),1,quadfid);
  }
  return;
}


char read_objs_header(FILE *fid, sidx *numstars, dimension *DimStars, 
		 double *ramin,double *ramax,double *decmin,double *decmax)
{
  char ASCII=READ_FAIL;
  magicval magic;
  fread(&magic,sizeof(magic),1,fid);
  if(magic==ASCII_VAL) {
    ASCII=1;
    fscanf(fid,"mStars=%lu\n",numstars);
    fscanf(fid,"DimStars=%hu\n",DimStars);
    fscanf(fid,"Limits=%lf,%lf,%lf,%lf\n",ramin,ramax,decmin,decmax);
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (read_objs_header) -- bad magic value\n");
      return(READ_FAIL);
    }
    ASCII=0;
    fread(numstars,sizeof(*numstars),1,fid);
    fread(DimStars,sizeof(*DimStars),1,fid);
    fread(ramin,sizeof(*ramin),1,fid);
    fread(ramax,sizeof(*ramax),1,fid);
    fread(decmin,sizeof(*decmin),1,fid);
    fread(decmax,sizeof(*decmax),1,fid);
  }

  return(ASCII);
}



char read_code_header(FILE *fid, qidx *numcodes, sidx *numstars,
		      dimension *DimCodes, double *index_scale)
{
  char ASCII=READ_FAIL;
  magicval magic;
  fread(&magic,sizeof(magic),1,fid);
  if(magic==ASCII_VAL) {
    ASCII=1;
    fscanf(fid,"mCodes=%lu\n",numcodes);
    fscanf(fid,"DimCodes=%hu\n",DimCodes);
    fscanf(fid,"IndexScale=%lf\n",index_scale);
    fscanf(fid,"NumStars=%lu\n",numstars);
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (read_code_header) -- bad magic value\n");
      return(READ_FAIL);
    }
    ASCII=0;
    fread(numcodes,sizeof(*numcodes),1,fid);
    fread(DimCodes,sizeof(*DimCodes),1,fid);
    fread(index_scale,sizeof(*index_scale),1,fid);
    fread(numstars,sizeof(*numstars),1,fid);
  }
  return(ASCII);

}

char read_quad_header(FILE *fid, qidx *numquads, sidx *numstars,
		      dimension *DimQuads, double *index_scale)
{
  char ASCII=READ_FAIL;
  magicval magic;
  fread(&magic,sizeof(magic),1,fid);
  if(magic==ASCII_VAL) {
    ASCII=1;
    fscanf(fid,"mQuads=%lu\n",numquads);
    fscanf(fid,"DimQuads=%hu\n",DimQuads);
    fscanf(fid,"IndexScale=%lf\n",index_scale);
    fscanf(fid,"NumStars=%lu\n",numstars);
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (read_quad_header) -- bad magic value\n");
      return(READ_FAIL);
    }
    ASCII=0;
    fread(numquads,sizeof(*numquads),1,fid);
    fread(DimQuads,sizeof(*DimQuads),1,fid);
    fread(index_scale,sizeof(*index_scale),1,fid);
    fread(numstars,sizeof(*numstars),1,fid);
  }
  return(ASCII);

}


xyarray *readxy(FILE *fid, char ParityFlip)
{
  char ASCII = 0;
  qidx ii,jj,numxy,numfields;
  magicval magic;
  if(fread(&magic,sizeof(magic),1,fid)!=1) {
    fprintf(stderr,"ERROR (readxy) -- bad magic value in field file.\n");
    return((xyarray *)NULL);
  }
  if(magic==ASCII_VAL) {
    ASCII=1;
    if(fscanf(fid,"mFields=%lu\n",&numfields)!=1) {
      fprintf(stderr,"ERROR (readxy) -- bad first line in field file.\n");
      return((xyarray *)NULL);
    }
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (readxy) -- bad magic value in field file.\n");
      return((xyarray *)NULL);
    }
    ASCII=0;
    if(fread(&numfields,sizeof(numfields),1,fid)!=1) {
      fprintf(stderr,"ERROR (readxy) -- bad numfields fread in field file.\n");
      return((xyarray *)NULL);
    }
  }
  xyarray *thepix = mk_xyarray(numfields);
  int tmpchar;
  for(ii=0;ii<numfields;ii++) {
    if(ASCII) {
      tmpchar=fgetc(fid);
      fprintf(stderr,"got char %c\n",(char)tmpchar);
      while(tmpchar==COMMENT_CHAR) {
	fprintf(stderr,"found comment in field %d\n",ii);
	fscanf(fid,"%*s\n");
	tmpchar=fgetc(fid);
	fprintf(stderr,"got char %c\n",(char)tmpchar);
      }
      ungetc(tmpchar,fid);
      fprintf(stderr,"pushed back char %c\n",(char)tmpchar);
      fscanf(fid,"%lu",&numxy); // CHECK THE RETURN VALUE MORON!
    }
    else
      fread(&numxy,sizeof(numxy),1,fid); // CHECK THE RETURN VALUE MORON!
    thepix->array[ii] = mk_xy(numxy);
    if(xya_ref(thepix,ii)==NULL) {
      fprintf(stderr,"ERROR (readxy) - out of memory at field %lu\n",ii);
      free_xyarray(thepix);
      return (xyarray *)NULL;
    }
    if(ASCII) {
      for(jj=0;jj<numxy;jj++)
	fscanf(fid,",%lf,%lf",(thepix->array[ii]->farr)+2*jj,
  	       (thepix->array[ii]->farr)+2*jj+1   );
      fscanf(fid,"\n");
    }
    else
      fread(thepix->array[ii]->farr,sizeof(double),DIM_XY*numxy,fid);

    if(ParityFlip) {
      double swaptmp;
      for(jj=0;jj<numxy;jj++) {
	swaptmp=*((thepix->array[ii]->farr)+2*jj+1);
	*((thepix->array[ii]->farr)+2*jj+1)=	
	  *((thepix->array[ii]->farr)+2*jj);
	*((thepix->array[ii]->farr)+2*jj)=swaptmp;
      }
    }
  }
  return thepix;
}


kdtree *read_starkd(FILE *treefid, double *ramin, double *ramax, 
		    double *decmin, double *decmax)
{
  kdtree *starkd = fread_kdtree(treefid);
  fread(ramin,sizeof(double),1,treefid);
  fread(ramax,sizeof(double),1,treefid);
  fread(decmin,sizeof(double),1,treefid);
  fread(decmax,sizeof(double),1,treefid);
  return starkd;
}

kdtree *read_codekd(FILE *treefid,double *index_scale)
{
  kdtree *codekd = fread_kdtree(treefid);
  fread(index_scale,sizeof(double),1,treefid);
  return codekd;
}

void write_starkd(FILE *treefid, kdtree *starkd,
		  double ramin, double ramax, double decmin, double decmax)
{
  fwrite_kdtree(starkd,treefid);
  fwrite(&ramin,sizeof(double),1,treefid);
  fwrite(&ramax,sizeof(double),1,treefid);
  fwrite(&decmin,sizeof(double),1,treefid);
  fwrite(&decmax,sizeof(double),1,treefid);
  return;
}

void write_codekd(FILE *treefid, kdtree *codekd,double index_scale)
{
  fwrite_kdtree(codekd,treefid);
  fwrite(&index_scale,sizeof(double),1,treefid);
  return;
}

char *mk_filename(const char *basename, const char *extension)
{
  char *fname;
  fname = (char *)malloc(strlen(basename)+strlen(extension)+1);
  sprintf(fname,"%s%s",basename,extension);
  return fname;
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


signed int compare_sidx(const void *x,const void *y)
{
  sidx xval,yval;
  xval = *(sidx *)x;
  yval = *(sidx *)y;
  if(xval>yval) return(1);
  else if(xval<yval) return(-1);
  else return(0);
}
