#include "ngcic-accurate.h"

static ngcic_accurate ngcic_acc[] = {
  #include "ngcic-accurate-entries.c"
};

int ngcic_accurate_get_radec(bool is_ngc, int id, float* ra, float* dec) {
  int i, N;
  N = sizeof(ngcic_acc) / sizeof(ngcic_accurate);
  for (i=0; i<N; i++) {
    if ((ngcic_acc[i].is_ngc != is_ngc) ||
	(ngcic_acc[i].id != id))
      continue;
    *ra = ngcic_acc[i].ra;
    *dec = ngcic_acc[i].dec;
    return 0;
  }
  return -1;
}
