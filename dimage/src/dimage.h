
void dcholsl(float *a, int n, float p[], float b[], float x[]);
void dcholdc(float *a, int n, float p[]);
int dfind(int *image, int nx, int ny, int *object);
int dsmooth(float *image, int nx, int ny, float sigma, float *smooth);
int dobjects(float *image, float *invvar, int nx, int ny, float dpsf, 
             float plim, float sigma, int *objects);
int dnonneg(float *xx, float *invcovar, float *bb, float offset,
            int nn, float tolerance, int maxiter, int *niter, float *chi2,
            int verbose);
int dpeaks(float *image, int nx, int ny, int *npeaks, float *xcen, 
           float *ycen, float sigma, float dlim, float saddle, int maxnpeaks,
           int smooth, float minpeak);
int dcentral(float *image, int nx, int ny, int npeaks, float *xcen, 
             float *ycen, int *central, float sigma, float dlim,    
             float saddle, int maxnpeaks);
int dmsmooth(float *image, int nx, int ny, int box, float *smooth);
int deblend(float *image, 
            float *invvar,
						int nx, 
						int ny,
						int *nchild, 
						float *xcen, 
						float *ycen, 
						float *cimages, 
						float *templates, 
            float sigma, 
            float dlim,
            float tsmooth,  /* smoothing of template */
            float tlimit,   /* lowest template value in units of sigma */
            float tfloor,   /* vals < tlimit*sigma are set to tfloor*sigma */
            float saddle,   /* number of sigma for allowed saddle */
            float parallel, /* how parallel you allow templates to be */
						int maxnchild, 
            float minpeak);
