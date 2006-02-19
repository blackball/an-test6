#include "lsfile.h"
#include "ioutils.h"

char* get_next_line(FILE* fid) {
	char* line = read_string_terminated(fid, "\n", 1);
	if (!line) return line;
	// ignore comments...
	if (line[0] == '#') {
		free(line);
		return get_next_line(fid);
	}
	return line;
}

void free_all(blocklist* list) {
	int i,N;
	N = blocklist_count(list);
	for (i=0; i<N; i++) {
		blocklist* plist = (blocklist*)blocklist_pointer_access(list, i);
		if (plist) {
			blocklist_free(plist);
		}
	}
	blocklist_free(list);
}

void ls_file_free(blocklist* list) {
	free_all(list);
}

int read_ls_file_header(FILE* fid) {
	char* line;
	int numfields;

	line = get_next_line(fid);
	if (!line) return -1;

	// first line: numfields
	if (sscanf(line, "NumFields=%i\n", &numfields) != 1) {
		fprintf(stderr, "parse error: numfields\n");
		free(line);
		return -1;
	}
	free(line);
	return numfields;
}

blocklist* read_ls_file_field(FILE* fid, int dimension) {
	blocklist* pointlist;
	int offset;
	int inc;
	char* line;
	int npoints;
	int i;

	// first element: number of entries
	line = get_next_line(fid);
	if (!line) {
		fprintf(stderr, "premature end of file.\n");
		return NULL;
	}
	if (sscanf(line, "%i%n", &npoints, &offset) < 1) {
		fprintf(stderr, "parse error: npoints\n");
		free(line);
		return NULL;
	}

	pointlist = blocklist_double_new(32);

	for (i=0; i<(npoints * dimension); i++) {
		double val;
		if (sscanf(line+offset, ",%lf%n", &val, &inc) < 1) {
			fprintf(stderr, "parse error: point %i\n", i);
			blocklist_free(pointlist);
			free(line);
			return NULL;
		}
		blocklist_double_append(pointlist, val);
		offset += inc;
	}
	free(line);
	return pointlist;
}

blocklist* read_ls_file(FILE* fid, int dimension) {
	int j;
	int numfields;
	blocklist* list;

	numfields = read_ls_file_header(fid);
	if (numfields == -1)
		return NULL;

	list = blocklist_pointer_new(256);

	for (j=0; j<numfields; j++) {
		blocklist* pointlist;

		pointlist = read_ls_file_field(fid, dimension);

		if (!pointlist) {
			free_all(list);
			return NULL;
		}
		blocklist_pointer_append(list, pointlist);

	}
	return list;
}

