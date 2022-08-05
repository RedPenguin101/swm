#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

Atom WMProtocols;
Atom WMDelete;
Atom WMState;
Atom WMTakeFocus;

Cursor cursor;

static int screen;
static int screen_w, screen_h;
static int running = true;
static Display* display;
static Window root;

static int (*xerrorxlib)(Display*, XErrorEvent*);

int window_count = 0;

// style

FILE* logfile;

int xerror(Display* dpy, XErrorEvent* ee) {
  if (ee->error_code == BadWindow ||
      (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch) ||
      (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolyFillRectangle &&
       ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolySegment && ee->error_code == BadDrawable) ||
      (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch) ||
      (ee->request_code == X_GrabButton && ee->error_code == BadAccess) ||
      (ee->request_code == X_GrabKey && ee->error_code == BadAccess) ||
      (ee->request_code == X_CopyArea && ee->error_code == BadDrawable)) {
    fprintf(logfile,
            "non-fatal error: request code=%d, error code=%d, resource=%ld\n",
            ee->request_code, ee->error_code, ee->resourceid);
    return 0;
  }
  fprintf(logfile,
          "fatal error: request code=%d, error code=%d, resource=%ld\n",
          ee->request_code, ee->error_code, ee->resourceid);
  fflush(logfile);
  return xerrorxlib(dpy, ee); /* may call exit */
}

static void setup() {
  logfile = fopen("/home/joe/programming/projects/swm/log_swm.txt", "w");
  fprintf(logfile, "START SESSION LOG\n");

  XSetErrorHandler(xerror);

  screen = DefaultScreen(display);
  screen_w = DisplayWidth(display, screen);
  screen_h = DisplayHeight(display, screen);
  root = RootWindow(display, screen);
  fprintf(logfile, "\t root window %ld\n", root);

  WMProtocols = XInternAtom(display, "WM_PROTOCOLS", False);
  WMDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
  WMState = XInternAtom(display, "WM_STATE", False);
  WMTakeFocus = XInternAtom(display, "WM_TAKE_FOCUS", False);

  XSetWindowAttributes wa;

  cursor = XCreateFontCursor(display, XC_left_ptr);
  wa.cursor = cursor;

  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                  KeyPressMask | ButtonPressMask | PointerMotionMask;
  XSelectInput(display, root, wa.event_mask);
  XSync(display, false);
  XMoveResizeWindow(display, root, 0, 0, screen_w, screen_h);
  XChangeWindowAttributes(display, root, CWEventMask | CWCursor, &wa);
}

void spawn_term() {
  if (fork() == 0) {
    fprintf(logfile, "spawning terminal\n");
    if (display)
      close(ConnectionNumber(display));
    static const char* termcmd[] = {"kitty", NULL};
    setsid();
    execvp(((char**)termcmd)[0], (char**)termcmd);
    exit(EXIT_SUCCESS);
  }
}

static void keypress_handler(XEvent* event) {
  XKeyEvent* ev = &event->xkey;
  KeySym keysym = XKeycodeToKeysym(display, ev->keycode, 0);
  fprintf(logfile, "Keypress \t\t KeySym: %ld ", keysym);
  fprintf(logfile, "State: %d\n", ev->state);
  if (keysym == XK_q && (ev->state == ControlMask || ev->state == Mod4Mask ||
                         ev->state == Mod5Mask))
    running = false;
  else if (keysym == XK_t && (ev->state == ControlMask ||
                              ev->state == Mod4Mask || ev->state == Mod5Mask)) {
    spawn_term();
  }
}

static void mapnotify_handler(XEvent* event) {
  XMapEvent* ev = &event->xmap;
  fprintf(logfile, "MapN \t\t\t\t %ld \t\t event from %ld \n", ev->window,
          ev->event);
}

static void maprequest_handler(XEvent* event) {
  XMapRequestEvent* ev = &event->xmaprequest;
  Window client = ev->window;

  window_count += 1;

  fprintf(logfile, "MapR \t\t\t\t %ld \t\t parent %ld \t window count %d\n",
          ev->window, ev->parent, window_count);

  XWindowAttributes wa;
  XGetWindowAttributes(display, client, &wa);

  XSelectInput(display, client,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                   StructureNotifyMask);

  XGrabKey(display, XKeysymToKeycode(display, XK_q),
           ControlMask | Mod4Mask | Mod5Mask, root, True, GrabModeAsync,
           GrabModeAsync);
  XMoveResizeWindow(display, client, 0, 0, screen_w, screen_h);
  XMapWindow(display, client);
  XSync(display, false);
}

