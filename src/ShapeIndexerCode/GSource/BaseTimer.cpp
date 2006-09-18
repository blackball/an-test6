/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <BaseTimer.hpp>
#ifndef WIN32
  #include <sys/time.h>
#endif

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * BaseTimer: Initialize the timer to a base state.
 * Assumes: <none>
 */
BaseTimer::BaseTimer()
{
#ifdef WIN32
  StartTime = 0;
#else
  StartTime.tv_sec  = 0;
  StartTime.tv_usec = 0;
#endif
}

/* -------------------------------------------------- */

/*
 * Start: Start timing.
 * Assumes: <none>
 */
void BaseTimer::Start()
{
#ifdef WIN32
  StartTime = GetTickCount();
#else
  //timezone tz;

  gettimeofday(&StartTime, NULL);
#endif
}

/* -------------------------------------------------- */

/*
 * Elapsed: Returns the elapsed time since the last call in seconds.
 * Assumes: <none>
 */
float BaseTimer::Elapsed()
{
#ifdef WIN32
  return ((float)(GetTickCount() - StartTime)) * 0.001f;
#else
  //timezone tz;
  timeval  tv;
  float temp;

  gettimeofday(&tv, NULL);

	return (float)(tv.tv_sec - StartTime.tv_sec) +
         ((float)(tv.tv_usec - StartTime.tv_usec)) * 0.000001f;
#endif
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */
