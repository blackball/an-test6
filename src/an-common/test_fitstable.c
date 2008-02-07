#include <stddef.h>

#include "fitstable.h"
#include "fitsioutils.h"

#include "cutest.h"

char* get_tmpfile(int i) {
    static char fn[256];
    sprintf(fn, "/tmp/test-fitstable-%i", i);
    return fn;
}

void test_one_column_write_read(CuTest* ct) {
    fitstable_t* tab, *outtab;
    int i;
    int N = 6;
    double outdata[6];
    double* indata;
    char* fn;
    tfits_type dubl;

    fn = get_tmpfile(0);
    outtab = fitstable_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);

    dubl = fitscolumn_double_type();
    
    fitstable_add_write_column(outtab, dubl, "X", "foounits");
    CuAssertIntEquals(ct, fitstable_ncols(outtab), 1);

    CuAssertIntEquals(ct, fitstable_write_primary_header(outtab), 0);
    CuAssertIntEquals(ct, fitstable_write_header(outtab), 0);

    for (i=0; i<N; i++) {
        outdata[i] = i*i;
    }

    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, fitstable_write_row(outtab, outdata+i), 0);
    }
    CuAssertIntEquals(ct, fitstable_fix_header(outtab), 0);
    CuAssertIntEquals(ct, fitstable_close(outtab), 0);

    // writing shouldn't affect the data values
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, outdata[i], i*i);
    }

    tab = fitstable_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, N, fitstable_nrows(tab));

    indata = fitstable_read_column(tab, "X", dubl);
    CuAssertPtrNotNull(ct, indata);
    CuAssertIntEquals(ct, 0, memcmp(outdata, indata, sizeof(outdata)));
    CuAssertIntEquals(ct, 0, fitstable_close(tab));

}

void test_headers(CuTest* ct) {
    fitstable_t* tab, *outtab;
    char* fn;
    qfits_header* hdr;

    fn = get_tmpfile(1);
    outtab = fitstable_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);

    hdr = fitstable_get_primary_header(outtab);
    CuAssertPtrNotNull(ct, hdr);
    fits_header_add_int(hdr, "TSTHDR", 42, "Test Comment");

    // [add columns...]

    hdr = fitstable_get_header(outtab);
    CuAssertPtrNotNull(ct, hdr);
    fits_header_add_int(hdr, "TSTHDR2", 99, "Test 2 Comment");

    CuAssertIntEquals(ct, 0, fitstable_write_primary_header(outtab));
    CuAssertIntEquals(ct, 0, fitstable_write_header(outtab));
    
    CuAssertIntEquals(ct, 0, fitstable_ncols(outtab));

    CuAssertIntEquals(ct, 0, fitstable_close(outtab));

    tab = fitstable_open(fn);
    CuAssertPtrNotNull(ct, tab);

    hdr = fitstable_get_primary_header(tab);
    CuAssertPtrNotNull(ct, hdr);
    CuAssertIntEquals(ct, 42, qfits_header_getint(hdr, "TSTHDR", -1));

    hdr = fitstable_get_header(tab);
    CuAssertPtrNotNull(ct, hdr);
    CuAssertIntEquals(ct, 99, qfits_header_getint(hdr, "TSTHDR2", -1));

    CuAssertIntEquals(ct, fitstable_close(tab), 0);
}

void test_multi_headers(CuTest* ct) {
    fitstable_t* tab, *outtab;
    char* fn;
    qfits_header* hdr;

    fn = get_tmpfile(2);
    outtab = fitstable_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);

    hdr = fitstable_get_primary_header(outtab);
    CuAssertPtrNotNull(ct, hdr);
    fits_header_add_int(hdr, "TSTHDR", 42, "Test Comment");

    CuAssertIntEquals(ct, 0, fitstable_write_primary_header(outtab));

    // [add columns...]

    hdr = fitstable_get_header(outtab);
    CuAssertPtrNotNull(ct, hdr);
    fits_header_add_int(hdr, "TSTHDR2", 99, "Test 2 Comment");
    CuAssertIntEquals(ct, 0, fitstable_write_header(outtab));
    CuAssertIntEquals(ct, 0, fitstable_ncols(outtab));

    fitstable_next_extension(outtab);
    // [add columns...]
    hdr = fitstable_get_header(outtab);
    CuAssertPtrNotNull(ct, hdr);
    fits_header_add_int(hdr, "TSTHDR3", 101, "Test 3 Comment");
    CuAssertIntEquals(ct, 0, fitstable_write_header(outtab));
    CuAssertIntEquals(ct, 0, fitstable_ncols(outtab));

    fitstable_next_extension(outtab);
    // [add columns...]
    hdr = fitstable_get_header(outtab);
    CuAssertPtrNotNull(ct, hdr);
    fits_header_add_int(hdr, "TSTHDR4", 104, "Test 4 Comment");
    CuAssertIntEquals(ct, 0, fitstable_write_header(outtab));
    CuAssertIntEquals(ct, 0, fitstable_ncols(outtab));

    CuAssertIntEquals(ct, 0, fitstable_close(outtab));

    tab = fitstable_open(fn);
    CuAssertPtrNotNull(ct, tab);

    hdr = fitstable_get_primary_header(tab);
    CuAssertPtrNotNull(ct, hdr);
    CuAssertIntEquals(ct, 42, qfits_header_getint(hdr, "TSTHDR", -1));

    hdr = fitstable_get_header(tab);
    CuAssertPtrNotNull(ct, hdr);
    CuAssertIntEquals(ct, 99, qfits_header_getint(hdr, "TSTHDR2", -1));

    fitstable_open_next_extension(tab);
    hdr = fitstable_get_header(tab);
    CuAssertPtrNotNull(ct, hdr);
    CuAssertIntEquals(ct, 101, qfits_header_getint(hdr, "TSTHDR3", -1));

    fitstable_open_next_extension(tab);
    hdr = fitstable_get_header(tab);
    CuAssertPtrNotNull(ct, hdr);
    CuAssertIntEquals(ct, 104, qfits_header_getint(hdr, "TSTHDR4", -1));

    CuAssertIntEquals(ct, fitstable_close(tab), 0);
}

