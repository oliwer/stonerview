/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)
   http://www.eblong.com/zarf/stonerview.html
   This program is distributed under the GPL.
   See main.c, the Copying document, or the above URL for details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include "general.h"
#include "osc.h"
#include "view.h"
#include "vroot.h"

#include "move.h"

static char *progclass = "StonerView";
static char *progname = NULL;

static GLfloat view_rotx = -45.0, view_roty = 0.0, view_rotz = 0.0;
static GLfloat view_scale = 4.0;

static void setup_window(void);

static void win_reshape(int width, int height);
static void handle_events(void);

static Display *dpy;
static Window window;
static int wireframe = FALSE;
static int addedges = FALSE;

static Atom XA_WM_PROTOCOLS, XA_WM_DELETE_WINDOW;


static void usage(void)
{
  fprintf(stderr,
    "usage: %s [--geom =WxH+X+Y | --fullscreen | --root] [--wire]\n",
    progname);
  exit(1);
}


static int visual_depth(Display *dpy, int screen, Visual *visual)
{
  XVisualInfo vi_in, *vi_out;
  int out_count, depth;

  vi_in.screen = screen;
  vi_in.visualid = XVisualIDFromVisual(visual);

  vi_out = XGetVisualInfo(dpy, (VisualScreenMask | VisualIDMask),
    &vi_in, &out_count);
  if (!vi_out)
    return -1;

  depth = vi_out[0].depth;
  XFree(vi_out);

  return depth;
}

#define UNDEF (-65536)

