#include <stdio.h>
#include "cairoutils.h"

int main() {
    unsigned char img[4];
    int W=1;
    int H=1;

    img[0] = 0;
    img[1] = 0;
    img[2] = 0;
    img[3] = 128;
    cairoutils_stream_png(stdout, img, W, H);

    return 0;
}
