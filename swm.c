#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

static int screen;
static int screen_w, screen_h;
static int running = true;
static Display* display;
static Window root;

// style

FILE* logfile;

static void setup() {
  logfile = fopen("/home/joe/programming/projects/swm/log_swm.txt", "w");
  fprintf(logfile, "START SESSION LOG\n");

  screen = DefaultScreen(display);
  screen_w = DisplayWidth(display, screen);
  screen_h = DisplayHeight(display, screen);
  root = RootWindow(display, screen);
  fprintf(logfile, "\t root window %ld\n", root);

  XSetWindowAttributes wa;
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
  fprintf(logfile, "KeySym: %ld ", keysym);
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
  fprintf(logfile, "\t window: %ld\n", ev->window);
  fprintf(logfile, "\t event: %ld\n", ev->event);
  fprintf(logfile, "\t send event?: %d\n", ev->send_event);
  fprintf(logfile, "\t overide redirect?: %d\n", ev->override_redirect);
}

static void maprequest_handler(XEvent* event) {
  XMapRequestEvent* ev = &event->xmaprequest;
  Window client = ev->window;
  fprintf(logfile, "\t maprequest for client %ld\n", client);
  fprintf(logfile, "\t parent %ld\n", ev->parent);

  if (client == root) {
    fprintf(logfile, "\t request is root, ignoring\n");
    return;
  }

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
  fprintf(logfile, "\t client requesting %ld\n", w);
  fprintf(logfile, "\t parent %ld\n", cwe->parent);
}

static void destroy_handler(XEvent* event) {
  XDestroyWindowEvent* dwe = &event->xdestroywindow;
  Window w = dwe->window;
  fprintf(logfile, "\t client requesting %ld\n", w);
  fprintf(logfile, "\t event %ld\n", dwe->event);
  if (w == root) {
    fprintf(logfile, "\t request is root, ignoring\n");
    return;
  }

  fprintf(logfile, "\t client destroyed %ld\n", w);
}

static void configurerequest_handler(XEvent* event) {
  XConfigureRequestEvent req = event->xconfigurerequest;
  fprintf(logfile, "\t parent: %ld\n", req.parent);
  fprintf(logfile, "\t window: %ld\n", req.window);
  fprintf(logfile, "\t xywhb: %d,%d,%d,%d,%d\n", req.x, req.y, req.width,
          req.height, req.border_width);
  fprintf(logfile, "\t above: %ld\n", req.above);
  fprintf(logfile, "\t val mask: %lx\n", req.value_mask);

  XWindowChanges wc;
  wc.x = req.x;
  wc.y = req.y;
  wc.width = req.width;
  wc.height = req.height;
  XConfigureWindow(display, req.window, req.value_mask, &wc);
}

static void configurenotify_handler(XEvent* event) {
  XConfigureEvent* ev = &event->xconfigure;
  fprintf(logfile, "\t event: %ld\n", ev->event);
  fprintf(logfile, "\t window: %ld\n", ev->window);
  fprintf(logfile, "\t xywhb: %d,%d,%d,%d,%d\n", ev->x, ev->y, ev->width,
          ev->height, ev->border_width);
  fprintf(logfile, "\t above: %ld\n", ev->above);
  fprintf(logfile, "\t overide: %d\n", ev->override_redirect);
}

static void propertynotify_handler(XEvent* event) {
  XPropertyEvent* ev = &event->xproperty;
  fprintf(logfile, "\t atom: %ld\n", ev->atom);
  fprintf(logfile, "\t state: %d\n", ev->state);
  fprintf(logfile, "\t window: %ld\n", ev->window);
}

static void focusin_handler(XEvent* event) {
  XFocusChangeEvent* ev = &event->xfocus;
  fprintf(logfile, "\t type: %d\n", ev->type);
  fprintf(logfile, "\t window: %ld\n", ev->window);
  fprintf(logfile, "\t mode: %d\n", ev->mode);
  fprintf(logfile, "\t detail: %d\n", ev->detail);
}

static void run() {
  XEvent event;
  while (running && XNextEvent(display, &event) == 0) {
    fflush(logfile);
    switch (event.type) {
      case MotionNotify:
        break;
      case KeyPress:
        fprintf(logfile, "Received keypress event ");
        keypress_handler(&event);
        break;
      case ConfigureRequest:
        fprintf(logfile, "Received configure request event\n");
        configurerequest_handler(&event);
        break;
      case ConfigureNotify:
        fprintf(logfile, "Received configure notify event\n");
        configurenotify_handler(&event);
        break;
      case MapRequest:
        fprintf(logfile, "Received maprequest event\n");
        maprequest_handler(&event);
        break;
      case MapNotify:
        fprintf(logfile, "Received map notify event\n");
        mapnotify_handler(&event);
        break;
      case CreateNotify:
        fprintf(logfile, "Received CreateNotify event\n");
        createnotify_handler(&event);
        break;
      case DestroyNotify:
        fprintf(logfile, "Received DestroyNotify event\n");
        destroy_handler(&event);
        break;
      case PropertyNotify:
        fprintf(logfile, "Received PropertyNotify event\n");
        propertynotify_handler(&event);
        break;
      case FocusIn:
        fprintf(logfile, "Received FocusIn event\n");
        focusin_handler(&event);
        break;
      case FocusOut:
        fprintf(logfile, "Received FocusOut event\n");
        focusin_handler(&event);
        break;
      default:
        fprintf(logfile, "Received event-type: %d\n", event.type);
    }
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
