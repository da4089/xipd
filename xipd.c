
#include "config.h"

#if defined(HAVE_ERRNO_H)
# include <errno.h>
#endif

#if defined(HAVE_FCNTL_H)
# include <fcntl.h>
#endif

#if defined(HAVE_NETDB_H)
# include <netdb.h>
#endif

#if defined(HAVE_STDLIB_H)
# include <stdlib.h>
#endif

#if defined(HAVE_STDIO_H)
# include <stdio.h>
#endif

#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_SYS_TYPES_H)  /* before network stuff */
# include <sys/types.h>
#endif

#if defined(HAVE_SYS_SOCKET_H)
# include <sys/socket.h>
#endif

#if defined(HAVE_NETINET_IN_H)
# include <netinet/in.h>
#endif

#if defined(HAVE_ARPA_INET_H)
# include <arpa/inet.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include <elvin/elvin.h>
#include <elvin/xt_mainloop.h>


/*#define ACTIVE_DELAY  (300)*/
#define ACTIVE_DELAY  (30)
#define APP_CLASS     "Xipd"


static elvin_error_t error;
static elvin_handle_t handle;
static elvin_notification_t nfn;


/* Application configuration */
typedef struct app_data {
    char *elvin;
    char *scope;
    char *user;
    char *domain;
    char *location;
    char *groups;
    Bool standalone;
    Bool help;
    Bool version;
    Bool debug;
} *app_data_t;

/* Legal command-line options */
static XrmOptionDescRec opt_table[] = 
{
    {"-elvin",       "*elvin",      XrmoptionSepArg, (XPointer)NULL},
    {"--elvin",      "*elvin",      XrmoptionSepArg, (XPointer)NULL},
    {"-Scope",       "*scope",      XrmoptionSepArg, (XPointer)NULL},
    {"--scope",      "*scope",      XrmoptionSepArg, (XPointer)NULL},
    {"-user",        "*user",       XrmoptionSepArg, (XPointer)NULL},
    {"--user",       "*user",       XrmoptionSepArg, (XPointer)NULL},
    {"-Domain",      "*domain",     XrmoptionSepArg, (XPointer)NULL},
    {"--domain",     "*domain",     XrmoptionSepArg, (XPointer)NULL},
    {"-location",    "*location",   XrmoptionSepArg, (XPointer)NULL},
    {"--location",   "*location",   XrmoptionSepArg, (XPointer)NULL},
    {"-standalone",  "*standalone", XrmoptionNoArg,  "True"},
    {"--standalone", "*standalone", XrmoptionNoArg,  "True"},
    {"-help",        "*help",       XrmoptionNoArg,  "True"},
    {"--help",       "*help",       XrmoptionNoArg,  "True"},
    {"-version",     "*version",    XrmoptionNoArg,  "True"},
    {"--version",    "*version",    XrmoptionNoArg,  "True"},
};


/* Conversions and default values for options */
static XtResource resources[] =
{
    {"elvin", "Elvin", XtRString, sizeof(XtRString), XtOffsetOf(struct app_data, elvin), XtRImmediate, NULL},
    {"scope", "Scope", XtRString, sizeof(XtRString), XtOffsetOf(struct app_data, scope), XtRImmediate, ""},
    {"user", "User", XtRString, sizeof(XtRString), XtOffsetOf(struct app_data, user), XtRImmediate, ""},
    {"domain", "Domain", XtRString, sizeof(XtRString), XtOffsetOf(struct app_data, domain), XtRImmediate, ""},
    {"location", "Location", XtRString, sizeof(XtRString), XtOffsetOf(struct app_data, location), XtRImmediate, ""},
    {"groups", "Groups", XtRString, sizeof(XtRString), XtOffsetOf(struct app_data, groups), XtRImmediate, ""},
    {"standalone", "standalone", XtRBool, sizeof(XtRBool), XtOffsetOf(struct app_data, standalone), XtRBool, "False"},
    {"help", "Help", XtRBool, sizeof(XtRBool), XtOffsetOf(struct app_data, help), XtRBool, "False"},
    {"version", "Version", XtRBool, sizeof(XtRBool), XtOffsetOf(struct app_data, version), XtRBool, "False"},
};


