#include <stdio.h>

#define LINECHARS 80
#define HDULINES 36

int main(void) {
  int counter,lines,thechar,hdus,endofblock;
  unsigned int naxis2=0;
  unsigned char printline[LINECHARS+1];
  for(hdus=0;hdus<2;hdus++) {
	 endofblock=0;
	 while(!endofblock) {
		for(lines=0;lines<HDULINES;lines++) {
		  printline[LINECHARS]=0;
		  for(counter=0;counter<LINECHARS;counter++) {
			 thechar=getchar();
			 if(thechar==EOF) {
				fprintf(stderr,"EOF encountered in header!\n");
				return(-1);
			 }
			 if(thechar<='~' && thechar>=' ')
				printline[counter]=(unsigned char)thechar;
			 else
				printline[counter]='*';
		  }
		  if(!endofblock) {
			 printf("%s\n",printline);
			 sscanf(printline,"NAXIS2 = %u %*s",&naxis2);
			 if(printline[0]=='E' && printline[1]=='N' && printline[2]=='D') {
				endofblock=1;
				printf("----------------\n");
			 }
		  }
		}
	 }
  }
  counter=0,endofblock=0;
  while((thechar=getchar())!=EOF) {
	 counter++;
	 if(thechar!=0) endofblock=counter;
  }
  printf("[%d nonzero (%d total) bytes follow, representing %u items]\n",endofblock,counter,naxis2);
}
