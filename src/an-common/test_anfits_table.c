#include "anfits.h"
#include "fitsioutils.h"

#include "cutest.h"

char* get_tmpfile(int i) {
    if (i == 1) {
        return "/tmp/test-anfits-1";
    } else if (i == 2) {
        return "/tmp/test-anfits-2";
    } else if (i == 3) {
        return "/tmp/test-anfits-3";
    } else {
        return "/tmp/test-anfits-4";
    }
}

void test_one_column_write_read(CuTest* ct) {
    anfits_table_t* tab, *outtab;
    int i;
    int N = 6;
    double outdata[6];
    double* indata;
    char* fn;

    fn = get_tmpfile(1);
    outtab = anfits_table_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);
    
    anfits_table_add_column(outtab, ANTYPE_DOUBLE, "X", "foounits");
    CuAssertIntEquals(ct, anfits_table_ncols(outtab), 1);

    CuAssertIntEquals(ct, anfits_table_write_primary_header(outtab), 0);
    CuAssertIntEquals(ct, anfits_table_write_header(outtab), 0);

    for (i=0; i<N; i++) {
        outdata[i] = i*i;
    }

    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, anfits_table_write_row(outtab, outdata+i), 0);
    }
    CuAssertIntEquals(ct, anfits_table_fix_header(outtab), 0);
    CuAssertIntEquals(ct, anfits_table_close(outtab), 0);

    // writing shouldn't affect the data values
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, outdata[i], i*i);
    }

    tab = anfits_table_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, N, anfits_table_nrows(tab));

    indata = anfits_table_read_column(tab, "X", ANTYPE_DOUBLE);
    CuAssertPtrNotNull(ct, indata);
    CuAssertIntEquals(ct, memcmp(outdata, indata, sizeof(outdata)), 0);
}

void test_one_int_column_write_read(CuTest* ct) {
    anfits_table_t* tab, *outtab;
    int i;
    int N = 100;
    int32_t outdata[N];
    int32_t* indata;
    char* fn;

    fn = get_tmpfile(2);
    outtab = anfits_table_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);
    
    anfits_table_add_column(outtab, ANTYPE_INT32, "X", "foounits");
    CuAssertIntEquals(ct, anfits_table_ncols(outtab), 1);

    CuAssertIntEquals(ct, anfits_table_write_header(outtab), 0);

    for (i=0; i<N; i++) {
        outdata[i] = i;
    }

    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, anfits_table_write_row(outtab, outdata+i), 0);
    }
    CuAssertIntEquals(ct, anfits_table_close(outtab), 0);

    // writing shouldn't affect the data values
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, outdata[i], i);
    }

    tab = anfits_table_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, N, anfits_table_nrows(tab));

    indata = anfits_table_read_column(tab, "X", ANTYPE_INT32);
    CuAssertPtrNotNull(ct, indata);
    CuAssertIntEquals(ct, memcmp(outdata, indata, sizeof(outdata)), 0);
}

void test_two_columns_with_conversion(CuTest* ct) {
    anfits_table_t* tab, *outtab;
    int i;
    int N = 100;
    int32_t outx[N];
    double outy[N];
    double* inx;
    int32_t* iny;
    char* fn;

    fn = get_tmpfile(3);
    outtab = anfits_table_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);
    
    anfits_table_add_column(outtab, ANTYPE_INT32, "X", "foounits");
    anfits_table_add_column(outtab, ANTYPE_DOUBLE, "Y", "foounits");
    CuAssertIntEquals(ct, anfits_table_ncols(outtab), 2);

    CuAssertIntEquals(ct, anfits_table_write_header(outtab), 0);

    for (i=0; i<N; i++) {
      outx[i] = i;
      outy[i] = i+1000;
    }

    for (i=0; i<N; i++) {
      CuAssertIntEquals(ct, anfits_table_write_row(outtab, outx+i, outy+i), 0);
    }
    CuAssertIntEquals(ct, anfits_table_close(outtab), 0);

    // writing shouldn't affect the data values
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, outx[i], i);
        CuAssertIntEquals(ct, outy[i], i+1000);
    }

    tab = anfits_table_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, N, anfits_table_nrows(tab));

    inx = anfits_table_read_column(tab, "X", ANTYPE_DOUBLE);
    CuAssertPtrNotNull(ct, inx);
    iny = anfits_table_read_column(tab, "Y", ANTYPE_INT32);
    CuAssertPtrNotNull(ct, iny);

    for (i=0; i<N; i++) {
      /*
	printf("inx %g, outx %i, iny %i, outy %g\n",
	inx[i], outx[i], iny[i], outy[i]);
      */
      CuAssertIntEquals(ct, outx[i], (int)inx[i]);
        CuAssertIntEquals(ct, (int)outy[i], iny[i]);
    }
    CuAssertIntEquals(ct, anfits_table_close(tab), 0);
}


void test_headers(CuTest* ct) {
    anfits_table_t* tab, *outtab;
    char* fn;
    qfits_header* hdr;

    fn = get_tmpfile(4);
    outtab = anfits_table_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);

    hdr = anfits_table_get_primary_header(outtab);
    CuAssertPtrNotNull(ct, hdr);
    fits_header_add_int(hdr, "TSTHDR", 42, "Test Comment");

    hdr = anfits_table_get_header(outtab);
    fits_header_add_int(hdr, "TSTHDR2", 99, "Test 2 Comment");

    CuAssertIntEquals(ct, 0, anfits_table_write_header(outtab));
    
    CuAssertIntEquals(ct, 0, anfits_table_ncols(outtab));

    CuAssertIntEquals(ct, 0, anfits_table_close(outtab));

    tab = anfits_table_open(fn);
    CuAssertPtrNotNull(ct, tab);

    hdr = anfits_table_get_primary_header(tab);
    CuAssertPtrNotNull(ct, hdr);
    CuAssertIntEquals(ct, 42, qfits_header_getint(hdr, "TSTHDR", -1));

    hdr = anfits_table_get_header(tab);
    CuAssertPtrNotNull(ct, hdr);
    CuAssertIntEquals(ct, 99, qfits_header_getint(hdr, "TSTHDR2", -1));

    CuAssertIntEquals(ct, anfits_table_close(tab), 0);
}
