double hoggLogsum(double * list,int nn);
double hoggLogGaussian(double xx, double var);
double hoggExMaxStep(double *xx, double *varin, int nn, double varout,
		     double *logpin);
double hoggLeastSquareFit(double *yy, double *xx, double *aa, int nn, int mm);
double hoggRansacTrial(double *yy, double *AA, double *xx, int nn, int mm,
		       int *use);
