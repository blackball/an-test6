
#define GLUE3(x,y,z) x ## _ ## y ## _ ## z
#define KDMANGLE(func, r, k) GLUE3(func, r, k)


kdtree_t* KDMANGLE(kdtree_build, REAL, KDTYPE)
	 (REAL* data, int N, int D, int maxlevel, bool bb, bool copydata);
