#include "anfits.h"

#include "cutest.h"

char* get_tmpfile() {
    return "/tmp/test-anfits-1";
}

void test_one_column_write_read(CuTest* ct) {
    anfits_table_t* tab, *outtab;
    int i, N2;
    int N = 6;
    double outdata[6];
    double* indata;
    char* fn;
    char* colname = "X";

    fn = get_tmpfile();
    outtab = anfits_table_open_for_writing(fn);
    CuAssertPtrNotNull(ct, outtab);
    
    anfits_table_add_column(outtab, colname, ANTYPE_DOUBLE);
    CuAssertIntEquals(ct, anfits_table_ncols(outtab), 1);

    for (i=0; i<N; i++) {
        outdata[i] = i*i;
    }

    for (i=0; i<N; i++) {
        CuAssertIntEquals(ct, anfits_table_write_row(outtab, outdata+i), 0);
    }
    CuAssertIntEquals(ct, anfits_table_close(outtab), 0);

    tab = anfits_table_open(fn);
    CuAssertPtrNotNull(ct, tab);
    CuAssertIntEquals(ct, anfits_table_nrows(tab), N);

    anfits_table_read_column(tab, colname, ANTYPE_DOUBLE, &indata);
    CuAssertPtrNotNull(ct, indata);
    CuAssertIntEquals(ct, memcmp(outdata, indata, sizeof(outdata)), 0);
}
