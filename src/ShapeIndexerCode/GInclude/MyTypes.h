#ifndef __MYTYPES_H__
#define __MYTYPES_H__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#ifdef WIN32
  #include <windows.h>
#endif

#include <assert.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#define FULL_GRAFX    1

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// usable ex: for (EVER) { ... }
#define EVER		;;

// basic data types
#ifndef BYTE
  #define BYTE    unsigned char
#endif
#ifndef WORD
  #define WORD    unsigned short
#endif
#ifndef LONG
	#define LONG    long
#endif
#ifndef DWORD
	#define DWORD		unsigned long
#endif

#ifndef MAX_LONG
  #define MAX_LONG ((long)2147483647L)
#endif
#ifndef MIN_LONG
  #define MIN_LONG ((long)-2147483647)
#endif

#ifndef MAX_DWORD
  #define MAX_DWORD  ((DWORD)4294967295)
#endif
#ifndef MIN_DWORD
  #define MIN_DWORD 0
#endif

#ifndef MAX_SHORT
  #define MAX_SHORT   ((SHORT)32767)
#endif
#ifndef MIN_SHORT
  #define MIN_SHORT   ((SHORT)-32768)
#endif 

#ifndef MAX_WORD
  #define MAX_WORD    ((WORD)65535)
#endif
#ifndef MIN_WORD
  #define MIN_WORD    ((WORD)0)
#endif 

#ifndef MAX_CHAR
  #define MAX_CHAR    ((CHAR)127)
#endif
#ifndef MIN_CHAR
  #define MIN_CHAR    ((CHAR)-128)
#endif

#ifndef MAX_BYTE
  #define MAX_BYTE    ((BYTE)255)
#endif
#ifndef MIN_BYTE
  #define MIN_BYTE    ((BYTE)0)
#endif

#ifndef MAX_FLOAT
  #define MAX_FLOAT   ((float)3.4e+38) 
#endif
#ifndef MIN_FLOAT
  #define MIN_FLOAT   ((float)3.4e-38)
#endif

#ifndef MAX_DOUBLE
  #define MAX_DOUBLE  ((double)1.7e+308) 
#endif
#ifndef MIN_DOUBLE
  #define MIN_DOUBLE  ((double)1.7e-308)
#endif

#ifndef MAX_LONG_DOUBLE
  #define MAX_LONG_DOUBLE  ((long double)1.2e+4932) 
#endif
#ifndef MIN_LONG_DOUBLE
  #define MIN_LONG_DOUBLE  ((long double)1.2e-4932)
#endif

// null value
#ifndef NULL
	#define NULL		(0L)
#endif

// max path length
#ifndef MAX_PATH
	#define MAX_PATH		260
#endif

// define directory markers
#ifndef DIR_MARKER1
  #define DIR_MARKER1 '\\'
#endif
#ifndef DIR_MARKER2
  #define DIR_MARKER2 '/'
#endif

// define PI as needed
#ifndef PI
  #define PI       (3.14159265358979323846264338327950288419716939937510)
#endif
// conversion constant to take degrees to rads
#ifndef RAD_CONV
  #define RAD_CONV (0.017453292519943295769236907684886)
#endif
// conversion constant to take rads to degrees
#ifndef DEG_CONV
  #define DEG_CONV (57.295779513082320876798154814105)
#endif
// euler's constant
#ifndef EULER
  #define EULER    (2.71828182845904523536028747135266249775724709369995)
#endif

// file reading flags
#ifdef WIN32
  #define READ_BINARY   (ios::in | ios::binary | ios::nocreate)
  #define WRITE_BINARY  (ios::out| ios::binary)
  #define READ_TEXT     (ios::in | ios::nocreate)
  #define WRITE_TEXT    (ios::out)
#else
  #define READ_BINARY   (ios::in | ios::binary)
  #define WRITE_BINARY  (ios::out| ios::binary)
  #define READ_TEXT     (ios::in)
  #define WRITE_TEXT    (ios::out)
#endif

/* -------------------------------------------------- */

// release objects safely
#ifndef SAFE_RELEASE
  #define SAFE_RELEASE(x) { if (x) x->Release(); x = NULL; }
#endif

#ifndef MIN
  #define MIN(x,y)        (((x) < (y)) ? (x):(y))
#endif
#ifndef MAX
  #define MAX(x,y)        (((x) < (y)) ? (y):(x))
#endif
#ifndef ABS
  #define ABS(x)          (((x) < 0)   ? (-(x)):(x))
#endif

/* -------------------------------------------------- */

// method to use when deleting something -- deallocate yourself, or ignore it
enum DELMETHOD { DM_DEALLOC, DM_IGNORE, DM_ENDHOLDER        = 0xFFFFFFFF };

// force booleans to be 32 bit for speed purposes -- MS compiler defaults to BYTE size
enum BOOL32    { FALSE32,    TRUE32,    BOOL32_ENDHOLDER    = 0xFFFFFFFF };

// direction value
enum DIRECTION { FORWARD,    BACKWARD,  DIRECTION_ENDHOLDER = 0xFFFFFFFF };

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
