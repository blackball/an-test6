#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include "tic.h"

time_t starttime, endtime;

double millis_between(struct timeval* tv1, struct timeval* tv2) {
	return
		(tv2->tv_usec - tv1->tv_usec)*1e-3 +
		(tv2->tv_sec  - tv1->tv_sec )*1e3;
}

void tic()
{
	starttime = time(NULL);
}

int get_resource_stats(double* p_usertime, double* p_systime, long* p_maxrss)
{
	struct rusage usage;
	if (getrusage(RUSAGE_SELF, &usage)) {
		fprintf(stderr, "getrusage failed: %s\n", strerror(errno));
		return 1;
	}
	if (p_usertime) {
		*p_usertime = usage.ru_utime.tv_sec + 1e-6 * usage.ru_utime.tv_usec;
	}
	if (p_systime) {
		*p_systime = usage.ru_stime.tv_sec + 1e-6 * usage.ru_stime.tv_usec;
	}
	if (p_maxrss) {
		*p_maxrss = usage.ru_maxrss;
	}
	return 0;
}


void toc() 
{
	double utime, stime;
	long rss;
	int dtime;
	endtime = time(NULL);
	dtime = (int)(endtime - starttime);
	if (!get_resource_stats(&utime, &stime, &rss)) {
		fprintf(stderr, "Finished: used %g s user, %g s system (%g s total), %i s wall time, max rss %li\n",
			utime, stime, utime + stime, dtime, rss);
	}
}
