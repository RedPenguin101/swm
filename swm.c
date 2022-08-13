#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

Cursor cursor;

static int screen;
static int screen_w, screen_h;
static int running = true;
static Display* display;
static Window root;

static int (*xerrorxlib)(Display*, XErrorEvent*);

Window windows[20] = {0};
int client_count = 0;
int current_client = -1;

XftColor col_bg, col_fg, col_border;

FILE* logfile;
bool verbose = false;

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

static void grabkeys() {
  XUngrabKey(display, AnyKey, AnyModifier, root);

  // q = quit, c = close, t = terminal, l = launcher
  XGrabKey(display, XKeysymToKeycode(display, XK_q), Mod4Mask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(display, XKeysymToKeycode(display, XK_c), Mod4Mask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(display, XKeysymToKeycode(display, XK_t), Mod4Mask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(display, XKeysymToKeycode(display, XK_l), Mod4Mask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(display, XKeysymToKeycode(display, XK_j), Mod4Mask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(display, XKeysymToKeycode(display, XK_k), Mod4Mask, root, True,
           GrabModeAsync, GrabModeAsync);
}

static void setup() {
  logfile = fopen("/home/joe/programming/projects/swm/log_swm.txt", "w");
  fprintf(logfile, "START SESSION LOG\n");

  XSetErrorHandler(xerror);

  screen = DefaultScreen(display);
  screen_w = DisplayWidth(display, screen);
  screen_h = DisplayHeight(display, screen);
  root = RootWindow(display, screen);
  fprintf(logfile, "Root window %ld \t\t w/h=%d,%d\n", root, screen_w,
          screen_h);

  grabkeys();

  XSetWindowAttributes wa;

  XftColorAllocName(display, DefaultVisual(display, screen),
                    DefaultColormap(display, screen), "#dedcfd", &col_bg);
  XftColorAllocName(display, DefaultVisual(display, screen),
                    DefaultColormap(display, screen), "#FF0000", &col_fg);
  XftColorAllocName(display, DefaultVisual(display, screen),
                    DefaultColormap(display, screen), "#0000FF", &col_border);
  wa.background_pixel = col_bg.pixel;

  cursor = XCreateFontCursor(display, XC_left_ptr);
  wa.cursor = cursor;

  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                  ButtonPressMask | PointerMotionMask;

  XSelectInput(display, root, wa.event_mask);
  XMoveResizeWindow(display, root, 0, 0, screen_w, screen_h);
  XChangeWindowAttributes(display, root, CWBackPixel | CWEventMask | CWCursor,
                          &wa);
  XClearWindow(display, root);
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

void spawn_dmenu() {
  if (fork() == 0) {
    fprintf(logfile, "spawning dmenu\n");
    if (display)
      close(ConnectionNumber(display));
    static const char* demenucmd[] = {
        "dmenu_run", "-m",      "0",       "-fn",     "monospace:size=10",
        "-nb",       "#bbbbbb", "-nf",     "#222222", "-sb",
        "#005577",   "-sf",     "#eeeeee", NULL};
    setsid();
    execvp(((char**)demenucmd)[0], (char**)demenucmd);
    exit(EXIT_SUCCESS);
  }
}

int find_window_client(Window w) {
  for (int i = 0; i < client_count; i++) {
    if (windows[i] == w)
      return i;
  }
  return -1;
}

void kill_window(Window w) {
  XGrabServer(display);
  XSetCloseDownMode(display, DestroyAll);
  XKillClient(display, w);

  if (client_count == 0)
    XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);

  XUngrabServer(display);
}

static void show_client(int client) {
  Window window = windows[client];
  XMoveWindow(display, window, 0, 0);
  XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
}

static void hide_client(int client) {
  Window window = windows[client];
  XMoveWindow(display, window, screen_w * -2, 0);
}

static void unmanage_client(int unmanaged_client) {
  client_count -= 1;

  for (int i = unmanaged_client; i < client_count; i++)
    windows[i] = windows[i + 1];

  if (current_client > unmanaged_client)
    current_client -= 1;

  if (current_client == unmanaged_client) {
    if (unmanaged_client == client_count)
      current_client -= 1;
    if (client_count == 0)
      XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
    else
      show_client(current_client);
  }
}

/* Handlers*/

static void keypress_handler(XEvent* event) {
  XKeyEvent* ev = &event->xkey;
  KeySym keysym = XKeycodeToKeysym(display, ev->keycode, 0);
  if (verbose)
    fprintf(logfile, "Keypress \t\t KeySym: %ld state = 0x%x\n", keysym,
            ev->state);
  if (keysym == XK_q && (ev->state == Mod4Mask || ev->state == Mod5Mask))
    running = false;
  else if (keysym == XK_t && (ev->state == Mod4Mask || ev->state == Mod5Mask))
    spawn_term();
  else if (keysym == XK_c && (ev->state == Mod4Mask || ev->state == Mod5Mask)) {
    kill_window(windows[current_client]);
    unmanage_client(current_client);
  } else if (keysym == XK_l && (ev->state == Mod4Mask || ev->state == Mod5Mask))
    spawn_dmenu();
  else if (keysym == XK_j && (ev->state == Mod4Mask || ev->state == Mod5Mask) &&
           current_client < client_count - 1) {
    hide_client(current_client);
    current_client += 1;
    show_client(current_client);
  } else if (keysym == XK_k &&
             (ev->state == Mod4Mask || ev->state == Mod5Mask) &&
             current_client > 0) {
    hide_client(current_client);
    current_client -= 1;
    show_client(current_client);
  }
}

static void mapnotify_handler(XEvent* event) {
  XMapEvent* ev = &event->xmap;
  if (verbose)
    fprintf(logfile, "MapN \t\t\t\t %ld \t\t event from %ld \n", ev->window,
            ev->event);
}

static void unmapnotify_handler(XEvent* event) {
  XUnmapEvent* ev = &event->xunmap;
  Window w = ev->window;

  fprintf(logfile, "UnmapN \t\t\t %ld \t\t event from %ld\n", ev->window,
          ev->event);

  int client = find_window_client(w);
  if (client == -1)
    fprintf(logfile, "\t Client not found\n");
  else
    unmanage_client(client);
}

static void maprequest_handler(XEvent* event) {
  XMapRequestEvent* ev = &event->xmaprequest;
  Window window = ev->window;

  windows[client_count] = window;
  current_client = client_count;
  client_count += 1;

  fprintf(logfile, "MapR \t\t\t\t %ld \t\t parent %ld \t window count %d\n",
          ev->window, ev->parent, client_count);

  XWindowAttributes wa;
  XGetWindowAttributes(display, window, &wa);

  if (verbose)
    fprintf(logfile,
            "\tReceived WA: xywhb=%d,%d,%d,%d,%d, IO:%d, mapstate=%d, "
            "all-ems=%ld, your-ems=%ld, DNP=%ld, override=%d\n",
            wa.x, wa.y, wa.width, wa.height, wa.border_width, wa.class,
            wa.map_state, wa.all_event_masks, wa.your_event_mask,
            wa.do_not_propagate_mask, wa.override_redirect);

  XSelectInput(display, window,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                   StructureNotifyMask);

  XMoveResizeWindow(display, window, 0, 0, screen_w, screen_h);
  XMapWindow(display, window);
  XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
}

static void createnotify_handler(XEvent* event) {
  XCreateWindowEvent* cwe = &event->xcreatewindow;
  if (verbose)
    fprintf(logfile, "CreateR \t\t %ld\t\t parent %ld\n", cwe->window,
            cwe->parent);
}

static void destroy_handler(XEvent* event) {
  XDestroyWindowEvent* dwe = &event->xdestroywindow;
  Window w = dwe->window;
  if (verbose)
    fprintf(logfile, "DestroyN \t\t %ld\t\t event %ld\n", w, dwe->event);
}

static void configurerequest_handler(XEvent* event) {
  XConfigureRequestEvent req = event->xconfigurerequest;
  fprintf(logfile,
          "ConfR \t\t\t %ld \t\t xywhb: %d,%d,%d,%d,%d \t\t parent: %ld above: "
          "%ld \t\t detail: %d mask: 0x%lx passed mask: 0x%lx\n",
          req.window, req.x, req.y, req.width, req.height, req.border_width,
          req.parent, req.above, req.detail, req.value_mask,
          0x3f & req.value_mask);

  XWindowChanges wc;
  wc.x = req.x;
  wc.y = req.y;
  wc.width = req.width;
  wc.height = req.height;
  XConfigureWindow(display, req.window, 0x3f & req.value_mask, &wc);
}

static void configurenotify_handler(XEvent* event) {
  XConfigureEvent* ev = &event->xconfigure;
  if (verbose)
    fprintf(logfile,
            "ConfN \t\t\t %ld \t\t xywhb: %d,%d,%d,%d,%d \t\t above: %ld\n",
            ev->window, ev->x, ev->y, ev->width, ev->height, ev->border_width,
            ev->above);
}

static void propertynotify_handler(XEvent* event) {
  XPropertyEvent* ev = &event->xproperty;
  if (verbose)
    fprintf(logfile, "PropN \t\t\t %ld \t\t %ld \t %d\n", ev->window, ev->atom,
            ev->state);
}

static void focus_handler(XEvent* event) {
  XFocusChangeEvent* ev = &event->xfocus;
  if (verbose)
    fprintf(logfile, "%s \t\t %ld \t\t mode-detail: %d-%d\n",
            (event->type == FocusIn ? "FocusIn" : "FocusOut"), ev->window,
            ev->mode, ev->detail);
}

static void enter_handler(XEvent* event) {
  XCrossingEvent* ev = &event->xcrossing;
  fprintf(logfile,
          "Cross \t\t\t %ld \t\t root: %ld sub: %ld\t\t pos: %d,%d,%d,%d \t "
          "mode-detail: "
          "%d-%d \t samescreen: %d \t focus: %d \t state 0x%x\n",
          ev->window, ev->root, ev->subwindow, ev->x, ev->y, ev->x_root,
          ev->y_root, ev->mode, ev->detail, ev->same_screen, ev->focus,
          ev->state);
}

static void mappingnotify_handler(XEvent* event) {
  XMappingEvent* ev = &event->xmapping;
  if (verbose)
    fprintf(logfile,
            "MappingN \t\t %ld \t\t request %d \t fkc %d \t count %d\n",
            ev->window, ev->request, ev->first_keycode, ev->count);
  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard)
    grabkeys();
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
      case MappingNotify:
        mappingnotify_handler(&event);
        break;
      case UnmapNotify:
        unmapnotify_handler(&event);
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
  }
}

static void cleanup() {
  for (int i = 0; i < client_count; i++)
    XUnmapWindow(display, windows[i]);
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
