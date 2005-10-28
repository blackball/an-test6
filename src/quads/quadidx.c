#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "habf:"
const char HelpString[] =
    "quadidx -f fname [-a|-b]\n"
    "  -a sets ASCII output, -b (default) sets BINARY output\n";


extern char *optarg;
extern int optind, opterr, optopt;

qidx deduplicate_quads(FILE *quadfid, FILE *codefid,
                       FILE *newquadfid, FILE *newcodefid, 
                       qidx numQuads);
sidx build_inverted_index(FILE *idxfid, sidx numStars);
char insertquad(ivec_array *qlist, qidx ii,
                sidx iA, sidx iB, sidx iC, sidx iD);
void getquadids(FILE *quadfid, FILE *codefid,
                qidx ii, sidx *iA, sidx *iB, sidx *iC, sidx *iD);
char newquad(ivec_array *qlist, sidx iA, sidx iB, sidx iC, sidx iD);

char *idxfname = NULL;
char *quadfname = NULL;
char *codefname = NULL;
char *newquadfname = NULL;
char *newcodefname = NULL;
char qASCII, qA2, ASCII = 0;
char buff[100], maxstarWidth, codeWidth;
off_t posmarker, cposmarker;

ivec_array *qlist = NULL;


int main(int argc, char *argv[])
{
	int argidx, argchar; //  opterr = 0;
	sidx numstars, ns2;
	qidx numquads, nq2;
	dimension DimQuads, DimCodes;
	double index_scale, is2;
	FILE *newquadfid = NULL, *newcodefid = NULL;
	FILE *idxfid = NULL, *quadfid = NULL, *codefid = NULL;

	if (argc <= 2) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
		fprintf(stderr, "argc=%d\n", argc);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'a':
			ASCII = 1;
			break;
		case 'b':
			ASCII = 0;
			break;
		case 'f':
			idxfname = mk_qidxfn(optarg);
			quadfname = mk_quad0fn(optarg);
			codefname = mk_code0fn(optarg);
			newquadfname = mk_quadfn(optarg);
			newcodefname = mk_codefn(optarg);
			break;
		case '?':
			fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		case 'h':
			fprintf(stderr, HelpString);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}

	if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	fprintf(stderr, "quadidx: deduplicating and indexing quads in %s...\n",
	        quadfname);

	fopenin(quadfname, quadfid);
	free_fn(quadfname);
	qASCII = read_quad_header(quadfid, &numquads, &numstars, &DimQuads, &index_scale);
	if (qASCII == READ_FAIL)
		fprintf(stderr, "ERROR (quadidx) -- read_quad_header failed\n");
	fopenin(codefname, codefid);
	free_fn(codefname);
	qA2 = read_code_header(codefid, &nq2, &ns2, &DimCodes, &is2);
	if (qA2 == READ_FAIL)
		fprintf(stderr, "ERROR (quadidx) -- read_code_header failed\n");
	if ((qA2 != qASCII) || (nq2 != numquads) || (ns2 != numstars) || (is2 != index_scale)) {
		fprintf(stderr, "ERROR (quadidx) -- codefile and quadfile disagree\n");
		return (2);
	}
	posmarker = ftello(quadfid);
	cposmarker = ftello(codefid);
	if (qASCII) {
		sprintf(buff, "%lu", numstars - 1);
		maxstarWidth = strlen(buff);
	}
	if (qA2) {
		sprintf(buff, "%lf", 1.0 / (double)PIl);
		codeWidth = strlen(buff);
	}

	qlist = mk_ivec_array(numstars);
	if(qlist==NULL) {
	  fprintf(stderr,"ERROR (quadidx) -- can't allocate qlist\n");
	  return(-1);
	}

	if (numquads > 1) {
		fopenout(newquadfname, newquadfid);
		fopenout(newcodefname, newcodefid);
		write_quad_header(newquadfid, qASCII, numquads, numstars, DimQuads, index_scale);
		write_code_header(newcodefid, qASCII, numquads, numstars, DimCodes, index_scale);

		nq2 = deduplicate_quads(quadfid, codefid, newquadfid, newcodefid, 
		                        numquads);

		if (qASCII)
			sprintf(buff, "%lu", numquads);
		fix_quad_header(newquadfid, qASCII, nq2, strlen(buff));
		fix_code_header(newcodefid, qASCII, nq2, strlen(buff));
		fclose(newquadfid);
		fclose(newcodefid);
		free_fn(newquadfname);
		free_fn(newcodefname);
	}
	fclose(quadfid);
	fprintf(stderr, "  done (%lu unique codes found).\n", nq2);

	fprintf(stderr, "building inverted index..."); fflush(stderr);
	fopenout(idxfname, idxfid);
   ns2=build_inverted_index(idxfid, numstars);
	fclose(idxfid);
	free_fn(idxfname);
	fprintf(stderr, "  done (%lu unique stars involved in codes).\n", ns2);


	free_ivec_array(qlist);

	//basic_am_malloc_report();
	return (0);
}


