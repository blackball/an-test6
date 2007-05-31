#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ngc2000.h"

struct ngc_name {
	bool is_ngc;
	int id;
	char* name;
};
typedef struct ngc_name ngc_name;

ngc_name ngc_names[] = {
#include "ngc2000names.c"
};

ngc_entry ngc_entries[] = {
#include "ngc2000entries.c"
};

char* ngc_get_name(ngc_entry* entry) {
	int i;
	for (i=0; i<sizeof(ngc_names)/sizeof(ngc_name); i++) {
		if ((entry->is_ngc == ngc_names[i].is_ngc) &&
			(entry->id == ngc_names[i].id)) {
			return ngc_names[i].name;
		}
	}
	return NULL;
}

/*
bool ngc_parse_line(char* line, ngc_entry* entry) {
	char* cursor;
	int len, i;
	int hra, degdec, mindec;
	float minra;
	char sign;

	cursor = line + 0;
	if (*cursor == ' ')
		entry->is_ngc = TRUE;
	else if (*cursor == 'I')
		entry->is_ngc = FALSE;
	else
		return FALSE;
	cursor = line + 1;
	if (sscanf(cursor, "%d", &entry->id) != 1)
		return FALSE;

	// copy up to three characters that have been left-padded with spaces.
	cursor = line + 6;
	memset(entry->classification, 0, 4);
	len = 3;
	for (i=0; i<3; i++)
		if (cursor[0] == ' ')
			len--;
		else
			break;
	memcpy(entry->classification, cursor, len);

	// RA
	cursor = line + 10;
	if (sscanf(cursor, " %d %f", &hra, &minra) != 2)
		return FALSE;
	entry->ra = (hra + minra/60.0) * 15.0;

	// Dec
	cursor = line + 19;
	if (sscanf(cursor, "%c%d %d", &sign, &degdec, &mindec) != 3)
		return FALSE;
	entry->dec = degdec + (float)mindec / 60.0;
	if (sign == '+') {
	} else if (sign == '-')
		entry->dec *= -1.0;
	else
		return FALSE;

	// Constellation
	cursor = line + 29;
	memcpy(entry->constellation, cursor, 3);
	entry->constellation[3] = '\0';

	return TRUE;
}
*/