int init_view(int *argc, char *argv[])
{
  int ix;
  int fullscreen = FALSE;
  int on_root = FALSE;

  int x = UNDEF, y = UNDEF;
  int depth = -1;
  int w = 400, h = 400;
  char *dpystr = (char *)getenv("DISPLAY");
  char *geom = NULL;
  int screen;
  Visual *visual = NULL;
  XWindowAttributes xgwa;
  XSetWindowAttributes xswa;
  unsigned long xswa_mask = 0;
  XSizeHints hints;
  GLXContext glx_context = 0;

  memset(&hints, 0, sizeof(hints));

  progname = argv[0];

  for (ix=1; ix<*argc; ix++) {
    if (argv[ix][0] == '-' && argv[ix][1] == '-')
      argv[ix]++;
    if (!strcmp(argv[ix], "-geometry") ||
      !strcmp(argv[ix], "-geom")) {
      if (on_root || fullscreen) usage();
      geom = argv[++ix];
    }
    else if (!strcmp(argv[ix], "-display") ||
      !strcmp(argv[ix], "-disp") ||
      !strcmp(argv[ix], "-dpy"))
      dpystr = argv[++ix];
    else if (!strcmp(argv[ix], "-root")) {
      if (geom || fullscreen) usage();
      on_root = TRUE;
    }
    else if (!strcmp(argv[ix], "-fullscreen") ||
      !strcmp(argv[ix], "-full")) {
      if (on_root || geom) usage();
      fullscreen = TRUE;
    }
    else if (!strcmp(argv[ix], "-wireframe") ||
      !strcmp(argv[ix], "-wire")) {
      wireframe = TRUE;
    }
    else if (!strcmp(argv[ix], "-edges") ||
      !strcmp(argv[ix], "-edge")) {
      addedges = TRUE;
    }
    else {
      usage();
    }
  }

  dpy = XOpenDisplay (dpystr);
  if (!dpy) {
    fprintf(stderr, "%s: unable to open display %s\n",
      progname, dpystr);
    return FALSE;
  }

  screen = DefaultScreen (dpy);

  XA_WM_PROTOCOLS = XInternAtom (dpy, "WM_PROTOCOLS", False);
  XA_WM_DELETE_WINDOW = XInternAtom (dpy, "WM_DELETE_WINDOW", False);

  if (on_root) {
    window = RootWindow (dpy, screen);
    XGetWindowAttributes (dpy, window, &xgwa);
    visual = xgwa.visual;
    w = xgwa.width;
    h = xgwa.height;
  }
  else {
    int ww = WidthOfScreen (DefaultScreenOfDisplay (dpy));
    int hh = HeightOfScreen (DefaultScreenOfDisplay (dpy));

    if (fullscreen) {
      w = ww;
      h = hh;
    }
    else if (geom) {
      char c;
      if      (4 == sscanf (geom, "=%dx%d+%d+%d%c", &w, &h, &x, &y, &c))
	;
      else if (4 == sscanf (geom, "=%dx%d-%d+%d%c", &w, &h, &x, &y, &c))
	x = ww-w-x;
      else if (4 == sscanf (geom, "=%dx%d+%d-%d%c", &w, &h, &x, &y, &c))
	y = hh-h-y;
      else if (4 == sscanf (geom, "=%dx%d-%d-%d%c", &w, &h, &x, &y, &c))
	x = ww-w-x, y = hh-h-y;
      else if (2 == sscanf (geom, "=%dx%d%c", &w, &h, &c))
	;
      else if (2 == sscanf (geom, "+%d+%d%c", &x, &y, &c))
	;
      else if (2 == sscanf (geom, "-%d+%d%c", &x, &y, &c))
	x = ww-w-x;
      else if (2 == sscanf (geom, "+%d-%d%c", &x, &y, &c))
	y = hh-h-y;
      else if (2 == sscanf (geom, "-%d-%d%c", &x, &y, &c))
	x = ww-w-x, y = hh-h-y;
      else {
	fprintf(stderr, "%s: unparsable geometry: %s\n",
	  progname, geom);
	return FALSE;
      }

      hints.flags = USSize;
      hints.width = w;
      hints.height = h;
      if (x != UNDEF && y != UNDEF) {
	hints.flags |= USPosition;
	hints.x = x;
	hints.y = y;
      }
    }

    /* Pick a good GL visual */
    {
#define R GLX_RED_SIZE
#define G GLX_GREEN_SIZE
#define B GLX_BLUE_SIZE
#define D GLX_DEPTH_SIZE
#define I GLX_BUFFER_SIZE
#define DB GLX_DOUBLEBUFFER

      int attrs[][20] = {
	{ GLX_RGBA, R, 8, G, 8, B, 8, D, 8, DB, 0 }, /* rgb double */
	{ GLX_RGBA, R, 4, G, 4, B, 4, D, 4, DB, 0 },
	{ GLX_RGBA, R, 2, G, 2, B, 2, D, 2, DB, 0 },
	{ GLX_RGBA, R, 8, G, 8, B, 8, D, 8,     0 }, /* rgb single */
	{ GLX_RGBA, R, 4, G, 4, B, 4, D, 4,     0 },
	{ GLX_RGBA, R, 2, G, 2, B, 2, D, 2,     0 },
	{ I, 8,                       D, 8, DB, 0 }, /* cmap double */
	{ I, 4,                       D, 4, DB, 0 },
	{ I, 8,                       D, 8,     0 }, /* cmap single */
	{ I, 4,                       D, 4,     0 },
	{ GLX_RGBA, R, 1, G, 1, B, 1, D, 1,     0 }  /* monochrome */
      };
      int i;

      for (i = 0; i < sizeof(attrs)/sizeof(*attrs); i++) {
	XVisualInfo *vi = glXChooseVisual (dpy, screen, attrs[i]);
	if (vi) {
	  visual = vi->visual;
	  XFree (vi);
	  break;
	}
      }
      if (!visual) {
	fprintf (stderr, "%s: unable to find a GL visual\n", progname);
	return FALSE;
      }
    }

    if (x == UNDEF) x = 0;
    if (y == UNDEF) y = 0;

    xswa_mask = (CWEventMask | CWColormap |
      CWBackPixel | CWBackingPixel | CWBorderPixel );
    xswa.colormap = XCreateColormap (dpy, RootWindow (dpy, screen),
      visual, AllocNone);
    xswa.background_pixel = BlackPixel (dpy, screen);
    xswa.backing_pixel = xswa.background_pixel;
    xswa.border_pixel = xswa.background_pixel;
    xswa.event_mask = (KeyPressMask | ButtonPressMask | StructureNotifyMask);

    depth = visual_depth (dpy, screen, visual);
    if (depth < 0)
      return FALSE;

    window = XCreateWindow(dpy, RootWindow(dpy, screen),
      x, y, w, h, 0,
      depth,
      InputOutput, visual,
      xswa_mask, &xswa);

    {
      XTextProperty tp;
      XStringListToTextProperty (&progclass, 1, &tp);
      XSetWMProperties (dpy, window, &tp, &tp, argv, *argc, &hints, 0, 0);
    }

    XChangeProperty (dpy, window, XA_WM_PROTOCOLS, XA_ATOM, 32,
      PropModeReplace,
      (unsigned char *)&XA_WM_DELETE_WINDOW, 1);

    XMapRaised (dpy, window);
    XSync (dpy, False);
  }


  /* Now hook up to GLX */
  {
    XVisualInfo vi_in, *vi_out;
    int out_count;

    vi_in.screen = screen;
    vi_in.visualid = XVisualIDFromVisual (visual);
    vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
      &vi_in, &out_count);
    if (!vi_out)
      return FALSE;

    glx_context = glXCreateContext (dpy, vi_out, 0, GL_TRUE);
    XFree(vi_out);

    if (!glx_context) {
      fprintf(stderr, "%s: couldn't create GL context for root window.\n",
	progname);
      return FALSE;
    }

    glXMakeCurrent (dpy, window, glx_context);
  }

  setup_window();
  win_reshape(w, h);

  return TRUE;
}

