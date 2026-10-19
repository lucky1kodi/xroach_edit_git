// Compilation: gcc -O2 -o xroach xroach_edit.c -lm -lX11 -lXrender -lXfixes and also activate picom with `picom -b` before running the program or it will show only a opaque screen.
// I want to add the squish feature later and also a feature that allows me to catch and drag a roach of my choice to a diffrent location from where it may turn in any random direction and move, I plan to do this reading the mouse movements directly from the kernel or Xserver.

// Although this is a bit more processor friendly, the optimizations that are performed behind the screens in picom makes that obselete.
// I made DrawRoach() manage clearing old and drawing new roachs to improve performance by only clearing parts of screen that need clearing unlike the previous program that clears the entire screen.

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <X11/Xutil.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include "roachmap.h"

typedef unsigned long Pixel;
#define SCAMPER_EVENT (LASTEvent + 1)

char         *display_name = NULL;
Display      *display;
GC           gc;// GC is a X11 data type, used for coloring the roach while unsquashed.
//GC           gutsGC;// this GC is used for the squish color.
int          screen;
Pixel        black;
unsigned int display_height;
unsigned int display_width;
Window       rootWin;

int depth;
Visual* visual;
Colormap overlayColormap;



//Bool   squishRoach = False;
//Bool   squishWinUp = False;
int    done        = 0;
//int    errorVal    = 0;
//int    eventBlock  = 0;
//Pixmap squishMap;

typedef struct Roach
{   RoachMap *rp;
    int      index;
    float    x;
    float    y;
    int      intX;
    int      intY;
    int      hidden;
    int      turnLeft;
    int      steps;   } Roach;

Roach *roaches;
int   maxRoaches = 10;
int   curRoaches = 0;
float roachSpeed = 20.0;
float turnSpeed  = 10.0;

//Region rootVisible = NULL;

#define USEPRT(msg) fprintf(stderr, msg)

void Usage()
{
    USEPRT("Usage: xroach [options]\n\n");//
    USEPRT("Options:\n");//
    USEPRT("       -display displayname\n");//
    USEPRT("       -rc      roachcolor\n");//
    USEPRT("       -roaches numroaches\n");//
    USEPRT("       -speed   roachspeed\n");//
//    USEPRT("       -squish\n");//
//    USEPRT("       -rgc     roachgutscolor\n");//
    USEPRT("./xroach -roaches 70 -speed 4 -squish -rc black -rgc yellowgreen\n");//
    exit(1);
}

int RandInt(int maxVal){ return rand() % maxVal;}

//   Check for roach completely in specified rectangle.
int RoachInRect(Roach *roach, int rx, int ry, int x, int y, unsigned int width, unsigned int height)
{
    if (rx < x)
        return 0;

    if ((rx + roach->rp->width) > (x + width))
        return 0;

    if (ry < y)
        return 0;

    if ((ry + roach->rp->height) > (y + height))
        return 0;

    return 1;
}

//   Check for roach overlapping specified rectangle. for moving when movning window and to squish.
int RoachOverRect(Roach *roach, int rx, int ry, int x, int y, unsigned int width, unsigned int height)
{
    if (rx >= (x + width))
        return 0;

    if ((rx + roach->rp->width) <= x)
        return 0;

    if (ry >= (y + height))
        return 0;

    if ((ry + roach->rp->height) <= y)
        return 0;

    return 1;
}

//   Give birth to a roach.
void AddRoach()
{
    Roach *r;

    if (curRoaches < maxRoaches)
    {
        r = &roaches[curRoaches++];
        r->index = RandInt(ROACH_HEADINGS);
        r->rp = &roachPix[r->index];
        r->x = RandInt(display_width - r->rp->width);
        r->y = RandInt(display_height - r->rp->height);
        r->intX = -1;
        r->intY = -1;
        r->hidden = 0;
        r->steps = RandInt((int) turnSpeed);
        r->turnLeft = RandInt(100) >= 50;
    }
}

//   Turn a roach.
void TurnRoach(Roach *roach)
{
    if (roach->index != (roach->rp - roachPix))
        return;

    if (roach->turnLeft)
    {
        roach->index += (RandInt(30) / 10) + 1;

        if (roach->index >= ROACH_HEADINGS)
            roach->index -= ROACH_HEADINGS;
    }
    else
    {
        roach->index -= (RandInt(30) / 10) + 1;

        if (roach->index < 0)
            roach->index += ROACH_HEADINGS;
    }
}