void test_one_int_column_write_read(CuTest* ct) {
    fitstable_t* tab, *outtab;
    int i;
    int N = 100;
    int32_t outdata[N];
    int32_t* indata;
    char* fn;
    tfits_type i32 = TFITS_BIN_TYPE_J;

    fn = get_tmpfile(3);
    outtab = fitstable_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);

    fitstable_add_write_column(outtab, i32, "X", "foounits");
    CuAssertIntEquals(ct, fitstable_ncols(outtab), 1);

    CuAssertIntEquals(ct, fitstable_write_primary_header(outtab), 0);
    CuAssertIntEquals(ct, fitstable_write_header(outtab), 0);

    for (i=0; i<N; i++) {
        outdata[i] = i;
    }

    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, fitstable_write_row(outtab, outdata+i), 0);
    }
    CuAssertIntEquals(ct, fitstable_fix_header(outtab), 0);
    CuAssertIntEquals(ct, fitstable_close(outtab), 0);

    // writing shouldn't affect the data values
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, outdata[i], i);
    }

    tab = fitstable_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, N, fitstable_nrows(tab));

    indata = fitstable_read_column(tab, "X", i32);
    CuAssertPtrNotNull(ct, indata);
    CuAssertIntEquals(ct, memcmp(outdata, indata, sizeof(outdata)), 0);
    CuAssertIntEquals(ct, 0, fitstable_close(tab));
}

void test_two_columns_with_conversion(CuTest* ct) {
    fitstable_t* tab, *outtab;
    int i;
    int N = 100;
    int32_t outx[N];
    double outy[N];
    double* inx;
    int32_t* iny;
    char* fn;

    tfits_type i32 = TFITS_BIN_TYPE_J;
    tfits_type dubl = fitscolumn_double_type();

    fn = get_tmpfile(4);
    outtab = fitstable_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);

    fitstable_add_write_column(outtab, i32,  "X", "foounits");
    fitstable_add_write_column(outtab, dubl, "Y", "foounits");
    CuAssertIntEquals(ct, 2, fitstable_ncols(outtab));

    CuAssertIntEquals(ct, 0, fitstable_write_primary_header(outtab));
    CuAssertIntEquals(ct, 0, fitstable_write_header(outtab));

    for (i=0; i<N; i++) {
      outx[i] = i;
      outy[i] = i+1000;
    }

    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, 0, fitstable_write_row(outtab, outx+i, outy+i));
    }
    CuAssertIntEquals(ct, 0, fitstable_fix_header(outtab));
    CuAssertIntEquals(ct, 0, fitstable_close(outtab));

    // writing shouldn't affect the data values
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, i, outx[i]);
        CuAssertIntEquals(ct, i+1000, outy[i]);
    }

    tab = fitstable_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, N, fitstable_nrows(tab));

    inx = fitstable_read_column(tab, "X", dubl);
    CuAssertPtrNotNull(ct, inx);
    iny = fitstable_read_column(tab, "Y", i32);
    CuAssertPtrNotNull(ct, iny);

    for (i=0; i<N; i++) {
        /*
         printf("inx %g, outx %i, iny %i, outy %g\n",
         inx[i], outx[i], iny[i], outy[i]);
         */
        CuAssertIntEquals(ct, outx[i], (int)inx[i]);
        CuAssertIntEquals(ct, (int)outy[i], iny[i]);
    }
    CuAssertIntEquals(ct, 0, fitstable_close(tab));
}

void test_arrays(CuTest* ct) {
    fitstable_t* tab, *outtab;
    int i;
    int N = 100;
    int DX = 4;
    int DY = 3;
    int32_t outx[N*DX];
    double outy[N*DY];
    double* inx;
    int32_t* iny;
    char* fn;
    int d, t;

    tfits_type i32 = TFITS_BIN_TYPE_J;
    tfits_type dubl = fitscolumn_double_type();

    fn = get_tmpfile(5);
    outtab = fitstable_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);
    CuAssertIntEquals(ct, 0, fitstable_write_primary_header(outtab));

    // first extension: arrays

    fitstable_add_write_column_array(outtab, i32,  DX, "X", "foounits");
    fitstable_add_write_column_array(outtab, dubl, DY, "Y", "foounits");
    CuAssertIntEquals(ct, 2, fitstable_ncols(outtab));
    CuAssertIntEquals(ct, 0, fitstable_write_header(outtab));

    for (i=0; i<N*DX; i++)
      outx[i] = i;
    for (i=0; i<N*DY; i++)
      outy[i] = i+1000;

    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, 0, fitstable_write_row(outtab, outx+i*DX, outy+i*DY));
    }
    CuAssertIntEquals(ct, 0, fitstable_fix_header(outtab));

    // writing shouldn't affect the data values
    for (i=0; i<N*DX; i++)
        CuAssertIntEquals(ct, i, outx[i]);
    for (i=0; i<N*DY; i++)
        CuAssertIntEquals(ct, i+1000, outy[i]);


    // second extension: scalars
    fitstable_next_extension(outtab);
    fitstable_clear_table(outtab);

    fitstable_add_write_column(outtab, i32,  "X", "foounits");
    fitstable_add_write_column(outtab, dubl, "Y", "foounits");
    CuAssertIntEquals(ct, 2, fitstable_ncols(outtab));
    CuAssertIntEquals(ct, 0, fitstable_write_header(outtab));

    for (i=0; i<N; i++)
        CuAssertIntEquals(ct, 0, fitstable_write_row(outtab, outx+i, outy+i));
    CuAssertIntEquals(ct, 0, fitstable_fix_header(outtab));

    // writing shouldn't affect the data values
    for (i=0; i<N*DX; i++)
        CuAssertIntEquals(ct, i, outx[i]);
    for (i=0; i<N*DY; i++)
        CuAssertIntEquals(ct, i+1000, outy[i]);

    CuAssertIntEquals(ct, 0, fitstable_close(outtab));



    tab = fitstable_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, N, fitstable_nrows(tab));

    d = fitstable_get_array_size(tab, "X");
    t = fitstable_get_type(tab, "X");

    CuAssertIntEquals(ct, DX, d);
    CuAssertIntEquals(ct, i32, t);

    d = fitstable_get_array_size(tab, "Y");
    t = fitstable_get_type(tab, "Y");

    CuAssertIntEquals(ct, DY, d);
    CuAssertIntEquals(ct, dubl, t);

    inx = fitstable_read_column_array(tab, "X", dubl);
    CuAssertPtrNotNull(ct, inx);
    iny = fitstable_read_column_array(tab, "Y", i32);
    CuAssertPtrNotNull(ct, iny);

    for (i=0; i<N*DX; i++)
        CuAssertIntEquals(ct, outx[i], (int)inx[i]);
    for (i=0; i<N*DY; i++)
        CuAssertIntEquals(ct, (int)outy[i], iny[i]);
    CuAssertIntEquals(ct, 0, fitstable_close(tab));
}

void test_conversion(CuTest* ct) {
    fitstable_t* tab, *outtab;
    int i;
    int N = 100;
    int D = 3;
    double out[N*D];
    double* in;
    char* fn;
    int d, t;

    tfits_type i32 = TFITS_BIN_TYPE_J;
    tfits_type dubl = fitscolumn_double_type();

    fn = get_tmpfile(6);
    outtab = fitstable_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);
    CuAssertIntEquals(ct, 0, fitstable_write_primary_header(outtab));

    fitstable_add_write_column_array_convert(outtab, i32,  dubl, D, "X", "foounits");
    CuAssertIntEquals(ct, 1, fitstable_ncols(outtab));
    CuAssertIntEquals(ct, 0, fitstable_write_header(outtab));

    for (i=0; i<N*D; i++)
        out[i] = i;

    for (i=0; i<N; i++)
        CuAssertIntEquals(ct, 0, fitstable_write_row(outtab, out+i*D));
    CuAssertIntEquals(ct, 0, fitstable_fix_header(outtab));

    // writing shouldn't affect the data values
    for (i=0; i<N*D; i++)
        CuAssertIntEquals(ct, i, out[i]);

    CuAssertIntEquals(ct, 0, fitstable_close(outtab));

    tab = fitstable_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, N, fitstable_nrows(tab));

    d = fitstable_get_array_size(tab, "X");
    t = fitstable_get_type(tab, "X");

    CuAssertIntEquals(ct, D, d);
    CuAssertIntEquals(ct, i32, t);

    in = fitstable_read_column_array(tab, "X", dubl);
    CuAssertPtrNotNull(ct, in);

    for (i=0; i<N*D; i++)
        CuAssertIntEquals(ct, out[i], (int)in[i]);
    CuAssertIntEquals(ct, 0, fitstable_close(tab));
}