static void setup_window()
{
  glEnable(GL_CULL_FACE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);

  glEnable(GL_NORMALIZE);
}

/* callback: draw everything */
void win_draw(void)
{
  int ix;

  static GLfloat white[] = { 1.0, 1.0, 1.0, 1.0 };
  static GLfloat grey[] =  { 0.6, 0.6, 0.6, 1.0 };

  glDrawBuffer(GL_BACK);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  glScalef(view_scale, view_scale, view_scale);
  glRotatef(view_rotx, 1.0, 0.0, 0.0);
  glRotatef(view_roty, 0.0, 1.0, 0.0);
  glRotatef(view_rotz, 0.0, 0.0, 1.0);

  glShadeModel(GL_FLAT);

  for (ix=0; ix<NUM_ELS; ix++) {
    elem_t *el = &elist[ix];

    glNormal3f(0.0, 0.0, 1.0);

    if (addedges || wireframe) {

      glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE,
	(wireframe ? white : grey));
      glBegin(GL_LINE_LOOP);

      glVertex3f(el->pos[0] - el->vervec[0], el->pos[1] - el->vervec[1],
	el->pos[2]);
      glVertex3f(el->pos[0] + el->vervec[1], el->pos[1] - el->vervec[0],
	el->pos[2]);
      glVertex3f(el->pos[0] + el->vervec[0], el->pos[1] + el->vervec[1],
	el->pos[2]);
      glVertex3f(el->pos[0] - el->vervec[1], el->pos[1] + el->vervec[0],
	el->pos[2]);

      glEnd();
    }

    if (!wireframe) {

      glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE,
	el->col);
      glBegin(GL_QUADS);

      glVertex3f(el->pos[0] - el->vervec[0], el->pos[1] - el->vervec[1],
	el->pos[2]);
      glVertex3f(el->pos[0] + el->vervec[1], el->pos[1] - el->vervec[0],
	el->pos[2]);
      glVertex3f(el->pos[0] + el->vervec[0], el->pos[1] + el->vervec[1],
	el->pos[2]);
      glVertex3f(el->pos[0] - el->vervec[1], el->pos[1] + el->vervec[0],
	el->pos[2]);

      glEnd();
    }
  }

  glPopMatrix();

  glFinish();
  glXSwapBuffers(dpy, window);

  handle_events();

}

/* callback: new window size or exposure */
static void win_reshape(int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40.0);
}


static void handle_events(void)
{
  while (XPending(dpy)) {
    XEvent evstruct;
    XEvent *event = &evstruct;
    XNextEvent (dpy, event);
    switch (event->xany.type) {
    case ConfigureNotify:
      {
	XWindowAttributes xgwa;
	XGetWindowAttributes (dpy, window, &xgwa);
	win_reshape (xgwa.width, xgwa.height);
	break;
      }
    case KeyPress:
      {
	KeySym keysym;
	char c = 0;
	XLookupString (&event->xkey, &c, 1, &keysym, 0);
	if (c == 'q' ||
	  c == 'Q' ||
	  c == 3 ||	/* ^C */
	  c == 27)	/* ESC */
	  exit (0);
	else if (! (keysym >= XK_Shift_L && keysym <= XK_Hyper_R))
	  XBell (dpy, 0);  /* beep for non-chord keys */
      }
      break;
    case ButtonPress:
      XBell (dpy, 0);
      break;
    case ClientMessage:
      {
	if (event->xclient.message_type != XA_WM_PROTOCOLS) {
	  char *s = XGetAtomName(dpy, event->xclient.message_type);
	  if (!s) s = "(null)";
	  fprintf (stderr, "%s: unknown ClientMessage %s received!\n",
	    progname, s);
	}
	else if (event->xclient.data.l[0] != XA_WM_DELETE_WINDOW) {
	  char *s1 = XGetAtomName(dpy, event->xclient.message_type);
	  char *s2 = XGetAtomName(dpy, event->xclient.data.l[0]);
	  if (!s1) s1 = "(null)";
	  if (!s2) s2 = "(null)";
	  fprintf (stderr,"%s: unknown ClientMessage %s[%s] received!\n",
	    progname, s1, s2);
	}
	else {
	  exit (0);
	}
      }
      break;
    }
  }
}
