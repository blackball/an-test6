/* SDL Tutorial Example 3 *
 * by Merlin262           *
 * merlin@nrrds.com       *
 **************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <limits.h>

#include <SDL/SDL.h>

#include "kdtree.h"

/* Global Variables */
SDL_Surface* mainScreen;
SDL_Surface* bumper;
int xi = 0, yi = 0;


/* Function Prototypes */
void setpixel(SDL_Surface *screen, int x, int y, Uint8 r, Uint8 g, Uint8 b);
int InitializeSDL(int argc, char* argv[]);

int double2int(double x)
{
    if (isinf(x)) 
        return INT_MAX*isinf(x);
    return rint(x);
}

void drawrect(SDL_Surface *screen, int x1, int y1, int x2, int y2, Uint8 r, Uint8 g, Uint8 b)
{
    if (x1 > screen->w)
        x1 = screen->w-1;
    if (y1 > screen->h)
        y1 = screen->h-1;
    if (x2 > screen->w)
        x2 = screen->w-1;
    if (y2 > screen->h)
        y2 = screen->h-1;
    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;
    if (x2 < 0)
        x2 = 0;
    if (y2 < 0)
        y2 = 0;

    int i;
    for (i=x1;i<=x2;i++) {
        setpixel(screen, i,y1,r,g,b);
        setpixel(screen, i,y2,r,g,b);
    }
    for (i=y1;i<=y2;i++) {
        setpixel(screen, x1,i,r,g,b);
        setpixel(screen, x2,i,r,g,b);
    }
}

/* Set pixel function */
void setpixel(SDL_Surface *screen, int x, int y, Uint8 r, Uint8 g, Uint8 b)
{
	Uint8 *ubuff8;
	Uint16 *ubuff16; 
	Uint32 *ubuff32;
	Uint32 color;

    if (x > screen->w || y > screen->h || x < 0 || y < 0)
        return;
	
	/* Lock the screen, if needed */
	if(SDL_MUSTLOCK(screen)) {
		if(SDL_LockSurface(screen) < 0) 
			return;
	}
 
	/* Get the color */
	color = SDL_MapRGB( screen->format, r, g, b );
	
	/* How we draw the pixel depends on the bitdepth */
	switch(screen->format->BytesPerPixel) {
		case 1: 
			ubuff8 = (Uint8*) screen->pixels;
			ubuff8 += (y * screen->pitch) + x; 
			*ubuff8 = (Uint8) color;
			break;
		case 2:
			ubuff16 = (Uint16*) screen->pixels;
			ubuff16 += ((y * screen->pitch)>>2) + x;
			*ubuff16 = (Uint16) color; 
			break;  
		case 3:
			ubuff8 = (Uint8*) screen->pixels;
			ubuff8 += (y * screen->pitch) + x;
					
			r = (color>>screen->format->Rshift)&0xFF;
			g = (color>>screen->format->Gshift)&0xFF;
			b = (color>>screen->format->Bshift)&0xFF;

			ubuff8[0] = r;
			ubuff8[1] = g;
			ubuff8[2] = b;
			break;
				
		case 4:
			ubuff32 = (Uint32*) screen->pixels;
			ubuff32 += ((y*screen->pitch)>>2) + x;
			*ubuff32 = color;
			break;
	
		default:
			fprintf(stderr, "Error: Unknown bitdepth!\n");
	}

	/* Unlock the screen if needed */
	if(SDL_MUSTLOCK(screen)) {
		SDL_UnlockSurface(screen);
	}
}



/* Initialize SDL */
int InitializeSDL(int argc, char* argv[]) 
{

	/* Initialize SDL */
	if( SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Could not initialize SDL: %s\n",
				SDL_GetError());
		return -1;
	}
	atexit(SDL_Quit);

	/* Create the main Window */
	mainScreen = SDL_SetVideoMode(640, 480, 16, 
			SDL_HWSURFACE|SDL_ANYFORMAT);
	
	if( !mainScreen ) {
		fprintf(stderr, "Couldn't create a surface: %s\n",
				SDL_GetError());
		return -1;
	}


	/* Set the window caption */
	SDL_WM_SetCaption("Tutorial 3 Program", "Tutorial 3 Program");
	
	return 0;
}



