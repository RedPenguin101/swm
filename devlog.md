# Devlog
## 10th August
### Crashing after trying to remove XSyncs
I wanted to take out the XSyncs because I was doing it once per run-loop, which I'm sure is massively wasteful.
But it kept crashing when I did that, with request code `X_ConfigureWindow` and error code `BadValue`.
I figured it was something to do with it not syncing when it needed to.
So I played around with XSyncs in different places but wasn't able to get much joy.
It seemed random whether it would crash or not.

I think I got it figured out:
The XConfigEvent has a value mask of `0x40`, or `CWStackMode`.
But in my config handler I'm only passing on the dimensions in the WindowAttributes, nothing to do with the stack mode.
So I _think_ that I'm passing the WA with nothing about the stack mode (or more likely, some random value in the slot for stack mode), then telling the configure function that I _am_ changing the stack mode by just passing on the config value mask from the event.

I fixed it by xor-ing out the stackmode from the value mask:

```c
XConfigureWindow(display, req.window, CWStackMode ^ req.value_mask, &wc);
```
Now the first window loads fine, but I get weird behaviour when launching a second window:
* App1 is terminal, App2 is Surf, just dumps me back to desktop
* App1 is terminal, App2 is Firefox, crashes program
* App1 is terminal, App2 is terminal - works fine

Actually, even just launching Firefox results in a crash.
That's probably the place to start here.
These are the logs

```
Root window 1980 		 w/h=3840,2160
MappingN 		 0 		 request 1 	 fkc 8 	 count 248
Unhandled event-type: 3
MapN 				 6291460 		 event from 1980 
UnmapN 			 6291460 		 event from 1980 	 Window not found
ConfR 			 6291500 		 xywhb: 0,0,2880,2160,0 		 parent: 1980 above: 0 		 detail: 0 mask: 0xc 
Unhandled event-type: 33
fatal error: request code=12, error code=2, resource=160
```

It's that same thing again: invalid values on config. The mask is width/height change. It's asking for 2880/2160, which _should_ be fine since the screen size is 3840/2160.

So after a bit of digging, turns out my XOR hack broke things. If the value mask is `0xc`, XORing that with `0x40` gives `0x4c`.
What I need is a bitwise and with the ones complement of `0x40`, which is `0x3F`.  


```c
XConfigureWindow(display, req.window, 0x3f & req.value_mask, &wc);
```

That fixed the crashing, but there's still no firefox page showing up on screen.

That might've been because there were already Firefox windows running in the other session, and they were sent there. 
I quit out of them on my Gnome env and it started working on SWM.
It's not grabbing Keyboard focus if you launch from a terminal though. 
If you type, the input shows up in the terminal, not Firefox.

I think this is either because I'm not handling the MapRequest properly (not setting the event mask right?), or I should be unmapping the terminal when Firefox opens.
Let's look at the first one first.

I added a bunch of mapping for MapRequest, with the following result

```
MapR 				 6291471 		 parent 1980 	 window count 1
	Received WA: xywhb=0,0,3840,2160,0, IO:1, mapstate=0, all-ems=6520959, your-ems=0, DNP=0, override=0
MapR 				 8388652 		 parent 1980 	 window count 2
	Received WA: xywhb=0,0,3840,2160,0, IO:1, mapstate=0, all-ems=6529151, your-ems=0, DNP=0, override=0
```

Those event masks include a bunch of stuff, _including_ all the button press, keypress stuff.
So I don't think that's the problem.

I noticed there's also a `XSetInputFocus` That I'm using to revert back to root.
That's the next think I'll try.

```c

static void maprequest_handler(XEvent* event) {
  // ...
  XMoveResizeWindow(display, window, 0, 0, screen_w, screen_h);
  XMapWindow(display, window);
  XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
}
```

That fixed I think.

I do need to handle the re-focusing when a window is closed though.

In this session I opened a terminal, loaded FF from that, and then closed the FF window:

```
START SESSION LOG
Unhandled event-type: 3
MapR 				 6291471 		 parent 1980 	 window count 1
FocusOut 		 6291471 		 mode-detail: 0-5
FocusIn 		 6291471 		 mode-detail: 0-3
MapR 				 8388652 		 parent 1980 	 window count 2
FocusOut 		 6291471 		 mode-detail: 0-3
FocusIn 		 8388652 		 mode-detail: 0-3
UnmapN 			 8388652 		 event from 8388652 	 Window count 1
FocusOut 		 8388652 		 mode-detail: 0-3
FocusIn 		 8388652 		 mode-detail: 0-5
FocusOut 		 6291471 		 mode-detail: 1-5
FocusIn 		 6291471 		 mode-detail: 1-5
END SESSION LOG
```

The focus events are interesting here.
They seem to always ceom in pairs (DWM doesn't even handle FocusOut events, which is telling).
The "modes" are always NotifyNormal except the last one (putting focus back to the terminal after FF is closed) which is NotifyGrab.
(The Details, which I don't understand at all, are a mix of NotifyNonlinear and NotifyPointer.)

The DWM handler for FocusIn is simply:

```c
void focusin(XEvent* e) {
  XFocusChangeEvent* ev = &e->xfocus;

  if (selmon->sel && ev->window != selmon->sel->win)
    // if there is a selected client and it's not ALREADY focued, switch it.
    setfocus(selmon->sel);
}

void setfocus(Client* c) {
  if (!c->neverfocus) {
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char*)&(c->win), 1);
  }
  sendevent(c, wmatom[WMTakeFocus]);
}
```

This seems pretty simple.
But I don't think I actually need to do this.
What I think I should do in this situation, is when the client unmaps I should set the focus to the previous window in the stack.

```c
static void unmapnotify_handler(XEvent* event) {
  //...all the actual unmapping stuff
  if (window_count == 0)
    XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
  else
    XSetInputFocus(display, windows[window_count - 1], RevertToPointerRoot,
                   CurrentTime);
}
```

That seems to work OK.