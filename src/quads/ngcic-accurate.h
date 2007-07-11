#include "an-bool.h"

struct ngcic_accurate {
  // true: NGC.  false: IC.
  bool is_ngc;
  // NGC/IC number
  int id;
  // RA,Dec in B2000.0 degrees
  float ra;
  float dec;
};
typedef struct ngcic_accurate ngcic_accurate;

int ngcic_accurate_get_radec(bool is_ngc, int id, float* ra, float* dec);
