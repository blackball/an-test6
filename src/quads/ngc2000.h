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

	// RA,Dec in B2000.0
	float ra;
	float dec;

	char constellation[4];

	// Maximum dimension in arcmin.
	float size;

	//char source;
	// bool sizelimit;
	// float mag;
	// bool photo_mag;
	// char[51] description;
};
typedef struct ngc_entry ngc_entry;

// find the common of the given ngc_entry, if it has one.
char* ngc_get_name(ngc_entry* entry);

extern ngc_entry ngc_entries[];

// convenience accessors:

int ngc_num_entries();

ngc_entry* ngc_get_entry(int i);

#endif
