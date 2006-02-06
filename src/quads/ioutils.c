#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>
#include <stdlib.h>

#include "ioutils.h"


unsigned int ENDIAN_DETECTOR = 0x01020304;

int read_u8(FILE* fin, unsigned char* val) {
    if (fread(val, 1, 1, fin) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't read u8: %s\n", strerror(errno));
		return 1;
    }
}

int read_u32(FILE* fin, unsigned int* val) {
    uint32_t u;
    if (fread(&u, 4, 1, fin) == 1) {
		*val = ntohl(u);
		return 0;
    } else {
		fprintf(stderr, "Couldn't read u32: %s\n", strerror(errno));
		return 1;
    }
}

int read_double(FILE* fin, double* val) {
    if (fread(val, sizeof(double), 1, fin) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't read double: %s\n", strerror(errno));
		return 1;
    }
}

int read_u32_native(FILE* fin, unsigned int* val) {
    uint32_t u;
    if (fread(&u, 4, 1, fin) == 1) {
		*val = (unsigned int)u;
		return 0;
    } else {
		fprintf(stderr, "Couldn't read u32 native: %s\n", strerror(errno));
		return 1;
    }
}

int read_u32s(FILE* fin, unsigned int* val, int n) {
    int i;
    uint32_t* u = (uint32_t*)malloc(sizeof(uint32_t) * n);
    if (!u) {
		fprintf(stderr, "Couldn't real uint32s: couldn't allocate temp array.\n");
		return 1;
    }
    if (fread(u, sizeof(uint32_t), n, fin) == n) {
		for (i=0; i<n; i++) {
			val[i] = ntohl(u[i]);
		}
		free(u);
		return 0;
    } else {
		fprintf(stderr, "Couldn't read u32: %s\n", strerror(errno));
		free(u);
		return 1;
    }
}

int read_fixed_length_string(FILE* fin, char* s, int length) {
	if (fread(s, 1, length, fin) != length) {
		fprintf(stderr, "Couldn't read fixed-length string: %s\n", strerror(errno));
		return 1;
	}
	return 0;
}

int write_fixed_length_string(FILE* fout, char* s, int length) {
	char* str;
	int res;
	str = (char*)malloc(length);
	if (!str) {
		fprintf(stderr, "Couldn't allocate a temp buffer of size %i.\n", length);
		return 1;
	}
	memset(str, 0, length);
	sprintf(str, "%.*s", length, s);
	res = fwrite(str, 1, length, fout);
	free(str);
	if (res != length) {
		fprintf(stderr, "Couldn't write fixed-length string: %s\n", strerror(errno));
		return 1;
	}
	return 0;
}

int write_double(FILE* fout, double val) {
    if (fwrite(&val, sizeof(double), 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write double: %s\n", strerror(errno));
		return 1;
    }
}

int write_float(FILE* fout, float val) {
    if (fwrite(&val, sizeof(float), 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write float: %s\n", strerror(errno));
		return 1;
    }
}

int write_u8(FILE* fout, unsigned char val) {
    if (fwrite(&val, 1, 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u8: %s\n", strerror(errno));
		return 1;
    }
}

int write_u32(FILE* fout, unsigned int val) {
    uint32_t v = htonl((uint32_t)val);
    if (fwrite(&v, 4, 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u32: %s\n", strerror(errno));
		return 1;
    }
}

int write_u32s(FILE* fout, unsigned int* val, int n) {
    int i;
    uint32_t* v = (uint32_t*)malloc(sizeof(uint32_t) * n);
    if (!v) {
		fprintf(stderr, "Couldn't write u32s: couldn't allocate temp array.\n");
		return 1;
    }
    for (i=0; i<n; i++) {
		v[i] = htonl((uint32_t)val[i]);
    }
    if (fwrite(v, sizeof(uint32_t), n, fout) == n) {
		free(v);
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u32s: %s\n", strerror(errno));
		free(v);
		return 1;
    }
}

int write_u32_native(FILE* fout, unsigned int val) {
    uint32_t v = (uint32_t)val;
    if (fwrite(&v, 4, 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u32: %s\n", strerror(errno));
		return 1;
    }
}

int write_uints_native(FILE* fout, unsigned int* val, int n) {
    if (fwrite(val, sizeof(unsigned int), n, fout) == n) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write uints: %s\n", strerror(errno));
		return 1;
    }
}

