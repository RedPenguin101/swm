# Devlog
## 12th/13th August
### Shifting windows with mod-arrow.
Obviously, need to figure out which keys are arrow keys.
Or maybe make it jk for now.
Maybe that's better actually.
The thing is, what to do with empty 'slots'? just skip them?
Or more proactive 'scan'?

1. Open window 1, fills screen (curr=0 count=1)
2. Open window 2, fills screen, replace w1. (curr=1 count=2)
3. "Shift" - move 'pointer' to earlier window (curr=0 count=1)

Got this working, pretty simple:

```c
static void show_window(int idx) {
  Window window = windows[idx];
  XMoveWindow(display, window, 0, 0);
  XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
}

static void hide_window(int idx) {
  Window window = windows[idx];
  XMoveWindow(display, window, screen_w * -2, 0);
}

// then in keypress handler

  else if (keysym == XK_j && (ev->state == Mod4Mask || ev->state == Mod5Mask) &&
           current_window < window_count - 1) {
    hide_window(current_window); // current window is a new global
    current_window += 1;
    show_window(current_window);
```

Slightly interesting bit here: to 'hide' a window you just move it off to the left of the screen. I copied this from dwm.

### Closing clients
Up to now, the WM has operated like a strict stack: the 'top' of the stack is always what is displayed.
So killing the visible window will always be killing the top of the stack, and reverting to the next window.
This is hard-coded into the program - you can't unmap something that isn't at the top of the stack.

```c
  else if (idx != window_count - 1)
    fprintf(logfile, "\t Window wasn't top of stack\n");
```

Now the user has the ability to move around in the stack, they could be closing windows that are not top of the stack, so I need to handle that:

* Allow users to close windows that are not top of the stack
* Do that in such a way that there are not 'gaps' in the client array
* Manage the case where the user closes the focused window (which will be the most common user case).

There are two ways a window can be closed:
* The client closes itself (including on user command) and sends an `unmapnotify` to Xorg
* The WM 'kills' the client with a command (currently this is done with the `kill_window` function.)

I think it makes sense to harmonize these a bit with an `unmanage_client` function, that:

1. Given a client number (array index), figures out which window it is (just an array lookup)
2. Shuffles the client list around so there are no 'holes'
3. Figures out which client it should be displaying now, and displays it.

[A Side Note, I really need to make a better distinction between 'clients' and 'windows' in the code.]

Looking at some scenarios of client list befores and afters:

```
  v unmanage this
12345
1245

    v unmanage this
12345
1234

v unmanage this
12345
2345
```

So I think this is a simple case of iterating through the array of the idx following the killed client and shifting them down one to close the gap.

If the client that's unmanaged is the current client (which it probably will be), we need to decide what the _new_ client is, and display this.
If it's the top of the stack, you need to move it to the previous one.
If it's the first in the stack, you need to move it to the _next_ one.
If it's in the middle, it could be either. Arbitrarily I'll say it should be the next one.

(Note that, even if the unmanaged client _isn't_ the visible window, we'll need to change the current client if it has a higher index than the unmanaged one)

```
     v unmanaged=5
xxxxxX
xxxxX
    ^new=4

Xxxxxx
Xxxxx (after client moves)
0->0

xxxXxx
xxxXx (after client moves)
3->3

X
-
```
The main addition is the `unmanage_client` which works as described above.
I also changed `kill_window` to accept an actual window as a parameter (since it won't be able to assume it's the top of the stack anymore),
and updated unmap handlers and close client keypress handler to use `unmanage_client`.

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

But I'm done with this - bit of a rabbit hole!

### Surf sizing
One thing I noticed is that Surf thinks it's in a tiny window when it's not.
When I search it's all squished to the left.

```
ConfR 			 10485763 		 xywhb: 0,0,800,600,0 		 parent: 1980 above: 0 		 detail: 0 mask: 0x40 passed mask: 0x0
MapR 				 10485763 		 parent 1980 	 window count 2
	Received WA: xywhb=0,0,800,600,0, IO:1, mapstate=0, all-ems=4947968, your-ems=0, DNP=0, override=0
UnmapN 			 10485763 		 event from 10485763 	 Window not found
END SESSION LOG
```

So first I get a confR from surf with 800x600 size. 
I straight up pass that on to Xorg.
_Then_ I get a MapR, again with 800x600, but instead I do a `XMoveResizeWindow(display, window, 0, 0, screen_w, screen_h);`

Does this pass the memo on to the window itself? What if I get the WA after I do the resize?

```
MapR 				 10485763 		 parent 1980 	 window count 2
	Received WA: xywhb=0,0,800,600,0, IO:1, mapstate=0, all-ems=4947968, your-ems=0, DNP=0, override=0
	Post resize WA: xywhb=0,0,3840,2160,0, IO:1, mapstate=2, all-ems=7045136, your-ems=6422544, DNP=0, override=0
```

This all seems fine. 
I think maybe I need to pass the message on to Surf that I've ignored it's sizing request?

DWMs configure request handler (`configurerequest(XEvent* e)`) has some pretty gnarly triple-nested ifs, but I think at some point it calls `configure(c)`, which I _think_ messages back to the Window?
I also think it only calls `XConfigureWindow` if it doesn't have the window mapped as a client.

I tried a few things here, including sending surf a confignotify event, but I'm pretty sure that X already does that on XConfigure.

Putting a pin in this for now, since it seems like it's isolated to surf.
