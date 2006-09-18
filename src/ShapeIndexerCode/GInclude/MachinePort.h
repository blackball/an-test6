#ifndef __MACHINEPORT_H__
#define __MACHINEPORT_H__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// Value useful for saving in files to later test endian of the file vs. system
#define ENDIAN_TEST_VAL     0xFF00

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// Enumeration of endian types
enum    ENDIAN_TYPE { UNKNOWN_ENDIAN = 0, L_ENDIAN = 1, B_ENDIAN = 2, ENDIAN_END = 0xFFFFFFFF };

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * swapEndian: A series of functions for checking and swapping the bytes of various
 *             standard data types from one endian type to another.
 */

// return the machine's endian
ENDIAN_TYPE getEndian();
// swap a character's endian
void swapEndian(char *data);
 // swap a uchar's endian
void swapEndian(unsigned char *data);
// swap a short's endian
void swapEndian(short *data);
// swap a ushort's endian
void swapEndian(unsigned short *data);
// swap a int's endian
void swapEndian(int *data);
// swap a uint's endian
void swapEndian(unsigned int *data);
// swap a long's endian
void swapEndian(long *data);
// swap a ulong's endian
void swapEndian(unsigned long *data);
// swap a float's endian
void swapEndian(float *data);
// swap a double's endian
void swapEndian(double *data);

/* -------------------------------------------------- */

/*
 * getBit/setBit: A series of functions for getting and setting the bits
 *                in a byte and variously increasing/decreasing pointers and poisition indicators.
 */

// get a partiular bit
BYTE getBit(unsigned char *b, long bit);
// get a bit and decrease the byte pointer and bit position indicator as needed.
BYTE getBitUdec(unsigned char **b, long *bit);
// get a bit and increase the byte pointer and bit position indicator as needed.            
BYTE getBitUinc(unsigned char **b, long *bit);
// set a particular bit
void setBit(BYTE val, unsigned char *b, long bit);
// set a bit and decrease the byte pointer and bit position indicator as needed.
void setBitUdec(BYTE val, unsigned char **b, long *bit);
// set a bit and increase the byte pointer and bit position indicator as needed.
void setBitUinc(BYTE val, unsigned char **b, long *bit);

/* -------------------------------------------------- */

/*
 * getNibble/setNibble: A series of functions for getting and setting the nibbles (4-bits)
 *                      in a byte and variously increasing/decreasing pointers and poisition indicators.
 */

// get a partiular nibble
BYTE getNibble(unsigned char *b, long nibble);
// get a nibble and decrease the byte pointer and nibble position indicator as needed.    
BYTE getNibbleUdec(unsigned char **b, long *nibble);
// get a nibble and increase the byte pointer and nibble position indicator as needed.    
BYTE getNibbleUinc(unsigned char **b, long *nibble);
// set a partiular nibble
void setNibble(BYTE val,  unsigned char *b, long nibble);
// set a nibble and decrease the byte pointer and nibble position indicator as needed.    
void setNibbleUdec(BYTE val, unsigned char **b, long *nibble);
// get a nibble and decrease the byte pointer and nibble position indicator as needed.    
void setNibbleUinc(BYTE val, unsigned char **b, long *nibble);

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
