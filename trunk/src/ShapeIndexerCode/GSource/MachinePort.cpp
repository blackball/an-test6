/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <memory.h>
#include <MachinePort.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// Maximum size of any individual data type
#define MAX_DATA_SIZE   64

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// Forward reference to function for block endian swapping
inline void doSwapEndian(void *data, long numBytes);

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * getEndian: Returns the endian type of the running CPU.
 */
ENDIAN_TYPE getEndian()
{
  static ENDIAN_TYPE endian = UNKNOWN_ENDIAN;

  // If the endian is unknown, determine the endian, otherwise
  // just return the currently known endian.
  if (endian == UNKNOWN_ENDIAN)
  {
    BYTE data[2] = {0, 1};
    short *sdata = (short*)data;

    if (*sdata & 0x01)
    {
      endian = B_ENDIAN;
    } else
    {
      endian = L_ENDIAN;
    }
  }

  return endian;
}

/* -------------------------------------------------- */

/* 
 * doSwapEndian: Swaps the order of the byes in 'data'. 'numBytes'
 *               refers to the number of bytes in the data object.
 */
void doSwapEndian(void *data, long numBytes)
{
  BYTE temp[MAX_DATA_SIZE];
  BYTE *tdata = (BYTE*)data;
  int  i;

  for (i = 0; i < numBytes; i++)
  {
    temp[i] = tdata[numBytes-i-1];
  }

  memcpy(data, temp, sizeof(BYTE)*numBytes);
}

/* -------------------------------------------------- */

/*
 * swapEndian: swaps the endian of a char data type.
 * Assumes: 'data' is a valid char pointer.
 */
void swapEndian(char *data)
{
  // no work required
  return;
}

/* -------------------------------------------------- */

/*
 * swapEndian: swaps the endian of an unsigned char data type.
 * Assumes: 'data' is a valid unsigned char pointer.
 */
void swapEndian(unsigned char *data)
{
  // no work required
  return;
}

/* -------------------------------------------------- */

/*
 * swapEndian: swaps the endian of a short data type.
 * Assumes: 'data' is a valid short pointer.
 */
void swapEndian(short *data)
{
  doSwapEndian((void*)data, sizeof(short));
}

/* -------------------------------------------------- */

/*
 * swapEndian: swaps the endian of an unsigned short data type.
 * Assumes: 'data' is a valid unsigned short pointer.
 */
void swapEndian(unsigned short *data)
{
  doSwapEndian((void*)data, sizeof(unsigned short));
}

/* -------------------------------------------------- */

/*
 * swapEndian: swaps the endian of an int data type.
 * Assumes: 'data' is a valid int pointer.
 */
void swapEndian(int *data)
{
  doSwapEndian((void*)data, sizeof(int));
}

/* -------------------------------------------------- */

/*
 * swapEndian: swaps the endian of an unsigned int data type.
 * Assumes: 'data' is a valid unsigned int pointer.
 */
void swapEndian(unsigned int *data)
{
  doSwapEndian((void*)data, sizeof(unsigned int));
}

/* -------------------------------------------------- */

/*
 * swapEndian: swaps the endian of a long data type.
 * Assumes: 'data' is a valid long pointer.
 */
void swapEndian(long *data)
{
  doSwapEndian((void*)data, sizeof(long));
}

/* -------------------------------------------------- */

/*
 * swapEndian: swaps the endian of an unsigned long data type.
 * Assumes: 'data' is a valid unsigned long pointer.
 */
void swapEndian(unsigned long *data)
{
  doSwapEndian((void*)data, sizeof(unsigned long));
}

/* -------------------------------------------------- */

/*
 * swapEndian: swaps the endian of a float data type.
 * Assumes: 'data' is a valid float pointer.
 */
void swapEndian(float *data)
{
  doSwapEndian((void*)data, sizeof(float));
}

/* -------------------------------------------------- */

/*
 * swapEndian: swaps the endian of a double data type.
 * Assumes: 'data' is a valid double pointer.
 */
