#ifndef TIC_H
#define TIC_H

#include <time.h>
#include <sys/time.h>

void tic();
int get_resource_stats(double* p_usertime, double* p_systime, long* p_maxrss);
void toc();

double millis_between(struct timeval* tv1, struct timeval* tv2);

#endif
