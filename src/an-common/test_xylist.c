
#include "xylist.h"
#include "fitsioutils.h"
#include "qfits.h"

#include "cutest.h"

static char* get_tmpfile(int i) {
    static char fn[256];
    sprintf(fn, "/tmp/test-xylist-%i", i);
    return fn;
}

void test_read_write(CuTest* ct) {
    xylist_t *in, *out;
    char* fn = get_tmpfile(0);
    field_t fld;

    out = xylist_open_for_writing(fn);
    CuAssertPtrNotNull(ct, out);

    qfits_header* hdr = xylist_get_primary_header(out);
    CuAssertPtrNotNull(ct, hdr);
    fits_header_add_int(hdr, "KEYA", 42, "Comment");

    CuAssertIntEquals(ct, 0, xylist_write_primary_header(out));

    xylist_set_xname(out, "XXX");
    xylist_set_yname(out, "YYY");
    xylist_set_ytype(out, TFITS_BIN_TYPE_E);
    xylist_set_xunits(out, "pix");
    xylist_set_yunits(out, "piy");

    xylist_set_include_flux(out, TRUE);

    hdr = xylist_get_header(out);
    CuAssertPtrNotNull(ct, hdr);
    fits_header_add_int(hdr, "KEYB", 43, "CommentB");

    CuAssertIntEquals(ct, 0, xylist_write_header(out));

    int N = 10;
    double x[N];
    double y[N];
    double flux[N];
    int i;

    for (i=0; i<N; i++) {
        x[i] = 10 * i;
        y[i] = 5 * i;
        flux[i] = 100 * i;
    }
    fld.N = 10;
    fld.x = x;
    fld.y = y;
    fld.flux = flux;

    CuAssertIntEquals(ct, 0, xylist_write_field(out, &fld));
    CuAssertIntEquals(ct, 0, xylist_fix_header(out));



    xylist_set_xname(out, "XXX");
    xylist_set_yname(out, "YYY");
    xylist_set_ytype(out, TFITS_BIN_TYPE_E);
    xylist_set_xunits(out, "pix");
    xylist_set_yunits(out, "piy");

    xylist_set_include_flux(out, TRUE);

    //xylist_next_field(out);


    CuAssertIntEquals(ct, 0, xylist_close(out));



}

