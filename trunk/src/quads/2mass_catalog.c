#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "2mass_catalog.h"

twomass_catalog* twomass_catalog_open(char* fn) {
	return NULL;
}

twomass_catalog* twomass_catalog_open_for_writing(char* fn) {
	return NULL;
}

int twomass_catalog_write_headers(twomass_catalog* cat) {
	return -1;
}

int twomass_catalog_fix_headers(twomass_catalog* cat) {
	return -1;
}

int twomass_catalog_read_entries(twomass_catalog* cat, uint offset,
								 uint count, twomass_entry* entries) {
	return -1;
}

twomass_entry* twomass_catalog_read_entry(twomass_catalog* cat) {
	return NULL;
}

int twomass_catalog_count_entries(twomass_catalog* cat) {
	return -1;
}

int twomass_catalog_close(twomass_catalog* cat) {
	return -1;
}

int twomass_catalog_write_entry(twomass_catalog* cat, twomass_entry* entry) {
	return -1;
}