void swapEndian(double *data)
{
  doSwapEndian((void*)data, sizeof(double));
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// Single bit-high-values
BYTE _bgMasks[8] = { 1,     2,   4,   8,  16,  32,  64, 128};
// Single bit-low-value
BYTE _bsMasks[8] = { 254, 253, 251, 247, 239, 223, 191, 127};

/* -------------------------------------------------- */

/*
 * getBit: Returns 0/1 depending on whether bit 'bit' in byte 'b' 
 *         is 0/1.
 * Assumes: 0 <= 'bit' <= 7, 'b' is a valid unsigned char pointer.
 */
BYTE getBit(unsigned char *b, long bit)
{
  assert(((bit >= 0) && (bit <= 7)));

  return (*b & _bgMasks[bit]) ? 1:0;
}

/* -------------------------------------------------- */

/*
 * getBitUdec: Returns 0/1 depending on whether bit 'bit' in byte 'b' 
 *             is 0/1. The pointer 'b' is decreased if 'bit' == 0, and 'bit'
 *             is decreased to the next appropriate bit position.
 * Assumes: 0 <= '*bit' <= 7, 'b' is a valid unsigned char pointer-pointer. 'bit' is a valid
 *          'long' pointer.
 */
BYTE getBitUdec(unsigned char **b, long *bit)
{
  BYTE tbyte;

  tbyte = getBit(*b, *bit);
  if (*bit == 0)
  {
    *bit = 7;
    (*b)++;
  } else
  {
    (*bit)--;
  }

  return tbyte;
}

/* -------------------------------------------------- */

/*
 * getBitUinc: Returns 0/1 depending on whether bit 'bit' in byte 'b' 
 *             is 0/1. The pointer 'b' is increased if 'bit' == 7, and 'bit'
 *             is increased to the next appropriate bit position.
 * Assumes: 0 <= '*bit' <= 7, 'b' is a valid unsigned char pointer-pointer. 'bit' is a valid
 *          'long' pointer.
 */
BYTE getBitUinc(unsigned char **b, long *bit)
{
  BYTE tbyte;

  tbyte = getBit(*b, *bit);
  if (*bit == 7)
  {
    *bit = 0;
    (*b)++;
  } else
  {
    (*bit)++;
  }

  return tbyte;
}

/* -------------------------------------------------- */

/*
 * setBit: Sets bit 'bit' in unsigned char 'b' according to 'val'.
 *         I.e. the bit is set to 1 if 'val' != 0, and 0 otherwise.
 * Assumes: 0 <= 'bit' <= 7, 'b' is a valid unsigned char pointer-pointer.
 */
void setBit(unsigned char val, unsigned char *b, long bit)
{
  assert(((bit >= 0) && (bit <= 7)));

  *b = (*b & _bsMasks[bit]) | ((val) ? _bgMasks[bit]:0);
}

/* -------------------------------------------------- */

/*
 * setBitUdec: Sets bit 'bit' in unsigned char 'b' according to 'val'.
 *             I.e. the bit is set to 1 if 'val' != 0, and 0 otherwise. 
 *             The pointer 'b' is decreased if 'bit' == 7, and 'bit'
 *             is decreased to the next appropriate bit position.
 * Assumes: 0 <= '*bit' <= 7, 'b' is a valid unsigned char pointer-pointer. 'bit' is a valid
 *          'long' pointer.
 */
void setBitUdec(unsigned char val, unsigned char **b, long *bit)
{
  setBit(val, *b, *bit);
  if (*bit == 0)
  {
    *bit = 7;
    (*b)++;
  } else
  {
    (*bit)--;
  }
}

/* -------------------------------------------------- */

/*
 * setBitUinc: Sets bit 'bit' in unsigned char 'b' according to 'val'.
 *             I.e. the bit is set to 1 if 'val' != 0, and 0 otherwise. 
 *             The pointer 'b' is increased if 'bit' == 7, and 'bit'
 *             is increased to the next appropriate bit position.
 * Assumes: 0 <= '*bit' <= 7, 'b' is a valid unsigned char pointer-pointer. 'bit' is a valid
 *          'long' pointer.
 */
void setBitUinc(unsigned char val, unsigned char **b, long *bit)
{
  setBit(val, *b, *bit);
  if (*bit == 7)
  {
    *bit = 0;
    (*b)++;
  } else
  {
    (*bit)++;
  }
}

/* -------------------------------------------------- */

/*
 * getNibble: Returns the nibble in 'b' (placed in the lower nibble of the
 *            return byte). If 'nibble' is 1 the top nibble is used, otherwise
 *            the bottom nibble is used.
 * Assumes: 0 <= 'nibble' <= 1, 'b' is a valid unsigned char pointer.
 */
BYTE getNibble(unsigned char *b, long nibble)
{
  assert(((nibble >= 0) && (nibble <= 1)));

  return (nibble) ? ((*b & 0xF0) >> 4):(*b & 0x0F);
}

/* -------------------------------------------------- */

/*
 * getNibbleUdec: Returns the nibble in 'b' (placed in the lower nibble of the
 *                return byte). If 'nibble' is 1 the top nibble is used, otherwise
 *                the bottom nibble is used. The pointer 'b' is decreased if 'nibble' == 1, 
 *                and 'nibble' is decreased to the next appropriate nibble position.
 * Assumes: 0 <= '*nibble' <= 1, 'b' is a valid unsigned char pointer. 'nibble' is a valid
 *          'long' pointer.
 */
BYTE getNibbleUdec(unsigned char **b, long *nibble)
{
  BYTE tbyte;

  tbyte = getNibble(*b, *nibble);
  if (*nibble == 0)
  {
    *nibble = 1;
    *b++;
  } else
  {
    (*nibble)--;
  }

  return tbyte;
}

/* -------------------------------------------------- */

/*
 * getNibbleUinc: Returns the nibble in 'b' (placed in the lower nibble of the
 *                return byte). If 'nibble' is 1 the top nibble is used, otherwise
 *                the bottom nibble is used. The pointer 'b' is increased if 'nibble' == 1, 
 *                and 'nibble' is increased to the next appropriate nibble position.
 * Assumes: 0 <= '*nibble' <= 1, 'b' is a valid unsigned char pointer. 'nibble' is a valid
 *          'long' pointer.
 */
BYTE getNibbleUinc(unsigned char **b, long *nibble)
{
  BYTE tbyte;

  tbyte = getNibble(*b, *nibble);
  if (*nibble == 1)
  {
    *nibble = 0;
    *b++;
  } else
  {
    (*nibble)++;
  }

  return tbyte;
}

/* -------------------------------------------------- */

/*
 * setNibble: Sets the nibble in 'b' (according to the lower nibble of the
 *            parameter 'val'). If 'nibble' is 1 the top nibble is used, otherwise
 *            the bottom nibble is used.
 * Assumes: 0 <= 'nibble' <= 1, 'b' is a valid unsigned char pointer.
 */
void setNibble(unsigned char val, unsigned char *b, long nibble)
{
  assert(((nibble >= 0) && (nibble <= 1)));

  *b = (nibble) ? ((*b & 0x0F) | (val << 4)):((*b & 0xF0) | (val & 0x0F));
}

/* -------------------------------------------------- */

/*
 * setNibbleUdec: Sets the nibble in 'b' (according to the lower nibble of the
 *                parameter 'val'). If 'nibble' is 1 the top nibble is used, otherwise
 *                the bottom nibble is used. The pointer 'b' is decreased if 'nibble' == 1, 
 *                and 'nibble' is decreased to the next appropriate nibble position.
 * Assumes: 0 <= 'nibble' <= 1, 'b' is a valid unsigned char pointer. 'nibble' is a valid long
 *          pointer.
 */
void setNibbleUdec(unsigned char val, unsigned char **b, long *nibble)
{
  setNibble(val, *b, *nibble);
  if (*nibble == 0)
  {
    *nibble = 1;
    (*b)++;
  } else
  {
    (*nibble)--;
  }
}

/* -------------------------------------------------- */

/*
 * setNibbleUinc: Sets the nibble in 'b' (according to the lower nibble of the
 *                parameter 'val'). If 'nibble' is 1 the top nibble is used, otherwise
 *                the bottom nibble is used. The pointer 'b' is increased if 'nibble' == 1, 
 *                and 'nibble' is increased to the next appropriate nibble position.
 * Assumes: 0 <= 'nibble' <= 1, 'b' is a valid unsigned char pointer. 'nibble' is a valid long
 *          pointer.
 */
void setNibbleUinc(unsigned char val, unsigned char **b, long *nibble)
{
  setNibble(val, *b, *nibble);
  if (*nibble == 1)
  {
    *nibble = 0;
    (*b)++;
  } else
  {
    (*nibble)++;
  }
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
