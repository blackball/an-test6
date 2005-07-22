#include "fileutil.h"

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
  }
  return(ASCII);

}


