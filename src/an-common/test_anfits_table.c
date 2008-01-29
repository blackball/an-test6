#include "anfits.h"

#include "cutest.h"

char* get_tmpfile(int i) {
    if (i == 1) {
        return "/tmp/test-anfits-1";
    } else {
        return "/tmp/test-anfits-2";
    }
}

void test_one_int_column_write_read(CuTest* ct) {
    anfits_table_t* tab, *outtab;
    int i;
    int N = 100;
    int32_t outdata[N];
    int32_t* indata;
    char* fn;

    fn = get_tmpfile(1);
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
    CuAssertIntEquals(ct, anfits_table_nrows(tab), N);

    indata = anfits_table_read_column(tab, "X", ANTYPE_INT32);
    CuAssertPtrNotNull(ct, indata);
    CuAssertIntEquals(ct, memcmp(outdata, indata, sizeof(outdata)), 0);
}

void test_one_column_write_read(CuTest* ct) {
    anfits_table_t* tab, *outtab;
    int i;
    int N = 6;
    double outdata[6];
    double* indata;
    char* fn;

    fn = get_tmpfile(2);
    outtab = anfits_table_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);
    
    anfits_table_add_column(outtab, ANTYPE_DOUBLE, "X", "foounits");
    CuAssertIntEquals(ct, anfits_table_ncols(outtab), 1);

    CuAssertIntEquals(ct, anfits_table_write_header(outtab), 0);

    for (i=0; i<N; i++) {
        outdata[i] = i*i;
    }

    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, anfits_table_write_row(outtab, outdata+i), 0);
    }
    CuAssertIntEquals(ct, anfits_table_close(outtab), 0);

    // writing shouldn't affect the data values
    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, outdata[i], i*i);
    }

    tab = anfits_table_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, anfits_table_nrows(tab), N);

    indata = anfits_table_read_column(tab, "X", ANTYPE_DOUBLE);
    CuAssertPtrNotNull(ct, indata);
    CuAssertIntEquals(ct, memcmp(outdata, indata, sizeof(outdata)), 0);
}
