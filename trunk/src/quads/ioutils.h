#ifndef IOUTILS_H
#define IOUTILS_H

#include <stdio.h>

extern unsigned int ENDIAN_DETECTOR;

int write_u8(FILE* fout, unsigned char val);
int write_u16(FILE* fout, unsigned int val);
int write_u32(FILE* fout, unsigned int val);
int write_uints(FILE* fout, unsigned int* val, int n);
int write_double(FILE* fout, double val);
int write_float(FILE* fout, float val);
int write_double(FILE* fout, double val);
int write_fixed_length_string(FILE* fout, char* s, int length);
int write_string(FILE* fout, char* s);

int write_u32_portable(FILE* fout, unsigned int val);
int write_u32s_portable(FILE* fout, unsigned int* val, int n);

int read_u8(FILE* fin, unsigned char* val);
int read_u16(FILE* fout, unsigned int* val);
int read_u32(FILE* fin, unsigned int* val);
int read_double(FILE* fin, double* val);
int read_fixed_length_string(FILE* fin, char* s, int length);
char* read_string(FILE* fin);
char* read_string_terminated(FILE* fin, char* terminators, int nterminators);

int read_u32_portable(FILE* fin, unsigned int* val);
int read_u32s_portable(FILE* fin, unsigned int* val, int n);

#endif
