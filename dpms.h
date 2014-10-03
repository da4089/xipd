#include <X11/Xlib.h>


extern int
xipd_read_dpms(Display *display, int *state);

extern int
xipd_have_dpms(Display *display);