/* Main Program... */
int main(int argc, char* argv[])
{
	/* Declare Variables */
	SDL_Event event;
	int exitkey = 0;
	int active = 1;
	int xinc = 0;
#define NPTS 16
    double data[2*NPTS];
	int i;
	kdtree_t *kd = (kdtree_t*)0xdeadbeef;
	
	/* Initialize SDL, exit if there is an error. */
	if(InitializeSDL(argc,argv)) {
		fprintf(stderr, "Could not initialize SDL...exiting\n");
		return -1;
	}

	while(!exitkey) {
		
		while(SDL_PollEvent(&event)) {
			switch( event.type ) {
				case SDL_KEYDOWN:
					printf("Key hit - exiting!\n");
					exitkey = 1;
					break;
				case SDL_ACTIVEEVENT:
					if ( event.active.state & 
							SDL_APPACTIVE ) {
						if( event.active.gain )
							active = 1;
						else 
							active = 0;
							
					}
					break;
//                case SDL_MOUSEMOTION:
                 //   printf("Mouse moved by %d,%d to (%d,%d)\n", 
                //           event.motion.xrel, event.motion.yrel,
               //            event.motion.x, event.motion.y);
 //                   break;
                case SDL_MOUSEBUTTONDOWN:
             //       printf("Mouse button %d pressed at (%d,%d)\n",
              //             event.button.button, event.button.x, event.button.y);
                    if (xinc < 2*NPTS) {
                        setpixel(mainScreen,  event.button.x, event.button.y, 255, 255,
                                255);
                        data[xinc++] = (double)event.button.x;
                        data[xinc++] = (double)event.button.y;
                        if (xinc >= 2*NPTS) {
                            printf("Building kd tree\n");
                            kd = kdtree_build(data, NPTS, 2, 4);
			    kdtree_optimize(kd);
                            printf("...done\n");
                            //kdtree_output_dot(kd);
                            int i = 0;
#define NODE_SIZE      (sizeof(kdtree_node_t) + sizeof(real)*kd->ndim*2)
#define LOW_HR(x, n)   ((real*)   (((void*)kd->tree) + NODE_SIZE*(x) + sizeof(kdtree_node_t) + sizeof(real)*(n))      )
#define HIGH_HR(x, n)  ((real*)   (((void*)kd->tree) + NODE_SIZE*(x) + sizeof(kdtree_node_t) + sizeof(real)*((n)+kd->ndim))  )
#define NODE(x)        ((kdtree_node_t*)  (((void*)kd->tree) + NODE_SIZE*(x))           )
                            for (i=0;i<kd->nnodes;i++) {
                                /*
                                int x1=(int)*LOW_HR(i,0);
                                int y1=(int)*LOW_HR(i,1);
                                int x2=(int)*HIGH_HR(i,0);
                                int y2=(int)*HIGH_HR(i,1);
                                */
                                int x1=double2int(*LOW_HR(i,0));
                                int y1=double2int(*LOW_HR(i,1));
                                int x2=double2int(*HIGH_HR(i,0));
                                int y2=double2int(*HIGH_HR(i,1));
                                /*
                                if (isinf(*LOW_HR(i,0)))  x1 =-isinf(*LOW_HR(i,0))*x1;
                                if (isinf(*LOW_HR(i,1)))  y1 =-isinf(*LOW_HR(i,1))*y1;
                                if (isinf(*HIGH_HR(i,0))) x2 =-isinf(*HIGH_HR(i,0))*x2;
                                if (isinf(*HIGH_HR(i,1))) y2 =-isinf(*HIGH_HR(i,1))*y2;
                                */
                                drawrect(mainScreen, x1,y1,x2,y2, 0,255,0);
                                printf("LOW %d %d\nHIG %d %d\n", x1,y1,x2,y2);
                                printf("LOW %.2f %.2f\nHIG %.2f %.2f\n",
                                 *LOW_HR(i,0), *LOW_HR(i,1),
                                 *HIGH_HR(i,0),*HIGH_HR(i,1));
                                printf(" pivot val: %.2f\n", NODE(i)->pivot);
                                int level,t;
                                level = 0; t = i+1;
                                while(t>>=1) level++;
                                printf(" level: %d\n\n", level);
                                printf(" L/R %d/%d\n\n", NODE(i)->l, NODE(i)->r);
                            }
                            kdtree_output_dot(stdout, kd);
                        }
                    } else {
                        real qp[2];
                        qp[0] = (double)event.button.x;
                        qp[1] = (double)event.button.y;
                        printf("Mouse button %d pressed at (%d,%d)\n",
                                event.button.button, event.button.x, event.button.y);
                        kdtree_qres_t *kr = kdtree_rangesearch(kd, qp,
                                200.*200.);
                        if(kr->nres != 0) {

                            printf("++++++++++++++ RESULTS++++++++++++++ %d\n",
                                    kr->nres);
                            printf("---- %d x %d y -----\n",  (int)kr->results[0],
                                                  (int)kr->results[1]);
                           setpixel(mainScreen,  (int)kr->results[0]+1,
                                                  (int)kr->results[1]+1,
                                                  255, 0, 0);
                            setpixel(mainScreen,  (int)kr->results[0]-1,
                                                  (int)kr->results[1]-1,
                                                  255, 0, 0);
                        }

                            for (i=0;i<NPTS;i++) {
                            setpixel(mainScreen, (int)data[2*i+0]-2,
                                                 (int)data[2*i+1]+2, 255, 0, 255);
                            setpixel(mainScreen, (int)data[2*i+0]+2,
                                                 (int)data[2*i+1]-2, 255, 0, 255);
                            printf("%d %d\n",  (int)data[2*i+0], (int)data[2*i+1]);
                            }
                        //kdtree_free_query(kr);
                    }

                    break;
                case SDL_QUIT:
                    exit(0);
			}
		}
        SDL_Flip(mainScreen);
        SDL_Delay(50);
	}

	return 0;
}
