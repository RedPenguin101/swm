// setup stuff
Atom WMProtocols;
Atom WMDelete;
Atom WMState;
Atom WMTakeFocus;

WMProtocols = XInternAtom(display, "WM_PROTOCOLS", False);
WMDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
WMState = XInternAtom(display, "WM_STATE", False);
WMTakeFocus = XInternAtom(display, "WM_TAKE_FOCUS", False);

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

Cursor cursor;

cursor = XCreateFontCursor(display, XC_left_ptr);
wa.cursor = cursor;