/* Prints out the usage message */
static void do_usage(int argc, char *argv[], FILE *file)
{
    fprintf(file, "Usage: %s [OPTION ...]\n", argv[0]);
    fprintf(file, "  -e elvin-url, --elvin=elvin-url\n");
    fprintf(file, "  -S scope,     --scope=scope\n");
    fprintf(file, "  -u user,      --user=user\n");
    fprintf(file, "  -D domain,    --domain=domain\n");
    fprintf(file, "  -l location,  --location=location\n");
    fprintf(file, "  -g groups,    --groups=group[,group...]\n");
    fprintf(file, "  -d display,   --display=host:0\n");
    fprintf(file, "  -s,           --standalone\n");
    fprintf(file, "  -v,           --version\n");
    fprintf(file, "  -h,           --help\n");
}


/* Send notification of state change */
int send_event(int now_active)
{
    struct timeval tv;
    char host[64], addr[64];
    struct hostent *this_host;

    /* Build the notification */
    if (! (nfn = elvin_notification_alloc(error))) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

    if (! elvin_notification_add_int32 (nfn, "activity.0x1.org", 1000, error)) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

    /* Get host name */
    if (gethostname(host, 64) < 0) {
        fprintf(stderr, "Failed to get trap host name\n");

    } else {
        /* Lookup trap host details */
        if (! (this_host = gethostbyname(host))) {
            fprintf(stderr, "Failed to get trap host address\n");

        } else {
            if (! elvin_notification_add_string (nfn, "Host-Name", this_host->h_name, error)) {
                elvin_error_fprintf(stderr, error);
                exit(1);
            }

            if (! inet_ntop(AF_INET, this_host->h_addr_list[0], addr, 64)) {
                fprintf(stderr, "Failed to get convert trap host address\n");
                strcpy(addr, "<unknown host>");
            }

            if (! elvin_notification_add_string (nfn, "Host-IP4-Address", addr, error)) {
                elvin_error_fprintf(stderr, error);
                exit(1);
            }
        }
    }

    /* Lookup time */
    if (gettimeofday(&tv, NULL) < 0) {
        fprintf(stderr, "Failed to get time of day!\n");

    } else {
        if (! elvin_notification_add_int32 (nfn, "Date-Time", tv.tv_sec, error)) {
            elvin_error_fprintf(stderr, error);
            return 0;
        }
    }

    /* Set state */
    if (! elvin_notification_add_string(nfn, 
                                        "State", 
                                        now_active ? "active" : "inactive", 
                                        error)) {
        elvin_error_fprintf(stderr, error);
        return 0;
    }

    /* Send notification */
    if (! elvin_sync_notify(handle, nfn, 1, NULL, error)) {
        elvin_error_fprintf(stderr, error);
        return 0;
    }

    /* Clean up notification */
    elvin_notification_remove(nfn, "Date-Time", NULL);
    elvin_notification_remove(nfn, "State", NULL);

    if (! elvin_notification_free(nfn, error)) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }


    return 1;
}


int foo(void)
{
    Display *display = NULL;
    int have_x, have_dpms, have_proc, is_active;

    /* */
    if (have_dpms) {

        printf("have dpms!\n");

        while (1) {
            int on;
            
            xipd_read_dpms(display, &on);

            if (on) {
                if (! is_active) {
                    printf("went active\n");
                    is_active = 1;
                    send_event(1);
                }

            } else {
                if (is_active) {
                    printf("went passive\n");
                    is_active = 0;
                    send_event(0);
                }
            }

            printf("sleep\n");
            sleep (ACTIVE_DELAY);
        }

    } else if (have_proc) {

        long keyboard, last_keyboard = 0;
        long mouse, last_mouse = 0;

        /* Loop, looking for change and no-change */
        while (1) {

            /* Get interrupt counts */
            if (! (xipd_read_proc(&keyboard, &mouse))) {
                fprintf(stderr, "error reading /proc/interrupts !?\n");
                exit(1);
            }

            printf("is_active %d\tkeyb %ld\tmouse %ld\n", is_active, keyboard, mouse);

            if (keyboard == last_keyboard && mouse == last_mouse) {
                /* No change == idle */
                if (is_active) {
                    printf("went passive ...\n");
                    is_active = 0;
                    send_event(0);
                }

            } else {
                /* Change == not idle */
                if (! is_active) {
                    printf("went active ...\n");
                    is_active = 1;
                    send_event(1);
                }
            }

            /* Sleep until next reading */
            sleep(ACTIVE_DELAY);

            /* Update */
            last_keyboard = keyboard;
            last_mouse = mouse;
        }
    }
}


