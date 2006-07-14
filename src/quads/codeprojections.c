/**
  Reads a .code file, projects each code onto each pair of axes, and
  histograms the results.  Writes out the histograms as Matlab literals.

  Pipe the output to a file like "hists.m", then in Matlab, do

  hists
  clf;
  for i=1:3,
  for j=0:(i-1),
  subplot(3,3,(i-1)*3+j+1);
  mesh(eval(sprintf('hist_%i_%i', i, j)));
  end;
  end

  or

  for i=1:3,for j=0:(i-1),subplot(3,3,(i-1)*3+j+1);surf(eval(sprintf('hist_%i_%i', i, j)));set(gca,'XTickLabel',{});set(gca,'YTickLabel',{});end;end
  set(gca,'ZTickLabel',{});

  for i=1:3,for j=0:(i-1),subplot(3,3,(i-1)*3+j+1);surf(eval(sprintf('hist_%i_%i', i, j)));set(gca,'XTickLabel',{});set(gca,'YTickLabel',{});view(2);axis tight; end; end

  set(gca,'ZTickLabel',{});

  mx=max(max(max(max(hist_1_0,hist_2_0),max(max(hist_2_1,hist_3_0),max(hist_3_1,hist_3_2)))))
  mn=min(min(min(min(hist_1_0,hist_2_0),min(min(hist_2_1,hist_3_0),min(hist_3_1,hist_3_2)))))
  for i=1:3,for j=0:(i-1),subplot(3,3,(i-1)*3+j+1);h=eval(sprintf('hist_%i_%i', i, j));surf(h);set(gca,'XTickLabel',{});set(gca,'YTickLabel',{});a=axis;a(5)=0;a(6)=mx*1.2;axis(a);if (~((i==1)*(j==0))),set(gca,'ZTickLabel',{});end;stddev=sqrt(var(h(1:length(h)))), end; end

  mx=max(max(max(max(hist_1_0,hist_2_0),max(max(hist_2_1,hist_3_0),max(hist_3_1,hist_3_2)))))
  mn=min(min(min(min(hist_1_0,hist_2_0),min(min(hist_2_1,hist_3_0),min(hist_3_1,hist_3_2)))))
  for i=1:3,for j=0:(i-1),subplot(3,3,(i-1)*3+j+1);h=eval(sprintf('hist_%i_%i', i, j));surf(h);set(gca,'XTickLabel',{});set(gca,'YTickLabel',{});a=axis;a(5)=0;a(6)=mx*1.2;axis(a);if (~((i==1)*(j==0)));end;stddev=sqrt(var(h(1:length(h)))), end; end

  For the one-d projections, try something like
  for i=1:4, subplot(4,1,i); bar(eval(sprintf('hist_%i', (i-1)))); axis([0 100 0 1.5e5]); set(gca,'XTickLabel',{}); end

*/

#include <string.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

#include "fileutil.h"
#include "starutil.h"
#include "codefile.h"
#include "kdtree_io.h"
#include "kdtree_fits_io.h"
#include "keywords.h"

#define OPTIONS "hf:F:pd"

extern char *optarg;
extern int optind, opterr, optopt;

static void print_help(char* progname)
{
	fprintf(stderr, "Usage: %s ( -f <code-file>   OR  -F <ckdt-file> )\n"
			"       [-p]: don't do all code permutations.\n"
			"       [-d]: normalize by volume (produce density plots)\n\n",
	        progname);
}

// 2-D hists
int** hists = NULL;
double** dhists = NULL;
int Nbins = 20;
int Dims;

// 2-D hist of {C,D}x,{C,D}y
int* xyhist = NULL;
double* dxyhist = NULL;

// 1-D hists
int* single = NULL;
double* dsingle = NULL;
int Nsingle = 100;

bool do_density = FALSE;

double minvalue;
double scale;

static Const double volume_at_value(double x) {
	// codes in a circle live inside the circle
	//    (x-1/2)^2 + (y-1/2)^2 = 1/2
	// we are given "x" and want to find the distance
	// between the upper and lower arcs of the circle;
	// ie y(x)_upper - y(x)_lower.  Hence we don't care
	// about the y offset of the center of the circle and
	// we want twice the value y(x)_upper.  Ie, solve
	//    (x-1/2)^2 + y^2 = 1/2
	// for y, and return twice that.
	//    y = sqrt(1/2 - (x - 1/2)^2).
	//      = sqrt(1/2 - (x^2 - x + 1/4)
	//      = sqrt(-x^2 + x + 1/4)
	return 2.0 * sqrt(-x*x + x + 0.25);
}

