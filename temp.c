// setup stuff

XftColor col_bg, col_fg, col_border;

XftColorAllocName(display,
                  DefaultVisual(display, screen),
                  DefaultColormap(display, screen),
                  "#00FF00",
                  &col_bg);
XftColorAllocName(display,
                  DefaultVisual(display, screen),
                  DefaultColormap(display, screen),
                  "#FF0000",
                  &col_fg);
XftColorAllocName(display,
                  DefaultVisual(display, screen),
                  DefaultColormap(display, screen),
                  "#0000FF",
                  &col_border);
wa.background_pixel = col_bg.pixel;

// clients
#define MAX_CLIENTS 5

Window clients[MAX_CLIENTS];
int client_count = 0;