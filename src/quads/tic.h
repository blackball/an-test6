
#ifndef _TIC_H
#define _TIC_H

void tic();
int get_resource_stats(double* p_usertime, double* p_systime, long* p_maxrss);
void toc();

#endif