//   Move a roach.
void MoveRoach(int rx)
{
    float newX;
    float newY;
    Roach *roach;

    roach = &roaches[rx];
    newX = roach->x + (roachSpeed * roach->rp->cosine);
    newY = roach->y - (roachSpeed * roach->rp->sine);

    if (RoachInRect(roach,
                    (int) newX, (int) newY,
                    0, 0,
                    display_width, display_height))
    {
        roach->x = newX;
        roach->y = newY;

        if (roach->steps-- <= 0)
        {
            TurnRoach(roach);
            roach->steps = RandInt((int) turnSpeed);

            /*Previously, roaches would just go around in circles.
               This makes their movement more interesting (and disgusting too!).*/
            if (RandInt(100) >= 80)
                roach->turnLeft ^= 1;
        }
    }
    else
    {
        TurnRoach(roach);
    }
}


//   Draw all roaches.
void DrawRoaches()
{
    Roach *roach;

    for (int rx = 0; rx < curRoaches; rx++)
    {
        roach = &roaches[rx];
        if (!roach->hidden)
        {
            XClearArea(display,
                       rootWin,
                       roach->intX,
                       roach->intY,
                       (unsigned int) roach->rp->width,
                       (unsigned int) roach->rp->height,
                       False);
            roach->intX = (int) roach->x;
            roach->intY = (int) roach->y;
            roach->rp = &roachPix[roach->index];

            XSetStipple(display, gc, roach->rp->pixmap);
            XSetTSOrigin(display, gc, roach->intX, roach->intY);
            XFillRectangle(display,
                           rootWin,
                           gc,
                           roach->intX,
                           roach->intY,
                           (unsigned int) roach->rp->width,
                           (unsigned int) roach->rp->height);
        }
        else
        {
            roach->intX = -1;
        }
    }
}
/*void ClearOverlay(void)
{
    static Picture overlayPicture = None;
    static XRenderPictFormat *format = NULL;
    XRenderColor transparent = { 0, 0, 0, 0 };

    if (overlayPicture == None) {
        format = XRenderFindVisualFormat(display,
                                         visual);// Only if rootWin is created with ARGB-visual capabilities.
        overlayPicture = XRenderCreatePicture(display,
                                              rootWin,
                                              format,
                                              0,
                                              NULL);
    }

    XRenderFillRectangle(display,
                         PictOpSrc,
                         overlayPicture,
                         &transparent,
                         0, 0,
                         display_width,
                         display_height);
}
*/
//   Allocate a color by name.
Pixel AllocNamedColor(char *colorName, Pixel dfltPix)
{
    Pixel  pix;
    XColor exactcolor;
    XColor scrncolor;

    if (XAllocNamedColor(display,
                         overlayColormap /*DefaultColormap(display, screen)*/,// This is ocmmented out cause the program runs as if nothing happening.
                         colorName,
                         &scrncolor,
                         &exactcolor))
        pix = scrncolor.pixel;
    else
        pix = dfltPix;

    return pix;
}

