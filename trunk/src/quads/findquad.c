#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hf:q:i:"
const char HelpString[]=
"findquad -f fname [-q quad# | -i starID]\n";

extern char *optarg;
extern int optind, opterr, optopt;

char *qidxfname=NULL;
char *quadfname=NULL;
char *codefname=NULL;
sidx thestar;
qidx thequad;
bool starset=FALSE,quadset=FALSE;
char buff[100],maxstarWidth;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  if(argc<=4) {fprintf(stderr,HelpString); return(OPT_ERR);}

  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'f':
	qidxfname = malloc(strlen(optarg)+6);
	quadfname = malloc(strlen(optarg)+6);
	codefname = malloc(strlen(optarg)+6);
	sprintf(qidxfname,"%s.qidx",optarg);
	sprintf(quadfname,"%s.quad",optarg);
	sprintf(codefname,"%s.code",optarg);
	break;
      case 'i':
	thestar = strtoul(optarg,NULL,0);
	starset = TRUE;
	break;
      case 'q':
	thequad = strtoul(optarg,NULL,0);
	quadset = TRUE;
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

  sidx ii,numstars,numstars2;
  qidx numquads,iA,iB,iC,iD,jj;
  dimension DimQuads;
  double index_scale,Cx,Cy,Dx,Dy;
  FILE *qidxfid=NULL,*quadfid=NULL,*codefid=NULL;
  sidx *starlist,*matchstar;
  qidx *starnumq;
  qidx **starquads;
  char qASCII;

  fprintf(stderr,"findquad: looking up quads in %s\n",qidxfname);

  if(starset==FALSE) {
    fprintf(stderr,"  Reading code/quad files...");fflush(stderr);
    fopenin(quadfname,quadfid);
    qASCII = read_quad_header(quadfid, 
			      &numquads, &numstars, &DimQuads, &index_scale);
    if(qASCII==READ_FAIL) return(1);
    fprintf(stderr,"  (%lu quads, %lu total stars, scale=%f)\n",
	    numquads,numstars,index_scale);
    if(quadset==TRUE) {
      if(qASCII){sprintf(buff,"%lu",numstars-1);maxstarWidth=strlen(buff);}
      if(qASCII) {
	fseeko(quadfid,ftello(quadfid)+thequad*
	      (DimQuads*(maxstarWidth+1)*sizeof(char)),SEEK_SET); 
	fscanf(quadfid,"%lu,%lu,%lu,%lu\n",&iA,&iB,&iC,&iD);
      }
      else {
	fseeko(quadfid,ftello(quadfid)+thequad*
	      (DimQuads*sizeof(iA)),SEEK_SET);
	fread(&iA,sizeof(iA),1,quadfid);
	fread(&iB,sizeof(iB),1,quadfid);
	fread(&iC,sizeof(iC),1,quadfid);
	fread(&iD,sizeof(iD),1,quadfid);
      }
      fprintf(stderr,"quad %lu : A=%lu,B=%lu,C=%lu,D=%lu\n",
	      thequad,iA,iB,iC,iD);

      fopenin(codefname,codefid);
      qASCII = read_code_header(codefid, 
				&numquads, &numstars, &DimQuads, &index_scale);
      if(qASCII==READ_FAIL) return(1);
      if(qASCII){sprintf(buff,"%f",1.0/(double)PIl);maxstarWidth=strlen(buff);}
      if(qASCII) {
	fseeko(codefid,ftello(codefid)+thequad*
	      (DIM_CODES*(maxstarWidth+1)*sizeof(char)),SEEK_SET); 
	fscanf(quadfid,"%lf,%lf,%lf,%lf\n",&Cx,&Cy,&Dx,&Dy);
      }
      else {
	fseeko(codefid,ftello(codefid)+thequad*
	      (DIM_CODES*sizeof(Cx)),SEEK_SET);
	fread(&Cx,sizeof(Cx),1,codefid);
	fread(&Cy,sizeof(Cy),1,codefid);
	fread(&Dx,sizeof(Dx),1,codefid);
	fread(&Dy,sizeof(Dy),1,codefid);
      }
      fprintf(stderr,"     code = %lf,%lf,%lf,%lf\n",Cx,Cy,Dx,Dy);

    }
  }
  if(quadset==FALSE) {
    fprintf(stderr,"  Reading quad index...");fflush(stderr);
    fopenin(qidxfname,qidxfid); 
    numstars2=readquadidx(qidxfid,&starlist,&starnumq,&starquads);
    if(numstars2==0) {
     fprintf(stderr,"ERROR (findquad) -- out of memory\n"); return(2);}
    fprintf(stderr,"  (%lu unique stars used in quads)\n",numstars2);
    fclose(qidxfid);
    if(starset==TRUE) {
      matchstar = (sidx *)bsearch(&thestar,starlist,numstars2,
				  sizeof(sidx *),compare_sidx);
      if(matchstar==NULL)
	fprintf(stderr,"no match for star %lu\n",thestar);
      else {
	ii=(sidx)(matchstar-starlist);
	fprintf(stderr,"star %lu appears in %lu quads:\n",
		starlist[ii],starnumq[ii]);
	for(jj=0;jj<starnumq[ii];jj++)
	  fprintf(stderr,"%lu ",*(starquads[ii]+jj));
	fprintf(stderr,"\n");
      }
      
    }

    for(ii=0;ii<numstars2;ii++)
      free(starquads[ii]);
    free(starquads);
    free(starnumq);
    free(starlist);
  }

  fnfree(codefname);
  fnfree(qidxfname);
  fnfree(quadfname);
  //basic_am_malloc_report();
  return(0);
}





