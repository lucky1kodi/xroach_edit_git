/* @(#)roachmap.h	1.3 4/2/91 11:55:35 */
// Compilation: gcc -O2 -o xroach xroach_edit.c -lm -lX11 -lXrender -lXfixes

#include "xbm/fish_right.xbm"
#include "xbm/fish_left.xbm"

#include "xbm/squish.xbm"

#define ROACH_HEADINGS 2    /* number of orientations */
#define ROACH_ANGLE    15    /* angle between orientations */

typedef struct RoachMap {
    char *roachBits;
    Pixmap pixmap;
    int width;
    int height;
} RoachMap;

RoachMap roachPix[] = {
    {fish_right_bits, None, fish_right_height, fish_right_width},
    {fish_left_bits, None, fish_left_height, fish_left_width}
};