void disconnect_cb(elvin_handle_t handle, int result, void *rock, elvin_error_t error)
{
    printf("disconnect\n");

    /* Request mainloop be halted. */
    XtAppSetExitFlag((XtAppContext)rock);

    return;
}


void connect_cb(elvin_handle_t handle, int result, void *rock, elvin_error_t error)
{

    printf("connect\n");

    /* Disconnect and clean up */
    if (! elvin_xt_disconnect(handle, disconnect_cb, rock, error) ) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

    return;
}


/* Main */

int main(int argc, char *argv[])
{
    XtAppContext app_context;
    Widget toplevel;
    struct app_data app_data;
    char *display_name = NULL;
    int xargc = argc;

    char c;
    char *space, *cr;
    int is_active = 1;
    int have_x, have_dpms, have_proc;


    /* Initialize */
    toplevel = XtVaAppInitialize(&app_context,
                                 APP_CLASS,
                                 opt_table,
                                 16,
                                 &xargc,
                                 argv,
                                 NULL,
                                 NULL, NULL);

    /* Check for excess args */
    if (xargc != 1) {
        do_usage(argc, argv, stderr);
        exit(1);
    }

    /* Fetch option values */
    XtVaGetApplicationResources(toplevel,
                                &app_data,
                                resources,
                                XtNumber(resources),
                                NULL);
    /* Help */
    if (app_data.help == True) {
        printf("help\n");
        exit(0);
    }

    /* Version */
    if (app_data.version == True) {
        printf(VERSION "\n");
        exit(0);
    }

    /* Check whether we can get activity info */
    have_dpms = xipd_have_dpms(XtDisplay(toplevel));
    have_proc = xipd_have_proc();

    if (! have_dpms && ! have_proc) {
        fprintf(stderr, "Cannot use DPMS or /proc for activity detection.\n");
        exit(1);
    }

    /* Initialise Elvin */
#if ! defined(ELVIN_VERSION_AT_LEAST)
    if (! (error = elvin_xt_init(app_context))) {
        fprintf(stderr, "Failed to initialise Elvin library\n");
        exit(1);
    }
#else
    if (! (error = elvin_error_alloc(NULL))) {
        fprintf(stderr, "Failed to allocate error context!\n");
        exit(1);
    }

    if (! elvin_xt_init_default(app_context, error)) {
        fprintf(stderr, "Failed to initialise Elvin library\n");
        exit(1);
    }
#endif

    /* Create handle */
    if (! (handle = elvin_handle_alloc(error))) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

    /* Un-limit connection retries */
    if (! elvin_handle_set_connection_retries(handle, -1, error)) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

    /* Add router URL to handle */
    if (app_data.elvin) {
        if (! elvin_handle_append_url(handle, app_data.elvin, error)) {
            elvin_error_fprintf(stderr, error);
            exit(1);
        }
    }

    /* Connect to the server */
    if (! elvin_xt_connect(handle, connect_cb, (void *)app_context, error)) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

    /* Start mainloop */
    XtAppMainLoop(app_context);

    /* Clean up */
    if (! elvin_handle_free(handle, error) || ! elvin_xt_cleanup(1, error)) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

#if defined(DEBUG)
    elvin_memory_report();
#endif

    exit(0);
}
