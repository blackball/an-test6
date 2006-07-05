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

#include "fileutil.h"
#include "starutil.h"
#include "codefile.h"

#define OPTIONS "hf:p"

extern char *optarg;
extern int optind, opterr, optopt;

static void print_help(char* progname)
{
	fprintf(stderr, "Usage: %s -f <code-file>\n"
			"       [-p]: don't do all code permutations.\n\n",
	        progname);
}

int** hists;
int Nbins = 20;
int Dims;
// single-dimension hists
int Nsingle = 100;
int* single;

static void add_to_single_histogram(int dim, double val)
{
	int* hist = single + Nsingle * dim;
	int bin = (int)(val * Nsingle);
	if (bin >= Nsingle) {
		bin = Nsingle;
		printf("truncating value %g\n", val);
	}
	if (bin < 0) {
		bin = 0;
		printf("truncating (up) value %g\n", val);
	}
	hist[bin]++;
}

static void add_to_histogram(int dim1, int dim2, double val1, double val2)
{
	int xbin, ybin;
	int* hist = hists[dim1 * Dims + dim2];
	xbin = (int)(val1 * Nbins);
	if (xbin >= Nbins) {
		xbin = Nbins;
		printf("truncating value %g\n", val1);
	}
	if (xbin < 0) {
		xbin = 0;
		printf("truncating (up) value %g\n", val1);
	}
	ybin = (int)(val2 * Nbins);
	if (ybin >= Nbins) {
		ybin = Nbins;
		printf("truncating value %g\n", val2);
	}
	if (ybin < 0) {
		ybin = 0;
		printf("truncating (up) value %g\n", val2);
	}

	hist[xbin * Nbins + ybin]++;
}


int main(int argc, char *argv[])
{
	int argchar;
	char *codefname = NULL;
	int i, d, e;
	bool allperms = TRUE;
	double onecode[4];

	if (argc <= 2) {
		print_help(argv[0]);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'f':
			codefname = optarg;
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

	if (!codefname) {
		print_help(argv[0]);
		return (OPT_ERR);
	}

	codefile* cf = codefile_open(codefname, 0);

	fprintf(stderr, "Number of codes: %i\n", cf->numcodes);
	fprintf(stderr, "Number of stars in catalogue: %i\n", cf->numstars);
	fprintf(stderr, "Index scale lower: %g\n", cf->index_scale_lower);
	fprintf(stderr, "Index scale upper: %g\n", cf->index_scale);

	// Allocate memory for projection histograms
	Dims = 4;
	hists = calloc(Dims * Dims, sizeof(int*));

	for (d = 0; d < Dims; d++) {
		for (e = 0; e < d; e++)
			hists[d*Dims + e] = calloc(Nbins * Nbins, sizeof(int));
		// Since the 4x4 matrix of histograms is actually symmetric,
		// only make half
		for (; e < Dims; e++)
			hists[d*Dims + e] = NULL;
	}

	single = calloc(Dims * Nsingle, sizeof(int));

	for (i = 0; i < cf->numcodes; i++) {
		int perm;
		double permcode[4];
		codefile_get_code(cf, i,
		                  onecode, onecode+1, onecode+2, onecode+3);

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
			}
		} else {
			for (d = 0; d < Dims; d++) {
				for (e = 0; e < d; e++) {
					add_to_histogram(d, e, onecode[d], onecode[e]);
				}
				add_to_single_histogram(d, onecode[d]);
			}
		}
	}

	codefile_close(cf);

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
		}
		printf("hist_%i=[", d);
		for (i = 0; i < Nsingle; i++) {
			printf("%i,", single[d*Nsingle + i]);
		}
		printf("];\n");
	}
	free(hists);
	free(single);

	fprintf(stderr, "Done!\n");

	return 0;
}