int main(int ac, char *av[])
{
    char                 *arg;
    char                 *gutsColor  = NULL;
    char                 *roachColor = "black";
    float                angle;
    int                  needCalc;
    int                  nVis;
    RoachMap             *rp;
    Window               squishWin;
//    XEvent               ev;//Cant have this as I'll be running inide of i3. I need to get data from /etc/input/whater_the_mouse_input_is.
    XGCValues            xgcv;
    XSetWindowAttributes xswa;

//  Formalities.
    for (int ax = 1; ax < ac; ax++)
    {   arg = av[ax];

        if (strcmp(arg, "-display") == 0)
            display_name = av[++ax];
        else if (strcmp(arg, "-rc") == 0)
            roachColor = av[++ax];
        else if (strcmp(arg, "-speed") == 0)
            roachSpeed = (float) strtod(av[++ax], (char **) NULL);
        else if (strcmp(arg, "-roaches") == 0)
            maxRoaches = (int) strtol(av[++ax], (char **) NULL, 0);
       /* else if (strcmp(arg, "-squish") == 0)
            squishRoach = True;
        else if (strcmp(arg, "-rgc") == 0)
            gutsColor = av[++ax]; */
        else
            Usage();}
    turnSpeed = 200 / roachSpeed;
    if (turnSpeed < 1)
        turnSpeed = 1;
    srand((unsigned int) time((time_t *) NULL));
//  Display.
    display = XOpenDisplay(display_name);
    if (display == NULL){
        if (display_name == NULL) display_name = getenv("DISPLAY");
        fprintf(stderr,"%s: cannot connect to X server %s\n",av[0],display_name ? display_name : "(default)");
        exit(1);}
    screen  = DefaultScreen(display);
    //rootWin = FindRootWindow();// I need to run a overlay window not a root window.
//    black   = /*BlackPixel(display, screen)*/ AllocNamedColor("black", 0);
    display_width  = (unsigned int) DisplayWidth(display, screen);
    display_height = (unsigned int) DisplayHeight(display, screen);
// The window to draw the roaches on is rootWin, didnt change the name cause it'll be a headache.
    Window realRoot = RootWindow(display, screen);
    XserverRegion emptyRegion = XFixesCreateRegion(display, NULL, 0);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
XVisualInfo tpl;
XVisualInfo *infos;
XRenderPictFormat *fmt;
int ninfo;
int found = 0;

tpl.screen = screen;
tpl.depth = 32;
tpl.class = TrueColor;

infos = XGetVisualInfo(display,
                       VisualScreenMask | VisualDepthMask | VisualClassMask,
                       &tpl,
                       &ninfo);
for (int i = 0; i < ninfo; i++)
{
    fmt = XRenderFindVisualFormat(display, infos[i].visual);
    if (fmt && fmt->type == PictTypeDirect && fmt->direct.alphaMask)
    {
        visual = infos[i].visual;
        depth = infos[i].depth;
        found = 1;
        break;
    }
}

overlayColormap = XCreateColormap(display, realRoot, visual, AllocNone);
    black   = BlackPixel(display, screen) /*AllocNamedColor("black", 0)*/;// The commented out portion is safer to use but commented out to test the limits of brattyness allowed.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    xswa.override_redirect = True;
    xswa.background_pixel = 0;
    xswa.border_pixel = 0;
    xswa.colormap = /*DefaultColormap(display, screen);*/overlayColormap;// U need to chech this line for transparancy.
    xswa.event_mask = ExposureMask;
//    depth = DefaultDepth(display, screen);// U need to chech this line for transparancy.
//    visual = DefaultVisual(display, screen);// U need to chech this line for transparancy.
    rootWin = XCreateWindow(display,
                            realRoot,
                            0, 0,
                            display_width, display_height,
                            0,
                            depth,
                            InputOutput,
                            visual,
                            CWOverrideRedirect | CWBackPixel | CWBorderPixel |
                            CWColormap | CWEventMask,
                            &xswa);
    XFixesSetWindowShapeRegion(display, rootWin, ShapeInput, 0, 0, emptyRegion);
    XFixesDestroyRegion(display, emptyRegion);
    XMapRaised(display, rootWin);// Maybe i need to do this evertime but idk Perplexity says once is enough.

//   Create roach pixmaps at several orientations.
    for (int ax = 0; ax < 360; ax += ROACH_ANGLE)
    {
        int rx = ax / ROACH_ANGLE;
        angle = (float) (rx * 0.261799387799);
        rp = &roachPix[rx];
        rp->pixmap = XCreateBitmapFromData(display,
                                           rootWin,
                                           rp->roachBits,
                                           (unsigned int) rp->width,
                                           (unsigned int) rp->height);
        rp->sine = (float) sin(angle);
        rp->cosine = (float) cos(angle);
    }

/*	//  Create the squished pixmap
    if (squishRoach)
        squishMap = XCreateBitmapFromData(display,
                                          rootWin,
                                          squish_bits,
                                          squish_width,
                                          squish_height);	*/

    roaches = (Roach *) malloc(sizeof(Roach) * maxRoaches);

    gc = XCreateGC(display, rootWin, 0L, &xgcv);
    XSetForeground(display, gc, AllocNamedColor(roachColor, black));
    XSetFillStyle(display, gc, FillStippled);

/*    if (squishRoach && gutsColor != NULL)
    {
        gutsGC = XCreateGC(display, rootWin, 0L, &xgcv);
        XSetForeground(display, gutsGC, AllocNamedColor(gutsColor, black));
        XSetFillStyle(display, gutsGC, FillStippled);
    }
    else
    {
        gutsGC = gc;
    }	*/
    while (curRoaches < maxRoaches) AddRoach();
/*    if (squishRoach){
        xswa.event_mask = ButtonPressMask;
        xswa.override_redirect = True;
        squishWin = XCreateWindow(display,
                                  rootWin,
                                  0, 0,
                                  display_width, display_height,
                                  0,
                                  CopyFromParent, InputOnly, CopyFromParent,
                                  CWOverrideRedirect | CWEventMask,
                                  &xswa);
        XLowerWindow(display, squishWin);}	*/
// This is the game engin, if u ever want to add squish feature, this is going to be the real trouble-u need to get the engin from the original code and edit it.
while (!done)
{
//    ClearOverlay();

    for (int rx = 0; rx < curRoaches; rx++)
        MoveRoach(rx);

    DrawRoaches();
    XFlush(display);
    usleep(20000);
}

// Cleanup
    XFreeGC(display, gc);
    XDestroyWindow(display, rootWin);
    XFreeColormap(display, overlayColormap);
    XCloseDisplay(display);
    free(roaches);
    return 0;
}