static int value_to_bin(double val, int Nbins) {
	int bin = (int)((val - minvalue) * scale * Nbins);
	if (bin >= Nbins) {
		bin = Nbins-1;
		printf("truncating value %g\n", val);
	}
	if (bin < 0) {
		bin = 0;
		printf("truncating (up) value %g\n", val);
	}
	return bin;
}

static void add_to_single_histogram(int dim, double val) {
	int* hist = single + Nsingle * dim;
	int bin = value_to_bin(val, Nsingle);
	hist[bin]++;
	if (do_density) {
		double* dhist = dsingle + Nsingle * dim;
		dhist[bin] += 1.0 / volume_at_value(val);
	}
}

static void add_to_histogram(int dim1, int dim2, double val1, double val2) {
	int xbin, ybin;
	int* hist = hists[dim1 * Dims + dim2];
	xbin = value_to_bin(val1, Nbins);
	ybin = value_to_bin(val2, Nbins);
	hist[xbin * Nbins + ybin]++;
	if (do_density) {
		double* dhist = dhists[dim1 * Dims + dim2];
		double inc;
		if (dim1/2 == dim2/2)
			// (cx vs cy) or (dx vs dy); the other two dimensions are independent.
			inc = 1.0;
		else
			inc = 1.0 / (volume_at_value(val1) * volume_at_value(val2));
		dhist[xbin * Nbins + ybin] += inc;
	}
}

static void add_to_cd_histogram(double val1, double val2) {
	int xbin, ybin;
	xbin = value_to_bin(val1, Nbins);
	ybin = value_to_bin(val2, Nbins);
	xyhist[xbin * Nbins + ybin]++;
	if (do_density)
		dxyhist[xbin * Nbins + ybin] += 1.0 / (volume_at_value(val1) * volume_at_value(val2));
}

