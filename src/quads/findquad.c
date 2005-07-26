#include <stdio.h>
#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hf:q:i:"
extern char *optarg;
extern int optind, opterr, optopt;

char *qidxfname=NULL;
char *quadfname=NULL;
sidx thestar;
qidx thequad;
bool starset=FALSE,quadset=FALSE;
char buff[100],maxstarWidth;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'f':
	qidxfname = malloc(strlen(optarg)+6);
	quadfname = malloc(strlen(optarg)+6);
	sprintf(qidxfname,"%s.qidx",optarg);
	sprintf(quadfname,"%s.quad",optarg);
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
	fprintf(stderr, 
	"findquad [-f fname] [-q quad# | -i starID]\n");
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  for (argidx = optind; argidx < argc; argidx++)
    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);


  sidx ii,numstars,numstars2;
  qidx numquads,iA,iB,iC,iD,jj;
  dimension DimQuads;
  double index_scale;
  FILE *qidxfid=NULL,*quadfid=NULL;
  sidx *starlist,*matchstar;
  qidx *starnumq;
  qidx **starquads;
  char qASCII;

  fprintf(stderr,"findquad: looking up quads in %s\n",qidxfname);

  if(starset==FALSE) {
    fprintf(stderr,"  Reading quad file...");fflush(stderr);
    fopenin(quadfname,quadfid); fnfree(quadfname);
    qASCII = read_quad_header(quadfid, 
			      &numquads, &numstars, &DimQuads, &index_scale);
    if(qASCII==READ_FAIL) return(1);
    fprintf(stderr,"  (%lu quads, %lu total stars, scale=%f)\n",
	    numquads,numstars,index_scale);
    if(quadset==TRUE) {
      if(qASCII){sprintf(buff,"%lu",numstars-1);maxstarWidth=strlen(buff);}
      if(qASCII) {
	fseek(quadfid,ftell(quadfid)+thequad*
	      (DimQuads*(maxstarWidth+1)*sizeof(char)),SEEK_SET); 
	fscanf(quadfid,"%lu,%lu,%lu,%lu\n",&iA,&iB,&iC,&iD);
      }
      else {
	fseek(quadfid,ftell(quadfid)+thequad*
	      (DimQuads*sizeof(iA)),SEEK_SET);
	fread(&iA,sizeof(iA),1,quadfid);
	fread(&iB,sizeof(iB),1,quadfid);
	fread(&iC,sizeof(iC),1,quadfid);
	fread(&iD,sizeof(iD),1,quadfid);
      }
      fprintf(stderr,"quad %lu : A=%lu,B=%lu,C=%lu,D=%lu\n",
	      thequad,iA,iB,iC,iD);
    }
  }
  if(quadset==FALSE) {
    fprintf(stderr,"  Reading quad index...");fflush(stderr);
    fopenin(qidxfname,qidxfid); fnfree(qidxfname);
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


  //basic_am_malloc_report();
  return(0);
}





