#include "rdlist.h"
#include "fitsioutils.h"
#include "qfits.h"

#include "cutest.h"

static char* get_tmpfile(int i) {
    static char fn[256];
    sprintf(fn, "/tmp/test-rdlist-%i", i);
    return strdup(fn);
}

void test_simple(CuTest* ct) {
    rdlist_t *in, *out;
    char* fn = get_tmpfile(1);
    rd_t fld, infld;
    rd_t* infld2;

    out = rdlist_open_for_writing(fn);
    CuAssertPtrNotNull(ct, out);
    CuAssertIntEquals(ct, 0, rdlist_write_primary_header(out));
    CuAssertIntEquals(ct, 0, rdlist_write_header(out));

    int N = 100;
    double ra[N];
    double dec[N];
    int i;
    for (i=0; i<N; i++) {
        ra[i] = i + 1;
        dec[i] = 3 * i;
    }
    fld.N = N;
    fld.ra = ra;
    fld.dec = dec;

    CuAssertIntEquals(ct, 0, rdlist_write_field(out, &fld));
    CuAssertIntEquals(ct, 0, rdlist_fix_header(out));
    CuAssertIntEquals(ct, 0, rdlist_close(out));
    out = NULL;


    in = rdlist_open(fn);
    CuAssertPtrNotNull(ct, in);
    CuAssertIntEquals(ct, 0, strcmp(in->antype, AN_FILETYPE_RDLS));
    CuAssertPtrNotNull(ct, rdlist_read_field(in, &infld));
    CuAssertIntEquals(ct, N, infld.N);
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, fld.ra[i], infld.ra[i]);
        CuAssertIntEquals(ct, fld.dec[i], infld.dec[i]);
    }
    rd_free_data(&infld);

    infld2 = rdlist_read_field(in, NULL);
    CuAssertPtrNotNull(ct, infld2);
    CuAssertIntEquals(ct, N, infld2->N);
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, fld.ra[i], infld2->ra[i]);
        CuAssertIntEquals(ct, fld.dec[i], infld2->dec[i]);
    }
    rd_free_data(infld2);

    CuAssertIntEquals(ct, 0, rdlist_close(in));
    in = NULL;

    free(fn);
}
/*
void tst_read_write(CuTest* ct) {
    rdlist_t *in, *out;
    char* fn = get_tmpfile(0);
    rd_t fld;
    rd_t infld;

    out = rdlist_open_for_writing(fn);
    CuAssertPtrNotNull(ct, out);

    qfits_header* hdr = rdlist_get_primary_header(out);
    CuAssertPtrNotNull(ct, hdr);
    fits_header_add_int(hdr, "KEYA", 42, "Comment");

    CuAssertIntEquals(ct, 0, rdlist_write_primary_header(out));

    rdlist_set_xname(out, "XXX");
    rdlist_set_yname(out, "YYY");
    rdlist_set_ytype(out, TFITS_BIN_TYPE_E);
    rdlist_set_xunits(out, "pix");
    rdlist_set_yunits(out, "piy");

    rdlist_set_include_flux(out, TRUE);

    hdr = rdlist_get_header(out);
    CuAssertPtrNotNull(ct, hdr);
    fits_header_add_int(hdr, "KEYB", 43, "CommentB");

    CuAssertIntEquals(ct, 0, rdlist_write_header(out));

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

    CuAssertIntEquals(ct, 0, rdlist_write_field(out, &fld));
    CuAssertIntEquals(ct, 0, rdlist_fix_header(out));

    rdlist_set_xname(out, "X2");
    rdlist_set_yname(out, "Y2");
    rdlist_set_xunits(out, "ux");
    rdlist_set_yunits(out, "uy");

    rdlist_set_include_flux(out, FALSE);

    rdlist_next_field(out);

    int N2 = 5;
    fld.N = N2;

    CuAssertIntEquals(ct, 0, rdlist_write_header(out));
    CuAssertIntEquals(ct, 0, rdlist_write_field(out, &fld));
    CuAssertIntEquals(ct, 0, rdlist_fix_header(out));

    CuAssertIntEquals(ct, 0, rdlist_close(out));

    out = NULL;


    
    in = rdlist_open(fn);
    CuAssertPtrNotNull(ct, in);

    hdr = rdlist_get_primary_header(in);
    CuAssertPtrNotNull(ct, hdr);
    char* typ = fits_get_dupstring(hdr, "AN_FILE");
    CuAssertPtrNotNull(ct, typ);
    CuAssertIntEquals(ct, 0, strcmp(typ, "RDLS"));
    free(typ);

    CuAssertIntEquals(ct, 42, qfits_header_getint(hdr, "KEYA", -1));

    rdlist_set_xname(in, "XXX");
    rdlist_set_yname(in, "YYY");

    hdr = rdlist_get_header(in);
    CuAssertPtrNotNull(ct, hdr);
    CuAssertIntEquals(ct, 43, qfits_header_getint(hdr, "KEYB", -1));

    CuAssertPtrNotNull(ct, rdlist_read_field(in, &infld));

    CuAssertIntEquals(ct, N, infld.N);
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, fld.x[i], infld.x[i]);
        CuAssertIntEquals(ct, fld.y[i], infld.y[i]);
        CuAssertIntEquals(ct, fld.flux[i], infld.flux[i]);
    }
    CuAssertPtrEquals(ct, NULL, infld.background);

    rd_free_data(&infld);

    rdlist_next_field(in);

    // the columns are named differently...
    CuAssertPtrEquals(ct, NULL, rdlist_read_field(in, &infld));

    rdlist_set_xname(in, "X2");
    rdlist_set_yname(in, "Y2");

    memset(&infld, 0, sizeof(infld));
    CuAssertPtrNotNull(ct, rdlist_read_field(in, &infld));

    CuAssertIntEquals(ct, N2, infld.N);
    for (i=0; i<N2; i++) {
        CuAssertIntEquals(ct, fld.x[i], infld.x[i]);
        CuAssertIntEquals(ct, fld.y[i], infld.y[i]);
    }
    CuAssertPtrEquals(ct, NULL, infld.flux);
    CuAssertPtrEquals(ct, NULL, infld.background);

    rd_free_data(&infld);

    rdlist_next_field(in);
    // no such field...
    CuAssertPtrEquals(ct, NULL, rdlist_read_field(in, &infld));

    CuAssertIntEquals(ct, 0, rdlist_close(in));


    // I love valgrind-clean tests
    free(fn);
}

 */
