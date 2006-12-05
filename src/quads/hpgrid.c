/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "healpix.h"
#include "starutil.h"
#include "mathutil.h"

#define OPTIONS "hN:"

void print_help(char* progname) {
    printf("usage:\n\n"
		   "%s\n"
		   "  [-N nside]  (default 1)\n"
		   "\n", progname);
}

int main(int argc, char** args) {
    int c;
	int Nside = 1;
	int HP, hp;
	int i;
	double* radecs;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'N':
			Nside = atoi(optarg);
			break;
		}
	}

	HP = 12 * Nside * Nside;

	printf("Nside=%i;\n", Nside);

	radecs = malloc(HP * 2 * sizeof(double));
	
	for (hp=0; hp<HP; hp++) {
		double xyz[3];
		double ra, dec;
		//healpix_to_xyzarr_lex(0.0, 0.0, hp, Nside, xyz);
		healpix_to_xyzarr_lex(0.5, 0.5, hp, Nside, xyz);
		xyzarr2radec(xyz, &ra, &dec);
		radecs[2*hp] = ra;
		radecs[2*hp+1] = dec;
	}

	printf("figure(1);\n");
	printf("clf;\n");
	printf("xmargin=0.5; ymargin=0.1;\n");
	printf("axis([0-xmargin, 2*pi+xmargin, -pi/2-ymargin, pi/2+ymargin]);\n");
	printf("texts=[];\n");
	printf("lines=[];\n");

	/*
	  printf("function [newh] = wrapline(h)\n"
	  "  x=get(h,'XData');\n"
	  "  y=get(h,'YData');\n"
	  // are points all on one side or the other?
	  //"  if prod(double(x<1.5*pi)), return []; end;\n"
	  //"  if prod(double(x>0.5*pi)), return []; end;\n"
	  // do points span the wrap-around?
	  "  if ~(sum(double(x<0.5*pi)) && sum(double(x>1.5*pi))), return []; end;\n"
	  // copy any line segment that crosses between x<pi/2 and x>3pi/2.
	  "  newx=[]; newy=[];\n"
	  "  for i=1:length(x)-1,\n"
	  "    if (x(i)<pi/2 && x(i+1)>3*pi/2) || (x(i)>3*pi/2 && x(i+1)<pi/2),\n"
	  "      newx(length(newx)+[1:2])=[x(i),x(i+1)];\n"
	  "      newy(length(newy)+[1:2])=[y(i),y(i+1)];\n"
	  "    end;\n"
	  "  end;\n"
	  "  newh=copyobj(h,get(h,'Parent'));\n"
	  "  set(newh,'XData',newx);\n"
	  "  set(newh,'YData',newy);\n"
	  "return;\n");
	*/
	for (hp=0; hp<12; hp++) {
		double xyz[3];
		double crd[8*2];
		double xy[] = { 0.0,0.0,   0.0,0.001,   0.0,1.0,
						0.999,1.0, 1.0,1.0,     1.0,0.999,
						1.0,0.0,   0.001,0.0 };
		for (i=0; i<8; i++) {
			healpix_to_xyzarr_lex(xy[i*2], xy[i*2+1], hp, 1, xyz);
			xyzarr2radec(xyz, crd+i*2+0, crd+i*2+1);
		}
		printf("la(%i)=line([", hp+1);
		for (i=0; i<9; i++)
			printf("%g,", crd[(i%8)*2+0]);
		printf("], [");
		for (i=0; i<9; i++)
			printf("%g,", crd[(i%8)*2+1]);
		printf("]);\n");
		printf("lb(%i)=wrapline(la(%i));\n", hp+1, hp+1);
		printf("set(lb(%i), 'LineStyle', '--');\n", hp+1);
	}

	for (hp=0; hp<HP; hp++) {
		static int nlines = 1;
		uint neigh[8];
		uint nn;

		printf("texts(%i)=text(%g, %g, '%i', 'HorizontalAlignment', 'center');\n",
			   hp+1, radecs[2*hp], radecs[2*hp+1], hp);

		nn = healpix_get_neighbours_nside(hp, neigh, Nside);
		for (i=0; i<nn; i++) {
			printf("lines(%i)=line([%g,%g], [%g,%g], "
				   "'Color', [0.5,0.5,0.5], "
				   "'Marker', 'o', "
				   "'MarkerEdgeColor', 'k', "
				   "'MarkerFaceColor', 'none', "
				   "'MarkerSize', 15);\n",
				   nlines++,
				   radecs[2*hp], radecs[2*neigh[i]], 
				   radecs[2*hp+1], radecs[2*neigh[i]+1]);
		}
	}

	free(radecs);

	return 0;
}

