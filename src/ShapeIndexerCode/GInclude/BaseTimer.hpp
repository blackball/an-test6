#ifndef __BASETIMER_HPP__
#define __BASETIMER_HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>

#ifdef WIN32
#else
  #include <sys/time.h>
#endif

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * BaseTimer: A class for timing. 
 */
class BaseTimer
{
protected:
#ifdef WIN32
  DWORD StartTime;
#else
  timeval StartTime;
#endif

public:
  BaseTimer();

  // start timing
  void  Start();
  // return the elapsed time from the last time Start() was called (in seconds)
  float Elapsed();
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif
