#include "fitstable.h"
#include "fitsioutils.h"

#include "cutest.h"

char* get_tmpfile(int i) {
    if (i == 1) {
        return "/tmp/test-fitstable-1";
    } else if (i == 2) {
        return "/tmp/test-fitstable-2";
    } else if (i == 3) {
        return "/tmp/test-fitstable-3";
    } else {
        return "/tmp/test-fitstable-4";
    }
}

void test_one_column_write_read(CuTest* ct) {
    fitstable_t* tab, *outtab;
    int i;
    int N = 6;
    double outdata[6];
    double* indata;
    char* fn;
    tfits_type dubl;

    fn = get_tmpfile(1);
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

    fn = get_tmpfile(4);
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

    fn = get_tmpfile(5);
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

#if 0
void tst_one_int_column_write_read(CuTest* ct) {
    fitstable_t* tab, *outtab;
    int i;
    int N = 100;
    int32_t outdata[N];
    int32_t* indata;
    char* fn;

    fn = get_tmpfile(2);
    outtab = fitstable_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);
    
    fitstable_add_column(outtab, ANTYPE_INT32, "X", "foounits");
    CuAssertIntEquals(ct, fitstable_ncols(outtab), 1);

    CuAssertIntEquals(ct, fitstable_write_header(outtab), 0);

    for (i=0; i<N; i++) {
        outdata[i] = i;
    }

    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, fitstable_write_row(outtab, outdata+i), 0);
    }
    CuAssertIntEquals(ct, fitstable_close(outtab), 0);

    // writing shouldn't affect the data values
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, outdata[i], i);
    }

    tab = fitstable_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, N, fitstable_nrows(tab));

    indata = fitstable_read_column(tab, "X", ANTYPE_INT32);
    CuAssertPtrNotNull(ct, indata);
    CuAssertIntEquals(ct, memcmp(outdata, indata, sizeof(outdata)), 0);
}

void tst_two_columns_with_conversion(CuTest* ct) {
    fitstable_t* tab, *outtab;
    int i;
    int N = 100;
    int32_t outx[N];
    double outy[N];
    double* inx;
    int32_t* iny;
    char* fn;

    fn = get_tmpfile(3);
    outtab = fitstable_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);
    
    fitstable_add_column(outtab, ANTYPE_INT32, "X", "foounits");
    fitstable_add_column(outtab, ANTYPE_DOUBLE, "Y", "foounits");
    CuAssertIntEquals(ct, fitstable_ncols(outtab), 2);

    CuAssertIntEquals(ct, fitstable_write_header(outtab), 0);

    for (i=0; i<N; i++) {
      outx[i] = i;
      outy[i] = i+1000;
    }

    for (i=0; i<N; i++) {
      CuAssertIntEquals(ct, fitstable_write_row(outtab, outx+i, outy+i), 0);
    }
    CuAssertIntEquals(ct, fitstable_close(outtab), 0);

    // writing shouldn't affect the data values
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, outx[i], i);
        CuAssertIntEquals(ct, outy[i], i+1000);
    }

    tab = fitstable_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, N, fitstable_nrows(tab));

    inx = fitstable_read_column(tab, "X", ANTYPE_DOUBLE);
    CuAssertPtrNotNull(ct, inx);
    iny = fitstable_read_column(tab, "Y", ANTYPE_INT32);
    CuAssertPtrNotNull(ct, iny);

    for (i=0; i<N; i++) {
      /*
	printf("inx %g, outx %i, iny %i, outy %g\n",
	inx[i], outx[i], iny[i], outy[i]);
      */
      CuAssertIntEquals(ct, outx[i], (int)inx[i]);
        CuAssertIntEquals(ct, (int)outy[i], iny[i]);
    }
    CuAssertIntEquals(ct, fitstable_close(tab), 0);
}
#endif