qidx deduplicate_quads(FILE *quadfid, FILE *codefid,
                       FILE *newquadfid, FILE *newcodefid, qidx numQuads)
{
	qidx ii, uniqueQuads = 0;
	sidx iA, iB, iC, iD;
	double Cx, Cy, Dx, Dy;

	for (ii = 0;ii < numQuads;ii++) {
	  if (is_power_of_two(ii + 1))
		 fprintf(stderr,"done %lu/%lu quads\r",ii+1,numQuads);
		getquadids(quadfid, codefid, ii, &iA, &iB, &iC, &iD);
		//fprintf(stderr,"checking quad %lu (%lu,%lu,%lu,%lu)\n",
		//	    ii,iA,iB,iC,iD);
		if (insertquad(qlist, uniqueQuads, iA, iB, iC, iD)) {
			uniqueQuads++;
			if (qASCII) {
				fscanfonecode(codefid, &Cx, &Cy, &Dx, &Dy);
				fprintf(newcodefid, "%lf,%lf,%lf,%lf\n", Cx, Cy, Dx, Dy);
				fprintf(newquadfid, "%2$*1$lu,%3$*1$lu,%4$*1$lu,%5$*1$lu\n",
				        maxstarWidth, iA, iB, iC, iD);
			} else {
				readonecode(codefid, &Cx, &Cy, &Dx, &Dy);
				writeonecode(newcodefid, Cx, Cy, Dx, Dy);
				writeonequad(newquadfid, iA, iB, iC, iD);
			}
		}
	}

	return uniqueQuads;
}




sidx build_inverted_index(FILE *idxfid, sidx numStars)
{
  sidx jj,numused=0;
  qidx ii,kk,thisnumq;
  ivec *tmpivec;
  magicval magic = MAGIC_VAL;

	if (ASCII)
		fprintf(idxfid, "NumIndexedStars=%lu\n", numStars);
	else {
		fwrite(&magic, sizeof(magic), 1, idxfid);
		fwrite(&numused, sizeof(numused), 1, idxfid);
	}

	for (jj = 0;jj < numStars;jj++) {
	  if (is_power_of_two(jj + 1))
		 fprintf(stderr,"done %lu/%lu stars\r",jj+1,numStars);
		tmpivec = ivec_array_ref(qlist, jj);
		if (tmpivec != NULL) {
			numused++;
			thisnumq = (qidx)ivec_size(tmpivec);
			if (ASCII)
				fprintf(idxfid, "%lu:%lu", jj, thisnumq);
			else {
				fwrite(&jj, sizeof(jj), 1, idxfid);
				fwrite(&thisnumq, sizeof(thisnumq), 1, idxfid);
			}
			for (ii = 0;ii < thisnumq;ii++) {
				kk = (qidx)ivec_ref(tmpivec, ii);
				if (ASCII)
					fprintf(idxfid, ",%lu", kk);
				else
					fwrite(&kk, sizeof(kk), 1, idxfid);
			}
			if (ASCII)
				fprintf(idxfid, "\n");
		}
	}
	rewind(idxfid);
	if (ASCII) {
		sprintf(buff, "%lu", numStars);
		fprintf(idxfid, "NumIndexedStars=%2$*1$lu\n", strlen(buff), numused);
	} else {
		fwrite(&magic, sizeof(magic), 1, idxfid);
		fwrite(&numused, sizeof(numused), 1, idxfid);
	}


	return(numused);

}





