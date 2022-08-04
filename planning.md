# Plan for my Singleton WM

A singleton WM has only one fullscreen client visible at a time.
If you open a new one, the old one goes on the stack. If you close the one at the top of the stack, the next one becomes the top one.

Features:
* no tags/workspaces/desktops
* No multimonitor support
* No switching: only open, close stack behavior
* No client decorations. Kill apps with Super+c
* The only thing you can open is a terminal (with super+enter). Then you launch other applications from that.
* No status bar
* only other shortcut is to close the WM with Super+Q
* No backgrounds or fancy things like that.
* No configuration

What will I need?
* Cursor
* Font
* Drawing Rectangle for the title bar
* Event handler for Map event
* Event handler for Unmap event
* Linked list for the client stack
* Keypress handlers for super+enter/c/q