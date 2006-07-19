#include <stdio.h>

#include "bt.h"

static int compare_ints(const void* v1, const void* v2) {
	int i1 = *(int*)v1;
	int i2 = *(int*)v2;
	if (i1 < i2) return -1;
	if (i1 > i2) return 1;
	return 0;
}

static void print_int(void* v1) {
	int i = *(int*)v1;
	printf("%i ", i);
}

int main() {
	int val;
	int i;
	bt* tree = bt_new(sizeof(int), 4);

	printf("Empty:\n");
	bt_print(tree, print_int);
	printf("\n");

	{
		int vals[] = { 10, 5, 100, 10, 50, 50, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 200,200,200,200,200,250,150 };
		for (i=0; i<sizeof(vals)/sizeof(int); i++) {
			val = vals[i];
			printf("Insert %i:\n", val);
			bt_insert(tree, &val, 0, compare_ints);
			bt_print(tree, print_int);
			printf("\n");
		}
	}

	printf("Values: ");
	for (i=0; i<tree->N; i++) {
		printf("%i ", *(int*)bt_access(tree, i));
	}
	printf("\n");

	{
		int vals[] = { 0, 1, 2, 9, 10, 11, 49, 50, 51, 99, 100, 101, 149, 150, 151, 199, 200, 201, 249, 250, 251 };
		for (i=0; i<sizeof(vals)/sizeof(int); i++) {
			val = vals[i];
			printf("Contains %i: %s\n", val,
				   (bt_contains(tree, &val, compare_ints) ? "yes" : "no"));
		}
	}

	bt_free(tree);

	return 0;
}
