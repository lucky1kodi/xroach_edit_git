// Compilation: gcc -O2 -o xroach xroach_edit.c -lm -lX11 -lXrender -lXfixes and also activate picom with `picom -b` before running the program or it will show only a opaque screen.
// I want to add the squish feature later and also a feature that allows me to catch and drag a roach of my choice to a diffrent location from where it may turn in any random direction and move, I plan to do this reading the mouse movements directly from the kernel or Xserver.

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
int          screen;
Pixel        black;
unsigned int display_height;
unsigned int display_width;
Window       rootWin;

int depth;
Visual* visual;// Visual is a data type, meaning how pixels are tansilated to actuall color
Colormap overlayColormap;//An lookup table

int    done        = 0;

typedef struct Roach
{   RoachMap *rp;
    int      index;
    float    x;
    float    y;
    int      intX;
    int      intY;
    bool      turnLeft;
    int      steps;
	int		goUp;   } Roach;

Roach *roaches;
int   maxRoaches = 10;
int   curRoaches = 0;
float roachSpeed = 10.0;
float turnSpeed  = 6.181;
float probUp     = 8;
float probTurn   = 8;
float mSteps     = 1.6;

//Region rootVisible = NULL;

void Usage();
void SigHandler(int a);
Window FindRootWindow();
int RandInt(int maxVal);
int RoachInRect(Roach *roach, int rx, int ry, int x, int y, unsigned int width, unsigned int height);
int RoachOverRect(Roach *roach, int rx, int ry, int x, int y, unsigned int width, unsigned int height);
void AddRoach();
void TurnRoach(Roach *roach);
void MoveRoach(int rx);
void DrawRoaches();
void CoverRoot();
//int RoachErrors(Display *display, XErrorEvent *err);
Pixel AllocNamedColor(char *colorName, Pixel dfltPix);
//void checkSquish(XButtonEvent *buttonEvent);

#define USEPRT(msg) fprintf(stderr, msg)

void Usage()//Modfy this later.
{
    USEPRT("Usage: xroach [options]\n\n");//
    USEPRT("Options:\n");//
    USEPRT("        -display displayname\n");//
    USEPRT("        -rc      roachcolor\n");//
    USEPRT("        -roaches numroaches\n");//
    USEPRT("        -speed   roachspeed\n");//
	USEPRT("		-up probability_changing_height\n");
	USEPRT("		-turn probability_to_turn\n");
	USEPRT("		-step max_numbers_of_steps");
    USEPRT("./xroach -roaches 7 -speed 4 -rc black -Up 6 -Turn 8\n");//
	USEPRT("");
    exit(1);
}

void SigHandler(int a)
{
        done = 1;
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
        r->steps = RandInt((int) (mSteps*turnSpeed)); r->steps=r->steps>turnSpeed?r->steps:turnSpeed;
        r->turnLeft = RandInt(100) >= 50;
		r->goUp=0;
    }
}

//   Turn a roach.
void TurnRoach(Roach *roach)
{
    if (roach->index != (roach->rp - roachPix))
        return;

    if (roach->turnLeft)
    {
	roach->index=1;
    }
    else
    {
	roach->index=0;
    }
}

//   Move a roach.
void MoveRoach(int rx)
{
    float newX;
    float newY;
    Roach *roach;

    roach = &roaches[rx];
	float px=roach->x;
	float py=roach->y;
	newX = px + (roachSpeed * (roach->turnLeft?-1:1));	
	newY = py + (float)roach->goUp * ((float)turnSpeed);

    if (RoachInRect(roach,
                    (int) newX, (int) newY,
                    0, 0,
                    display_width, display_height))
    {
        roach->x = newX;
        roach->y = newY;

        if (roach->steps-- < 0)
        {
			int t=roach->turnLeft;
			//int g=roach->goUp;
			roach->goUp=0;
			if(RandInt(10)>probUp) roach->goUp=RandInt(3)-1;//control going up and down probability.
            TurnRoach(roach);
            roach->steps = RandInt((int)turnSpeed);
            if(RandInt(10)>probTurn) roach->turnLeft = RandInt(2);//Probability of changing direction.
			if(t!=roach->turnLeft)
					roach->x=px, roach->y=py;
			//THIS WILL BE THE CAUSE FOR A WEIRD EFFECT, SO FUCK THIS LINE, IDK HOW THEY EVEN DISSAPERE: if(t!=roach->turnLeft) roach->x=px, roach->y=px; or just roach->y=px;
        }
    }
    else
    {
		roach->turnLeft=!roach->turnLeft;
		roach->goUp=0;
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
}
void ClearOverlay(void)
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
    char                 *roachColor = "black";
    RoachMap             *rp;
    XGCValues            xgcv;//Althoug only used as garbage, its fine the '0L' makes it unconsidered for.
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
		else if (strcmp(arg, "-up") == 0)
			probUp=(int) strtol(av[++ax], (char**) NULL, 0);
		else if (strcmp(arg, "-turn") == 0)
			probTurn=(int) strtol(av[++ax], (char**) NULL, 0);
		else if (strcmp(arg, "-step") == 0)
			{mSteps=(int) strtol(av[++ax], (char**) NULL, 0); if(mSteps<1) mSteps=1;}
        else
            Usage();}
    turnSpeed = roachSpeed / 1.618 ;//Golden Ratio
    if (turnSpeed < 1)
        turnSpeed = 1;
    srand((unsigned int) time((time_t *) NULL));

    //Catch some signals so we can erase any visible roaches.
    signal(SIGKILL, SigHandler);
    signal(SIGINT, SigHandler);
    signal(SIGTERM, SigHandler);
    signal(SIGHUP, SigHandler);


//  Display.
    display = XOpenDisplay(display_name);
    if (display == NULL){
        if (display_name == NULL) display_name = getenv("DISPLAY");
        fprintf(stderr,"%s: cannot connect to X server %s\n",av[0],display_name ? display_name : "(default)");
        exit(1);}
    screen  = DefaultScreen(display);
    display_width  = (unsigned int) DisplayWidth(display, screen);
    display_height = (unsigned int) DisplayHeight(display, screen);

// The window to draw the roaches on is rootWin, didnt change the name cause it'll be a headache.
    Window realRoot = RootWindow(display, screen);
    XserverRegion emptyRegion = XFixesCreateRegion(display, NULL, 0);
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
    xswa.override_redirect = True;
    xswa.background_pixel = 0;
    xswa.border_pixel = 0;
    xswa.colormap = /*DefaultColormap(display, screen);*/overlayColormap;// U need to check this line for transparancy.
    xswa.event_mask = ExposureMask;
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

	//Making pixmaps
	for(int ax=0; ax<ROACH_HEADINGS; ax++)
	{	
		int rx=ax; //Yeah i know, a lot of useless shit, but this keeps this closer to original program.
		rp = &roachPix[rx];
        rp->pixmap = XCreateBitmapFromData(display,
                                           rootWin,
                                           rp->roachBits,
                                           (unsigned int) rp->width,
                                           (unsigned int) rp->height);
	}

    roaches = (Roach *) malloc(sizeof(Roach) * maxRoaches);

    gc = XCreateGC(display, rootWin, 0L, &xgcv);
    XSetForeground(display, gc, AllocNamedColor(roachColor, black));
    XSetFillStyle(display, gc, FillStippled);

    while (curRoaches < maxRoaches) AddRoach();

// Engin.
while (!done)
{
    ClearOverlay();
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
