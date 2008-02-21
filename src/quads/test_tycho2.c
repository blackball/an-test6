#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cutest.h"
#include "tycho2.h"
#include "an-bool.h"
#include "starutil.h"

static double deg2mas(double x) {
    return 1000.0 * deg2arcsec(x);
}

static void check_line1(CuTest* tc, const tycho2_entry* e) {
    CuAssertIntEquals(tc, 1, e->tyc1);
    CuAssertIntEquals(tc, 8, e->tyc2);
    CuAssertIntEquals(tc, 1, e->tyc3);
    CuAssertIntEquals(tc, 0, e->photo_center);
    CuAssertIntEquals(tc, 0, e->no_motion);

    CuAssertDblEquals(tc, 2.31750494, e->mean_ra,  1e-8);
    CuAssertDblEquals(tc, 2.23184345, e->mean_dec, 1e-8);
    CuAssertDblEquals(tc, -16.3, e->pm_ra  * 1000.0, 1e-1);
    CuAssertDblEquals(tc,  -9.0, e->pm_dec * 1000.0, 1e-1);

    CuAssertDblEquals(tc, 68.0, deg2mas(e->sigma_mean_ra),  1e-1);
    CuAssertDblEquals(tc, 73.0, deg2mas(e->sigma_mean_dec), 1e-1);
    CuAssertDblEquals(tc, 1.7, e->sigma_pm_ra  * 1000.0, 1e-1);
    CuAssertDblEquals(tc, 1.8, e->sigma_pm_dec * 1000.0, 1e-1);

    CuAssertDblEquals(tc, 1958.89, e->epoch_mean_ra,  1e-2);
    CuAssertDblEquals(tc, 1951.94, e->epoch_mean_dec, 1e-2);

    CuAssertIntEquals(tc, 4, e->nobs);

    CuAssertDblEquals(tc, 1.0, e->goodness_mean_ra,  1e-1);
    CuAssertDblEquals(tc, 1.0, e->goodness_mean_dec, 1e-1);
    CuAssertDblEquals(tc, 0.9, e->goodness_pm_ra,  1e-1);
    CuAssertDblEquals(tc, 1.0, e->goodness_pm_dec, 1e-1);

    CuAssertDblEquals(tc, 12.146, e->mag_BT,   1e-3);
    CuAssertDblEquals(tc,  0.158, e->sigma_BT, 1e-3);
    CuAssertDblEquals(tc, 12.146, e->mag_VT,   1e-3);
    CuAssertDblEquals(tc,  0.223, e->sigma_VT, 1e-3);

    CuAssertDblEquals(tc, -1.0, e->prox, 1e-1);
    CuAssertIntEquals(tc, FALSE, e->tycho1_star);
    CuAssertIntEquals(tc, 0, e->hipparcos_id);
    CuAssertIntEquals(tc, 0, strlen(e->hip_ccdm));

    CuAssertDblEquals(tc, 2.31754222, e->ra,  1e-8);
    CuAssertDblEquals(tc, 2.23186444, e->dec, 1e-8);
    CuAssertDblEquals(tc, 1.67, e->epoch_ra - 1990.0, 1e-2);
    CuAssertDblEquals(tc, 1.54, e->epoch_dec- 1990.0, 1e-2);

    CuAssertDblEquals(tc,  88.0, deg2mas(e->sigma_ra),  1e-1);
    CuAssertDblEquals(tc, 100.8, deg2mas(e->sigma_dec), 1e-1);

    CuAssertIntEquals(tc, FALSE, e->double_star);
    CuAssertIntEquals(tc, FALSE, e->photo_center_treatment);

    CuAssertDblEquals(tc, -0.2, e->correlation, 1e-1);
}

void test_read_raw(CuTest* tc) {
    char* line1 = "0001 00008 1| |"
        "  2.31750494|  2.23184345|  -16.3|   -9.0|"
        " 68| 73| 1.7| 1.8|"
        "1958.89|1951.94| 4|"
        "1.0|1.0|0.9|1.0|"
        "12.146|0.158|12.146|0.223|"
        "999| |         |"
        "  2.31754222|  2.23186444|1.67|1.54|"
        " 88.0|100.8|"
        " |-0.2\r\n";
    //char* fn = "/tmp/test-2mass-0";
    tycho2_entry entry;
    memset(&entry, 0, sizeof(tycho2_entry));

    CuAssertIntEquals(tc, 0, tycho2_guess_is_supplement(line1));
    CuAssertIntEquals(tc, 0, tycho2_parse_entry(line1, &entry));

    check_line1(tc, &entry);

}