static void createnotify_handler(XEvent* event) {
  XCreateWindowEvent* cwe = &event->xcreatewindow;
  Window w = cwe->window;
  fprintf(logfile, "CreateR \t\t %ld\t\t parent %ld\n", w, cwe->parent);
}

static void destroy_handler(XEvent* event) {
  XDestroyWindowEvent* dwe = &event->xdestroywindow;
  Window w = dwe->window;
  fprintf(logfile, "DestroyR \t\t %ld\t\t event %ld\n", w, dwe->event);
}

static void configurerequest_handler(XEvent* event) {
  XConfigureRequestEvent req = event->xconfigurerequest;
  fprintf(logfile,
          "ConfR \t\t\t %ld \t\t xywhb: %d,%d,%d,%d,%d \t\t above: %ld \t\t "
          "mask: 0x%lx \n",
          req.window, req.x, req.y, req.width, req.height, req.border_width,
          req.above, req.value_mask);

  XWindowAttributes rwa;
  XGetWindowAttributes(display, root, &rwa);

  fprintf(logfile, "\t\t ScreenH: %d ScreenW: %d RootH: %d RootW: %d \n",
          DisplayWidth(display, screen), DisplayHeight(display, screen),
          rwa.width, rwa.height);

  XWindowChanges wc;
  wc.x = req.x;
  wc.y = req.y;
  wc.width = req.width;
  wc.height = req.height;
  XConfigureWindow(display, req.window, req.value_mask, &wc);
}

static void configurenotify_handler(XEvent* event) {
  XConfigureEvent* ev = &event->xconfigure;

  fprintf(logfile,
          "ConfN \t\t\t %ld \t\t xywhb: %d,%d,%d,%d,%d \t\t above: %ld\n",
          ev->window, ev->x, ev->y, ev->width, ev->height, ev->border_width,
          ev->above);
}

static void propertynotify_handler(XEvent* event) {
  XPropertyEvent* ev = &event->xproperty;
  fprintf(logfile, "PropN \t\t\t %ld \t\t %ld \t %d\n", ev->window, ev->atom,
          ev->state);
}

static void focus_handler(XEvent* event) {
  XFocusChangeEvent* ev = &event->xfocus;
  fprintf(logfile, "%s \t\t %ld \t\t %d-%d\n",
          (event->type == FocusIn ? "FocusIn" : "FocusOut"), ev->window,
          ev->mode, ev->detail);
}

static void enter_handler(XEvent* event) {
  XCrossingEvent* ev = &event->xcrossing;
  fprintf(
      logfile,
      "Cross \t\t\t %ld \t\t root: %ld sub: %ld\t\t pos: %d,%d,%d,%d \t md: "
      "%d-%d \t samescreen: %d \t focus: %d \t state 0x%x\n",
      ev->window, ev->root, ev->subwindow, ev->x, ev->y, ev->x_root, ev->y_root,
      ev->mode, ev->detail, ev->same_screen, ev->focus, ev->state);
}

static void run() {
  XEvent event;
  while (running && XNextEvent(display, &event) == 0) {
    switch (event.type) {
      case MotionNotify:
        break;
      case KeyPress:
        keypress_handler(&event);
        break;
      case ConfigureRequest:
        configurerequest_handler(&event);
        break;
      case ConfigureNotify:
        configurenotify_handler(&event);
        break;
      case MapRequest:
        maprequest_handler(&event);
        break;
      case MapNotify:
        mapnotify_handler(&event);
        break;
      case CreateNotify:
        createnotify_handler(&event);
        break;
      case DestroyNotify:
        destroy_handler(&event);
        break;
      case PropertyNotify:
        propertynotify_handler(&event);
        break;
      case FocusIn:
        focus_handler(&event);
        break;
      case FocusOut:
        focus_handler(&event);
        break;
      case EnterNotify:
        enter_handler(&event);
        break;
      default:
        fprintf(logfile, "Unhandled event-type: %d\n", event.type);
    }
    XSync(display, false);
    fflush(logfile);
  }
}

static void cleanup() {
  fprintf(logfile, "END SESSION LOG\n");
  fclose(logfile);
}

int main(int args, char* argv[]) {
  if (!(display = XOpenDisplay(NULL)))
    return 1;
  setup();
  run();
  cleanup();
  XCloseDisplay(display);
  return 0;
}
