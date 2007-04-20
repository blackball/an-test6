#include <stdio.h>
#include <math.h>

#include "tilerender.h"
#include "render_rdls.h"
#include "rdlist.h"
#include "starutil.h"
#include "mathutil.h"
#include "mercrender.h"

char* rdls_dirs[] = {
    "/home/gmaps/gmaps-rdls/",
    "/home/gmaps/ontheweb-data/",
    "./"
};

int render_rdls(unsigned char* img, render_args_t* args)
{
    int i,j,Nstars,Nib;
    xylist* rdls;
    double* rdvals;
    float* fluximg;

    /* Search in the rdls paths */
    for (i=0; i<sizeof(rdls_dirs)/sizeof(char*); i++) {
        char fn[256];
        snprintf(fn, sizeof(fn), "%s/%s", rdls_dirs[i], args->rdlsfn);
        fprintf(stderr, "render_rdls: Trying file: %s\n", fn);
        rdls = rdlist_open(fn);
        if (rdls)
            break;
    }
    if (!rdls) {
        fprintf(stderr, "render_rdls: Failed to open RDLS file.\n");
        return -1;
    }

    Nstars = rdlist_n_entries(rdls, args->fieldnum);
    if (Nstars == -1) {
        fprintf(stderr, "render_rdls: Failed to read RDLS file.\n");
        return -1;
    }

    if (args->Nstars && args->Nstars < Nstars)
        Nstars = args->Nstars;

    rdvals = malloc(Nstars * 2 * sizeof(double));
    if (rdlist_read_entries(rdls, args->fieldnum, 0, Nstars, rdvals)) {
        fprintf(stderr, "render_rdls: Failed to read RDLS file.\n");
        free(rdvals);
        return -1;
    }

    fprintf(stderr, "render_rdls: Got %i stars.\n", Nstars);

    fluximg = calloc(args->W*args->H*3, sizeof(float));
    if (!fluximg) {
        fprintf(stderr, "render_rdls: Failed to allocate flux image.\n");
        return -1;
    }

    Nib = 0;
    for (i=0; i<Nstars; i++) {
        double px =  ra2merc(deg2rad(rdvals[i*2+0]));
        double py = dec2merc(deg2rad(rdvals[i*2+1]));

        /* This is just a hack by using 255.0.... */
        //fprintf(stderr, "render_rdls: %g %g %d\n", px, py, Nib);
        //Nib += add_star(px, py, 205.0, 355.0, 0.0, fluximg, RENDERSYMBOL_o, args);
        Nib += add_star(px, py, 255.0, 205.0, 0.0, fluximg, RENDERSYMBOL_o, args);
    }

    fprintf(stderr, "render_rdls: %i stars inside image bounds.\n", Nib);

    for (j=0; j<args->H; j++) {
        for (i=0; i<args->W; i++) {
            unsigned char* pix = pixel(i,j, img, args);
            pix[0] = min(fluximg[3*(j*args->W+i)+0], 255);
            pix[1] = min(fluximg[3*(j*args->W+i)+1], 255);
            pix[2] = min(fluximg[3*(j*args->W+i)+2], 255);
            //pix[3] = (pix[0]+pix[1]+pix[2])/3.0;
            pix[3] = 0;
            // hack; adds a black 'outline' 
            if (pix[0] || pix[1] || pix[2]) {
                pix[3] = 95;
            }
        }
    }

    free(fluximg);
    free(rdvals);

    return 0;
}