int main(int argc, char *argv[])
{
	int argchar;
	char *codefname = NULL;
	char *ckdtfname = NULL;
	int i, j, d, e;
	bool allperms = TRUE;
	bool circle;
	codefile* cf = NULL;
	kdtree_t* ckdt = NULL;
	int Ncodes;

	if (argc <= 2) {
		print_help(argv[0]);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'd':
			do_density = TRUE;
			break;
		case 'f':
			codefname = optarg;
			break;
		case 'F':
			ckdtfname = optarg;
			break;
		case 'p':
			allperms = FALSE;
			break;
		case 'h':
			print_help(argv[0]);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}

	if (!(codefname || ckdtfname)) {
		print_help(argv[0]);
		return (OPT_ERR);
	}

	if (codefname) {
		cf = codefile_open(codefname, 0);
		if (!cf) {
			fprintf(stderr, "Failed to read codefile %s.\n", codefname);
			exit(-1);
		}
		fprintf(stderr, "Number of codes: %i\n", cf->numcodes);
		fprintf(stderr, "Number of stars in catalogue: %i\n", cf->numstars);
		fprintf(stderr, "Index scale lower: %g\n", cf->index_scale_lower);
		fprintf(stderr, "Index scale upper: %g\n", cf->index_scale);
		circle = qfits_header_getboolean(cf->header, "CIRCLE", 0);
		Ncodes = cf->numcodes;
	} else {
		qfits_header* hdr;
		ckdt = kdtree_fits_read_file(ckdtfname);
		if (!ckdt) {
			fprintf(stderr, "Failed to read code kdtree file %s.\n", ckdtfname);
			exit(-1);
		}
		hdr = qfits_header_read(ckdtfname);
		if (!hdr) {
			fprintf(stderr, "Failed to read FITS header from code kdtree file %s.\n", ckdtfname);
			exit(-1);
		}
		circle = qfits_header_getboolean(hdr, "CIRCLE", 0);
		Ncodes = ckdt->ndata;
	}

	fprintf(stderr, "Index %s the CIRCLE property.\n",
			(circle ? "has" : "does not have"));

	if (circle) {
		minvalue = 0.5 - M_SQRT1_2;
		scale = M_SQRT1_2;
	} else {
		minvalue = 0.0;
		scale = 1.0;

		if (do_density) {
			fprintf(stderr, "Warning: this index does not have the CIRCLE property "
					"so the -d flag has no effect.\n");
			do_density = FALSE;
		}
	}

	// Allocate memory for projection histograms
	Dims = 4;
	hists  = calloc(Dims * Dims, sizeof(int*));
	dhists = calloc(Dims * Dims, sizeof(double*));

	for (d = 0; d < Dims; d++) {
		for (e = 0; e < d; e++) {
			 hists[d*Dims + e] = calloc(Nbins * Nbins, sizeof(int));
			dhists[d*Dims + e] = calloc(Nbins * Nbins, sizeof(double));
		}
		// Since the 4x4 matrix of histograms is actually symmetric,
		// only make half
		for (; e < Dims; e++) {
			hists [d*Dims + e] = NULL;
			dhists[d*Dims + e] = NULL;
		}
	}

	xyhist  = calloc(Nbins * Nbins, sizeof(int));
	dxyhist = calloc(Nbins * Nbins, sizeof(double));

	single  = calloc(Dims * Nsingle, sizeof(int));
	dsingle = calloc(Dims * Nsingle, sizeof(double));

	for (i=0; i<Ncodes; i++) {
		int perm;
		double codearr[4];
		double permcode[4];
		double* onecode;

		if (cf) {
			codefile_get_code(cf, i,
							  codearr, codearr+1, codearr+2, codearr+3);
			onecode = codearr;
		} else
			//memcpy(onecode, ckdt->data + i*4, 4*sizeof(double));
			onecode = ckdt->data + i*4;

		if (allperms) {
			for (perm = 0; perm < 4; perm++) {
				switch (perm) {
				case 0:
					permcode[0] = onecode[0];
					permcode[1] = onecode[1];
					permcode[2] = onecode[2];
					permcode[3] = onecode[3];
					break;
				case 1:
					permcode[0] = onecode[2];
					permcode[1] = onecode[3];
					permcode[2] = onecode[0];
					permcode[3] = onecode[1];
					break;
				case 2:
					permcode[0] = 1.0 - onecode[0];
					permcode[1] = 1.0 - onecode[1];
					permcode[2] = 1.0 - onecode[2];
					permcode[3] = 1.0 - onecode[3];
					break;
				case 3:
					permcode[0] = 1.0 - onecode[2];
					permcode[1] = 1.0 - onecode[3];
					permcode[2] = 1.0 - onecode[0];
					permcode[3] = 1.0 - onecode[1];
					break;
				}

				for (d = 0; d < Dims; d++) {
					for (e = 0; e < d; e++) {
						add_to_histogram(d, e, permcode[d], permcode[e]);
					}
					add_to_single_histogram(d, permcode[d]);
				}
				add_to_cd_histogram(permcode[0], permcode[1]);
				add_to_cd_histogram(permcode[2], permcode[3]);
			}
		} else {
			for (d = 0; d < Dims; d++) {
				for (e = 0; e < d; e++) {
					add_to_histogram(d, e, onecode[d], onecode[e]);
				}
				add_to_single_histogram(d, onecode[d]);
			}
			add_to_cd_histogram(onecode[0], onecode[1]);
			add_to_cd_histogram(onecode[2], onecode[3]);
		}
	}

	if (cf)
		codefile_close(cf);
	else
		kdtree_close(ckdt);

	for (d = 0; d < Dims; d++) {
		for (e = 0; e < d; e++) {
			int* hist;
			printf("hist_%i_%i=zeros([%i,%i]);\n",
			       d, e, Nbins, Nbins);
			hist = hists[d * Dims + e];
			for (i = 0; i < Nbins; i++) {
				int j;
				printf("hist_%i_%i(%i,:)=[", d, e, i + 1);
				for (j = 0; j < Nbins; j++) {
					printf("%i,", hist[i*Nbins + j]);
				}
				printf("];\n");
			}
			free(hist);
			if (do_density) {
				double* dhist;
				printf("dhist_%i_%i=zeros([%i,%i]);\n",
					   d, e, Nbins, Nbins);
				dhist = dhists[d * Dims + e];
				for (i = 0; i < Nbins; i++) {
					printf("dhist_%i_%i(%i,:)=[", d, e, i + 1);
					for (j = 0; j < Nbins; j++)
						printf("%g,", dhist[i*Nbins + j]);
					printf("];\n");
				}
				free(dhist);
			}
		}
		printf("hist_%i=[", d);
		for (i = 0; i < Nsingle; i++)
			printf("%i,", single[d*Nsingle + i]);
		printf("];\n");

		if (do_density) {
			printf("dhist_%i=[", d);
			for (i = 0; i < Nsingle; i++)
				printf("%g,", dsingle[d*Nsingle + i]);
			printf("];\n");
		}
	}
	printf("hist_xy=[");
	for (i=0; i<Nbins; i++) {
		for (j=0; j<Nbins; j++)
			printf("%i,", xyhist[i*Nbins+j]);
		printf(";");
	}
	printf("];\n");
	if (do_density) {
		printf("dhist_xy=[");
		for (i=0; i<Nbins; i++) {
			for (j=0; j<Nbins; j++)
				printf("%g,", dxyhist[i*Nbins+j]);
			printf(";");
		}
		printf("];\n");
	}

	free(xyhist);
	free(hists);
	free(single);
	if (do_density) {
		free(dxyhist);
		free(dhists);
		free(dsingle);
	}

	fprintf(stderr, "Done!\n");

	return 0;
}



