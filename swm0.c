#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdio.h>

static int screen;
static int screen_w, screen_h;
static int running = true;
static Display* display;
static Window root;

Cursor cursor;

FILE* logfile;
const char* logpath = "/home/joe/programming/projects/swm/log_swm0.txt";

static void setup() {
  logfile = fopen(logpath, "w");
  fprintf(logfile, "START SESSION LOG\n");

  screen = DefaultScreen(display);
  screen_w = DisplayWidth(display, screen);
  screen_h = DisplayHeight(display, screen);
  root = RootWindow(display, screen);
  fprintf(logfile, "Root window %ld\n", root);

  XSetWindowAttributes wa;
  cursor = XCreateFontCursor(display, XC_left_ptr);
  wa.cursor = cursor;

  wa.event_mask =
      SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask;

  XMoveResizeWindow(display, root, 0, 0, screen_w, screen_h);
  XChangeWindowAttributes(display, root, CWEventMask | CWCursor, &wa);
}

static void keypress_handler(XEvent* event) {
  XKeyEvent* ev = &event->xkey;
  KeySym keysym = XKeycodeToKeysym(display, ev->keycode, 0);
  fprintf(logfile, "Keypress \t\t KeySym: %ld state = 0x%x\n", keysym,
          ev->state);
  if (keysym == XK_q)
    running = false;
}

static void run() {
  XEvent event;
  while (running && XNextEvent(display, &event) == 0) {
    switch (event.type) {
      case KeyPress:
        keypress_handler(&event);
        break;
      default:
        fprintf(logfile, "Unhandled event-type: %d\n", event.type);
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