struct ts1 {
    int x1;
    int x2[3];
    double x3;
    double x4;
};
typedef struct ts1 ts1;

struct ts2 {
    double x1;
    int16_t x2[3];
    int x3;
    float x4;
};
typedef struct ts2 ts2;

void test_struct_1(CuTest* ct) {
    fitstable_t* tab, *outtab;
    tfits_type i16 = TFITS_BIN_TYPE_I;
    tfits_type itype = fitscolumn_int_type();
    tfits_type dubl = fitscolumn_double_type();
    tfits_type flt = fitscolumn_float_type();
    char* fn;
    int i, N = 100;
    ts1 x[N];
    ts2 y[N];
    int d, t;

    fn = get_tmpfile(7);
    outtab = fitstable_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);
    CuAssertIntEquals(ct, 0, fitstable_write_primary_header(outtab));

    fitstable_add_write_column_struct(outtab, itype, 1, offsetof(ts1, x1),
                                      itype, "X1", "x1units");
    fitstable_add_write_column_struct(outtab, itype, 3, offsetof(ts1, x2),
                                      i16, "X2", "x2units");
    fitstable_add_write_column_struct(outtab, dubl, 1, offsetof(ts1, x3),
                                      dubl, "X3", "x3units");
    fitstable_add_write_column_struct(outtab, dubl, 1, offsetof(ts1, x4),
                                      flt, "X4", "x4units");
    CuAssertIntEquals(ct, 0, fitstable_write_header(outtab));

    for (i=0; i<N; i++) {
        x[i].x1 = i;
        x[i].x2[0] = 1000 + i;
        x[i].x2[1] = 2000 + i;
        x[i].x2[2] = 3000 + i;
        x[i].x3 = i * 1000.0;
        x[i].x4 = i * 1e6;
        CuAssertIntEquals(ct, 0, fitstable_write_struct(outtab, x+i));
    }

    CuAssertIntEquals(ct, 0, fitstable_fix_header(outtab));
    CuAssertIntEquals(ct, 0, fitstable_close(outtab));

    tab = fitstable_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, N, fitstable_nrows(tab));

    d = fitstable_get_array_size(tab, "X1");
    t = fitstable_get_type(tab, "X1");
    CuAssertIntEquals(ct, 1, d);
    CuAssertIntEquals(ct, itype, t);

    d = fitstable_get_array_size(tab, "X2");
    t = fitstable_get_type(tab, "X2");
    CuAssertIntEquals(ct, 3, d);
    CuAssertIntEquals(ct, i16, t);

    d = fitstable_get_array_size(tab, "X3");
    t = fitstable_get_type(tab, "X3");
    CuAssertIntEquals(ct, 1, d);
    CuAssertIntEquals(ct, dubl, t);

    d = fitstable_get_array_size(tab, "X4");
    t = fitstable_get_type(tab, "X4");
    CuAssertIntEquals(ct, 1, d);
    CuAssertIntEquals(ct, flt, t);

    fitstable_add_read_column_struct(tab, dubl, 1, offsetof(ts2, x1),
                                     itype, "X1");
    fitstable_add_read_column_struct(tab, i16, 3, offsetof(ts2, x2),
                                     i16, "X2");
    fitstable_add_read_column_struct(tab, itype, 1, offsetof(ts2, x3),
                                     dubl, "X3");
    fitstable_add_read_column_struct(tab, flt, 1, offsetof(ts2, x4),
                                     flt, "X4");

    fitstable_read_extension(tab, 1);

    for (i=0; i<N; i++) {
        double eps = 1e-10;
        CuAssertIntEquals(ct, 0, fitstable_read_struct(tab, y+i));
        CuAssertDblEquals(ct, x[i].x1, y[i].x1, eps);
        CuAssertIntEquals(ct, x[i].x2[0], y[i].x2[0]);
        CuAssertIntEquals(ct, x[i].x2[1], y[i].x2[1]);
        CuAssertIntEquals(ct, x[i].x2[2], y[i].x2[2]);
        CuAssertIntEquals(ct, x[i].x3, y[i].x3);
        CuAssertDblEquals(ct, x[i].x4, y[i].x4, eps);
    }

}


