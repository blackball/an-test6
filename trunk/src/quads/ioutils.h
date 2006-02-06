#ifndef IOUTILS_H_
#define IOUTILS_H_

#include <stdio.h>

extern unsigned int ENDIAN_DETECTOR;

int write_u8(FILE* fout, unsigned char val);
int write_u32(FILE* fout, unsigned int val);
int write_u32s(FILE* fout, unsigned int* val, int n);
int write_u32_native(FILE* fout, unsigned int val);
int write_uints_native(FILE* fout, unsigned int* val, int n);
int write_double(FILE* fout, double val);
int write_float(FILE* fout, float val);
int write_double(FILE* fout, double val);
int write_fixed_length_string(FILE* fout, char* s, int length);

int read_u8(FILE* fin, unsigned char* val);
int read_u32(FILE* fin, unsigned int* val);
int read_u32_native(FILE* fin, unsigned int* val);
int read_u32s(FILE* fin, unsigned int* val, int n);
int read_double(FILE* fin, double* val);
int read_fixed_length_string(FILE* fin, char* s, int length);

#endif
