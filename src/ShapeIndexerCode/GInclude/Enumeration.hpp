#ifndef __ENUMERATION_HPP__
#define __ENUMERATION_HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * Enumeration: Provides an abstract class that can be used to 
 *              iterate over the elements held in a data structure.
 */
class Enumeration
{
public:
  // Restart the eumeration.
  virtual void   ReInitEnum()   = 0;
  // Return the next data item.
  virtual void*  GetEnumNext()  = 0;
  // Indicates if the enumeration is still valid.
  virtual BOOL32 GetEnumOK()    = 0;
  // Returns the number of element remaining in tehe enumeration.
  virtual DWORD  GetEnumCount() = 0;
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
