#include <math.h>
#include <stdio.h>

#include "CuTest.h"
#include "blocklist.h"

void test_delete(CuTest* tc) {
	blocklist* bl;
	bl = blocklist_int_new(4);
	blocklist_int_append(bl, 42);
	blocklist_int_append(bl, 43);
	blocklist_int_append(bl, 47);
	blocklist_int_append(bl, 49);

	blocklist_remove_index(bl, 0);
	blocklist_remove_index(bl, 0);
	blocklist_remove_index(bl, 0);
	blocklist_remove_index(bl, 0);

	CuAssertIntEquals(tc, blocklist_count(bl), 0);
	CuAssertPtrEquals(tc, bl->head, NULL);
	CuAssertPtrEquals(tc, bl->tail, NULL);
}

void test_delete_2(CuTest* tc) {
	blocklist* bl;
	bl = blocklist_int_new(2);
	blocklist_int_append(bl, 42);
	blocklist_int_append(bl, 43);
	blocklist_int_append(bl, 47);
	blocklist_int_append(bl, 49);

	blocklist_remove_index(bl, 0);
	blocklist_remove_index(bl, 0);
	blocklist_remove_index(bl, 0);
	blocklist_remove_index(bl, 0);

	CuAssertIntEquals(tc, blocklist_count(bl), 0);
	CuAssertPtrEquals(tc, bl->head, NULL);
	CuAssertPtrEquals(tc, bl->tail, NULL);
}

void test_delete_3(CuTest* tc) {
	blocklist* bl;
	bl = blocklist_int_new(2);
	blocklist_int_append(bl, 42);
	blocklist_int_append(bl, 43);
	blocklist_int_append(bl, 47);
	blocklist_int_append(bl, 49);

	blocklist_remove_index(bl, 3);
	blocklist_remove_index(bl, 2);
	blocklist_remove_index(bl, 1);
	blocklist_remove_index(bl, 0);

	CuAssertIntEquals(tc, blocklist_count(bl), 0);
	CuAssertPtrEquals(tc, bl->head, NULL);
	CuAssertPtrEquals(tc, bl->tail, NULL);
}

void test_delete_4(CuTest* tc) {
	blocklist* bl;
	int i, j, N;
	bl = blocklist_int_new(20);
	N = 10000;
	for (i=0; i<N; i++)
		blocklist_int_append(bl, i);

	for (i=0; i<N; i++) {
		int ind = rand() % blocklist_count(bl);
		blocklist_remove_index(bl, ind);

		for (j=1; j<blocklist_count(bl); j++) {
			CuAssert(tc, "mono", (blocklist_int_access(bl, j) - blocklist_int_access(bl, j-1)) > 0);
		}
	}

	CuAssertIntEquals(tc, blocklist_count(bl), 0);
	CuAssertPtrEquals(tc, bl->head, NULL);
	CuAssertPtrEquals(tc, bl->tail, NULL);
}

int main(int argc, char** args) {
	/* Run all tests */
	CuString *output = CuStringNew();
	CuSuite* suite = CuSuiteNew();

	/* Add new tests here */
	SUITE_ADD_TEST(suite, test_delete);
	SUITE_ADD_TEST(suite, test_delete_2);
	SUITE_ADD_TEST(suite, test_delete_3);
	SUITE_ADD_TEST(suite, test_delete_4);

	/* Run the suite, collect results and display */
	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);

	return 0;
}
