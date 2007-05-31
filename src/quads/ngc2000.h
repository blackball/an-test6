#ifndef NGC2000_H
#define NGC2000_H

#include "an-bool.h"

/*
  The NGC2000 catalog can be found at:
    ftp://cdsarc.u-strasbg.fr/cats/VII/118/
*/

struct ngc_entry {
	// true: NGC.  false: IC.
	bool is_ngc;
	// NGC/IC number
	int id;
	char classification[4];
	float ra;
	float dec;
	//char source;
	char constellation[4];
	// bool sizelimit;
	float size;
	// float mag;
	// bool photo_mag;
	// char[51] description;
};
typedef struct ngc_entry ngc_entry;

char* ngc_get_name(ngc_entry* entry);

extern ngc_entry ngc_entries[];

/*
  #define NGC2000_LINE_LENGTH 96
  bool ngc_parse_line(char* line, ngc_entry* entry);
*/

#endif
