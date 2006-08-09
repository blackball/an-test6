BEGIN{first=1;}
/^BADFIELDS/{
lst=""
for (i=6; i<=NF; i++) {
	 lst = lst " " $i;
}
if (first == 1) {
	first = 0;
} else {
	printf(",\n");
}
printf("{%i, %i, %i, \"%s\", %s}", $2, $4, $5, $3, lst);
}
END{printf("\n");}

