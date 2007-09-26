#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <regex.h>
#include <cairo.h>

#include "cairoutils.h"
#include "dimage.h"


int is_png(const struct dirent *de){
  regex_t preq;
  regcomp(&preq, "[^ ]*[.](png|PNG)$", REG_EXTENDED);
  return !regexec(&preq, de->d_name, (size_t) 0, NULL, 0);
}

int is_jpeg(const struct dirent *de){
  regex_t preq;
  regcomp(&preq, "[^ ]*[.](jpg|JPG|jpeg|JPEG)$", REG_EXTENDED);
  return !regexec(&preq, de->d_name, (size_t) 0, NULL, 0);
}

int is_image(const struct dirent *de){
  return is_jpeg(de) || is_png(de);
}

int main(void)
{
  struct dirent **namelist;
  int n, N;
  unsigned char *image;
  char fullpath[255];
  int imW, imH;

  N = scandir("test_simplexy_images", &namelist, is_image, alphasort);
  if (N < 0)
    perror("scandir");
  else{
    for(n=0; n<N; n++){

      strcpy(fullpath, "test_simplexy_images/");
      strcat(fullpath, namelist[n]->d_name);

      printf("processing %s ", fullpath);

      if(is_png(namelist[n])){
	printf("as a PNG\n");
	image = cairoutils_read_png(fullpath, &imW, &imH);
      }

      if(is_jpeg(namelist[n])){
	printf("as a JPEG\n");
	image = cairoutils_read_jpeg(fullpath, &imW, &imH);
      }
      //      simplexy(image);
    
      free(namelist[n]);

    }
    free(namelist);
  }

  free(image);
  return 0;
}

