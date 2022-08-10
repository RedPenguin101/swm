# Devlog
## 10th August
I wanted to take out the XSyncs, but it kept crashing when I did that, with request code `X_ConfigureWindow` and error code `BadValue`.
I figured it was something to do with it not syncing when it needed to.
So I played around with XSyncs in different places but wasn't able to get much joy.
It seemed random whether it would crash or not.

I think I got it figured out:
The XConfigEvent has a value mask of `0x40`, or `CWStackMode`.
But in my config handler I'm only passing on the dimensions in the WindowAttributes, nothing to do with the stack mode.
So I _think_ that I'm passing the WA with nothing about the stack mode (or more likely, some random value in the slot for stack mode), then telling the configure function that I _am_ changing the stack mode by just passing on the config value mask from the event.

I fixed it by xor-ing out the stackmode from the value mask 

```c
XConfigureWindow(display, req.window, CWStackMode ^ req.value_mask, &wc);
```