char insertquad(ivec_array *qlist, qidx ii,
                sidx iA, sidx iB, sidx iC, sidx iD)
{
	char new = FALSE;
	ivec *Alist, *Blist, *Clist, *Dlist;
	Alist = ivec_array_ref(qlist, iA);
	Blist = ivec_array_ref(qlist, iB);
	Clist = ivec_array_ref(qlist, iC);
	Dlist = ivec_array_ref(qlist, iD);
	if (Alist == NULL) {
		ivec_array_set_no_copy(qlist, iA, mk_ivec(0));
		Alist = ivec_array_ref(qlist, iA);
		new = TRUE;
		//fprintf(stderr,"  star %lu previously NULL\n",iA);
	}
	if (Blist == NULL) {
		ivec_array_set_no_copy(qlist, iB, mk_ivec(0));
		Blist = ivec_array_ref(qlist, iB);
		new = TRUE;
		//fprintf(stderr,"  star %lu previously NULL\n",iB);
	}
	if (Clist == NULL) {
		ivec_array_set_no_copy(qlist, iC, mk_ivec(0));
		Clist = ivec_array_ref(qlist, iC);
		new = TRUE;
		//fprintf(stderr,"  star %lu previously NULL\n",iC);
	}
	if (Dlist == NULL) {
		ivec_array_set_no_copy(qlist, iD, mk_ivec(0));
		Dlist = ivec_array_ref(qlist, iD);
		new = TRUE;
		//fprintf(stderr,"  star %lu previously NULL\n",iD);
	}

	if (new == FALSE)
		new = newquad(qlist, iA, iB, iC, iD);

	if (new == TRUE) {
		add_to_ivec_unique2(Alist, ii);
		add_to_ivec_unique2(Blist, ii);
		add_to_ivec_unique2(Clist, ii);
		add_to_ivec_unique2(Dlist, ii);
	}

	return (new);
}

char newquad(ivec_array *qlist, sidx iA, sidx iB, sidx iC, sidx iD)
{
	int minlength, thislength, whichmin = 1;
	ivec *walkivec, *thisivec;
	qidx thisquad, ii;

	walkivec = ivec_array_ref(qlist, iA);
	minlength = ivec_size(walkivec);
	thisivec = ivec_array_ref(qlist, iB);
	thislength = ivec_size(thisivec);
	if (thislength < minlength) {
		minlength = thislength;
		walkivec = thisivec;
		whichmin = 2;
	}
	thisivec = ivec_array_ref(qlist, iC);
	thislength = ivec_size(thisivec);
	if (thislength < minlength) {
		minlength = thislength;
		walkivec = thisivec;
		whichmin = 3;
	}
	thisivec = ivec_array_ref(qlist, iD);
	thislength = ivec_size(thisivec);
	if (thislength < minlength) {
		minlength = thislength;
		walkivec = thisivec;
		whichmin = 4;
	}
	if (minlength == 0)
		return FALSE;

	//fprintf(stderr,"  checking if quad (%lu,%lu,%lu,%lu) is new. (walking %d from %d)\n", iA,iB,iC,iD,minlength,whichmin);

	for (ii = 0;ii < minlength;ii++) {
		thisquad = ivec_ref(walkivec, ii);
		if (is_in_ivec(ivec_array_ref(qlist, iA), thisquad) &&
		        is_in_ivec(ivec_array_ref(qlist, iB), thisquad) &&
		        is_in_ivec(ivec_array_ref(qlist, iC), thisquad) &&
		        is_in_ivec(ivec_array_ref(qlist, iD), thisquad))
			return (FALSE);
	}

	return (TRUE);

}


void getquadids(FILE *quadfid, FILE *codefid,
                qidx ii, sidx *iA, sidx *iB, sidx *iC, sidx *iD)
{
	if (qASCII) {
		fseeko(codefid, cposmarker + ii*
		       (DIM_CODES*(codeWidth + 1)*sizeof(char)), SEEK_SET);
		fseeko(quadfid, posmarker + ii*
		       (DIM_QUADS*(maxstarWidth + 1)*sizeof(char)), SEEK_SET);
		fscanfonequad(quadfid, iA, iB, iC, iD);
	} else {
		fseeko(codefid, cposmarker + ii*
		       (DIM_CODES*sizeof(double)), SEEK_SET);
		fseeko(quadfid, posmarker + ii*
		       (DIM_QUADS*sizeof(*iA)), SEEK_SET);
		readonequad(quadfid, iA, iB, iC, iD);
	}
	return ;
}



