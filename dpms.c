

#include "config.h"

#if ! defined(HAVE_DPMS_EXTENSION)

int xipd_read_dpms(Display *display, int *state)
{
    return 0;
}

int xipd_have_dpms(Display *display)
{
    return 0;
}

#else  /* HAVE_DPMS_EXTENSION */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/dpmsstr.h>

/* Define API to DPMS functions, cos it's not already defined! */
extern Bool   DPMSQueryExtension(Display *dpy, int *event_ret, int *error_ret);
extern Bool   DPMSCapable(Display *dpy);
extern Status DPMSForceLevel(Display *dpy, CARD16 level);
extern Status DPMSInfo(Display *dpy, CARD16 *power_level, BOOL *state);
extern Status DPMSGetVersion(Display *dpy, int *major_ret, int *minor_ret);
extern Status DPMSSetTimeouts(Display *dpy, CARD16 standby, CARD16 suspend, CARD16 off);
extern Bool   DPMSGetTimeouts(Display *dpy, CARD16 *standby, CARD16 *suspend, CARD16 *off);
extern Status DPMSEnable(Display *dpy);
extern Status DPMSDisable(Display *dpy);


/* Read X DPMS extension and extract monitor state */
int xipd_read_dpms(Display *display, int *state)
{
    CARD16 power_level;
    BOOL enabled;

    /* Query DPMS status */
    DPMSInfo(display, &power_level, &enabled);
    if (! enabled) {
        return 0;
    }

    /* Convert power level to on/off status */
    switch (power_level) {
    case DPMSModeOn:
        *state = 1;
        break;

    case DPMSModeStandby:
    case DPMSModeSuspend:
    case DPMSModeOff:
        *state = 0;
        break;

    default:
        return 0;
    
    }

    return 1;
}


int xipd_have_dpms(Display *display)
{
    Bool result;
    int event_number, error_number;

    if (! DPMSQueryExtension(display, &event_number, &error_number)) {
        return 0;
    }

    result = DPMSCapable(display);
    if (result == True) {
        return 1;
    } else {
        return 0;
    }
}

#endif /* HAVE_DPMS_EXTENSION */
