
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

#if defined(HAVE_SIGNAL_H)
# include <signal.h>
#endif


#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include <elvin/elvin.h>
#include <elvin/xt_mainloop.h>


/*#define ACTIVE_DELAY  (300)*/
#define ACTIVE_DELAY        (3)
#define APP_CLASS           "Xipd"

#define STATUS_ONLINE       "online"
#define STATUS_DOUBTFUL     "unavailable?"
#define STATUS_UNAVAILABLE  "unavailable"
#define STATUS_OFFLINE      "offline"

#define TEXT_ONLINE         "Active at %s"
#define TEXT_DOUBTFUL       "Last active at %s at %s"


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


/* Application runtime state */
typedef struct state {
    /* X properties */
    XtAppContext app_context;
    Widget toplevel;
    Display *display;

    /* Elvin properties */
    elvin_error_t error;
    elvin_handle_t handle;

    /* Config info, extracted from X resources */
    struct app_data app_data;

    /* Activity monitor timer */
    XtIntervalId timer;

    /* Current state flag */
    int is_active;

    /* Monitoring mechanism flags */
    int have_dpms;
    int have_proc;

    /* Last sample counts for /proc interface */
    long last_keyboard, last_mouse;

    /* Time of last state change (for duration calculations) */
    time_t last_change;

    /* Calculated text field for notifications */
    char *host;
    char *addr;
    char *user;
    char *client_id;

} *state_t;


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
    {"-groups",      "*groups",     XrmoptionSepArg, (XPointer)NULL},
    {"--groups",     "*groups",     XrmoptionSepArg, (XPointer)NULL},
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


/* Signal handler identifier */
static XtSignalId sigint_id;



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


int send_initial_presence_info(state_t s)
{
    elvin_notification_t nfn;
    char text[64];

    /* Build the notification */
    if (! (nfn = elvin_notification_alloc(s->error))) {
        elvin_error_fprintf(stderr, s->error);
        exit(1);
    }

    /* Build status text */
    snprintf(text, 64, "xipd presence monitor started at %s", 
             strlen(s->app_data.location) ? s->app_data.location : s->host);

    /* Build notification */
    if (! elvin_notification_add_int32 (nfn, "Presence-Protocol", 1000, s->error) ||
        ! elvin_notification_add_string(nfn, "Presence-Info", "initial", s->error) ||
        ! elvin_notification_add_string(nfn, "Client-Id", s->client_id, s->error) ||
        ! elvin_notification_add_string(nfn, "User", s->user, s->error) ||
        ! elvin_notification_add_string(nfn, "Groups", s->app_data.groups, s->error) ||
        ! elvin_notification_add_string(nfn, "Status", STATUS_ONLINE, s->error) ||
        ! elvin_notification_add_string(nfn, "Status-Text", text, s->error) ||
        ! elvin_notification_add_int32 (nfn, "Status-Duration", 0, s->error)) {
        elvin_error_fprintf(stderr, s->error);
        exit(1);
    }

    /* Send notification */
    if (! elvin_xt_notify(s->handle, nfn, 1, NULL, s->error)) {
        elvin_error_fprintf(stderr, s->error);
        exit(1);
    }

    /* Free notification */
    if (! elvin_notification_free(nfn, s->error)) {
        elvin_error_fprintf(stderr, s->error);
        exit(1);
    }

    return 1;
}


int send_final_presence_info(state_t s)
{
    elvin_notification_t nfn;

    /* Build the notification */
    if (! (nfn = elvin_notification_alloc(s->error))) {
        elvin_error_fprintf(stderr, s->error);
        exit(1);
    }

    /* Construct notification */
    if (! elvin_notification_add_int32 (nfn, "Presence-Protocol", 1000, s->error) ||
        ! elvin_notification_add_string(nfn, "Presence-Info", "update", s->error) ||
        ! elvin_notification_add_string(nfn, "Client-Id", s->client_id, s->error) ||
        ! elvin_notification_add_string(nfn, "User", s->user, s->error) ||
        ! elvin_notification_add_string(nfn, "Groups", s->app_data.groups, s->error) ||
        ! elvin_notification_add_string(nfn, "Status", STATUS_OFFLINE, s->error) ||
        ! elvin_notification_add_string(nfn, "Status-Text", 
                                        "xipd presence monitor exited" , s->error) ||
        ! elvin_notification_add_int32 (nfn, "Status-Duration", 0, s->error)) {
        elvin_error_fprintf(stderr, s->error);
        exit(1);
    }

    /* Send notification */
    if (! elvin_xt_notify(s->handle, nfn, 1, NULL, s->error)) {
        elvin_error_fprintf(stderr, s->error);
        return 0;
    }

    /* Free notification */
    if (! elvin_notification_free(nfn, s->error)) {
        elvin_error_fprintf(stderr, s->error);
        exit(1);
    }

    return 1;
}


/* Send notification of state change */
int send_event(state_t s, int now_active)
{
    elvin_notification_t nfn;
    char text[64];

    /* Build the notification */
    if (! (nfn = elvin_notification_alloc(s->error))) {
        elvin_error_fprintf(stderr, s->error);
        exit(1);
    }

    if (s->app_data.standalone == True) {

        /* Construct status text */
        if (now_active) {
            snprintf(text, 64, TEXT_ONLINE, 
                     strlen(s->app_data.location) ? s->app_data.location : s->host);
        } else {
            snprintf(text, 64, TEXT_DOUBTFUL, 
                     strlen(s->app_data.location) ? s->app_data.location : s->host,
                     ctime(&(s->last_change)));
        }

        /* Construct notification */
        if (! elvin_notification_add_int32(nfn, "Presence-Protocol", 1000, s->error) ||
            ! elvin_notification_add_string(nfn, "Presence-Info", "update", s->error) ||
            ! elvin_notification_add_string(nfn, "Client-Id", s->client_id, s->error) ||
            ! elvin_notification_add_string(nfn, "User", s->user, s->error) ||
            ! elvin_notification_add_string(nfn, "Groups", s->app_data.groups, s->error) ||
            ! elvin_notification_add_string(nfn, "Status", 
                                            now_active ? STATUS_ONLINE : STATUS_DOUBTFUL, 
                                            s->error) ||
            ! elvin_notification_add_string(nfn, "Status-Text", text, s->error) ||
            ! elvin_notification_add_int32(nfn, "Status-Duration", 
                                           time(NULL) - s->last_change, s->error)) {
            elvin_error_fprintf(stderr, s->error);
            exit(1);
        }

    } else { /* activity.0x1.org */

        /* Construct notification */
        if (! elvin_notification_add_string(nfn, "Host-Name", s->host, s->error) ||
            ! elvin_notification_add_string(nfn, "Host-IP4-Address", s->addr, s->error) ||
            ! elvin_notification_add_int32 (nfn, "activity.0x1.org", 1000, s->error) ||
            ! elvin_notification_add_int32 (nfn, "Date-Time", time(NULL), s->error) ||
            ! elvin_notification_add_string(nfn, "State", 
                                            now_active ? "active" : "inactive", 
                                            s->error)) {
            elvin_error_fprintf(stderr, s->error);
            exit(1);
        }
    }

    /* Send notification */
    if (! elvin_xt_notify(s->handle, nfn, 1, NULL, s->error)) {
        elvin_error_fprintf(stderr, s->error);
        return 0;
    }

    /* Free notification */
    if (! elvin_notification_free(nfn, s->error)) {
        elvin_error_fprintf(stderr, s->error);
        exit(1);
    }

    /* Reset last update time */
    s->last_change = time(NULL);

    return 1;
}


int check_activity(state_t state)
{
    /* */
    if (state->have_dpms) {
        int on;

        xipd_read_dpms(state->display, &on);

        if (on) {
            if (! state->is_active) {
                printf("went active\n");
                state->is_active = 1;
                send_event(state, 1);
            }

        } else {
            if (state->is_active) {
                printf("went passive\n");
                state->is_active = 0;
                send_event(state, 0);
            }
        }

    } else if (state->have_proc) {

        long keyboard = 0;
        long mouse = 0;

        /* Get interrupt counts */
        if (! (xipd_read_proc(&keyboard, &mouse))) {
            fprintf(stderr, "error reading /proc/interrupts !?\n");
            exit(1);
        }

        if (keyboard == state->last_keyboard && mouse == state->last_mouse) {
            /* No change == idle */
            if (state->is_active) {
                printf("went passive ...\n");
                state->is_active = 0;
                send_event(state, 0);
            }

        } else {
            /* Change == not idle */
            if (! state->is_active) {
                printf("went active ...\n");
                state->is_active = 1;
                send_event(state, 1);
            }
        }

        /* Update */
        state->last_keyboard = keyboard;
        state->last_mouse = mouse;
    }
}


void sub_cb(void)
{
    /* Send notification of current status */
}


void disconnect_cb(elvin_handle_t handle, int result, void *rock, elvin_error_t error)
{
    printf("disconnect\n");

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


/* Handle Unix signal */
void sigint(int signo)
{
    XtNoticeSignal(sigint_id);
}


/* Handle SIGINT signal callback from Xt */
void sigint_cb(XtPointer rock, XtSignalId *id)
{
    state_t s = (state_t)rock;

    /* Send final notification */
    if (s->app_data.standalone) {
        send_final_presence_info(s);
    }

    /* Disconnect and clean up */
    if (! elvin_xt_disconnect(s->handle, disconnect_cb, rock, s->error) ) {
        elvin_error_fprintf(stderr, s->error);
        exit(1);
    }

    return;
}



void timer_cb(XtPointer rock, XtIntervalId *id)
{
    state_t state = (state_t)rock;

    /* See if machine has changed state */
    check_activity(state);

    /* Reschedule timer */
    state->timer = XtAppAddTimeOut(state->app_context, 
                                   ACTIVE_DELAY * 1000, 
                                   timer_cb, 
                                   (XtPointer)state);
    return;
}


void connect_cb(elvin_handle_t handle, int result, void *rock, elvin_error_t error)
{
    state_t state = (state_t)rock;

    /* Set SIGINT handler */
    sigint_id = XtAppAddSignal(state->app_context, sigint_cb, (XtPointer)rock);
    signal(SIGINT, sigint);

    /* Send initial notification */
    if (state->app_data.standalone) {
        send_initial_presence_info(state);
    }

    /* Reschedule timer */
    state->timer = XtAppAddTimeOut(state->app_context, 
                                   ACTIVE_DELAY * 1000, 
                                   timer_cb, 
                                   (XtPointer)state);
    return;
}


/* Main */

int main(int argc, char *argv[])
{
    struct state state;
    char host[64], addr[64], user[64], client_id[64];
    struct hostent *this_host;
    int xargc = argc;

    /* Initialize */
    state.toplevel = XtVaAppInitialize(&state.app_context,
                                       APP_CLASS,
                                       opt_table,
                                       XtNumber(opt_table),
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
    XtVaGetApplicationResources(state.toplevel,
                                &state.app_data,
                                resources,
                                XtNumber(resources),
                                NULL);
    /* Help */
    if (state.app_data.help == True) {
        printf("help\n");
        exit(0);
    }

    /* Version */
    if (state.app_data.version == True) {
        printf(VERSION "\n");
        exit(0);
    }

    /* Get host name */
    if (gethostname(host, 64) < 0) {
        fprintf(stderr, "Failed to get host name\n");
        strcpy(host, "<unknown>");
        strcpy(addr, "<unknown>");

    } else {
        /* Lookup trap host details */
        if (! (this_host = gethostbyname(host))) {
            fprintf(stderr, "Failed to get host address\n");
            strcpy(addr, "<unknown>");

        } else {
            if (! inet_ntop(AF_INET, this_host->h_addr_list[0], addr, 64)) {
                fprintf(stderr, "Failed to get convert host address\n");
                strcpy(addr, "<unknown>");
            }
        }
    }

    /* Construct text attributes */
    snprintf(user, 64, "%s@%s", state.app_data.user, state.app_data.domain);
    snprintf(client_id, 64, "%d%d", getpid(), getppid());

    /* Initialise state info */
    state.display = XtDisplay(state.toplevel);
    state.is_active = 0;
    state.have_dpms = 0;
    state.have_proc = 0;
    state.last_keyboard = 0;
    state.last_mouse = 0;
    state.last_change = time(NULL);
    state.host = host;
    state.addr = addr;
    state.user = user;
    state.client_id = client_id;

    /* Check whether we can get activity info */
    state.have_dpms = xipd_have_dpms(state.display);
    state.have_proc = xipd_have_proc();

    if (! state.have_dpms && ! state.have_proc) {
        fprintf(stderr, "Cannot use DPMS or /proc for activity detection.\n");
        exit(1);
    }

    /* Initialise Elvin */
#if ! defined(ELVIN_VERSION_AT_LEAST)
    if (! (state.error = elvin_xt_init(state.app_context))) {
        fprintf(stderr, "Failed to initialise Elvin library\n");
        exit(1);
    }
#else
    if (! (state.error = elvin_error_alloc(NULL))) {
        fprintf(stderr, "Failed to allocate error context!\n");
        exit(1);
    }

    if (! elvin_xt_init_default(state.app_context, state.error)) {
        fprintf(stderr, "Failed to initialise Elvin library\n");
        exit(1);
    }
#endif

    /* Create handle */
    if (! (state.handle = elvin_handle_alloc(state.error))) {
        elvin_error_fprintf(stderr, state.error);
        exit(1);
    }

    /* Un-limit connection retries */
    if (! elvin_handle_set_connection_retries(state.handle, -1, state.error)) {
        elvin_error_fprintf(stderr, state.error);
        exit(1);
    }

    /* Add router URL to handle */
    if (state.app_data.elvin) {
        if (! elvin_handle_append_url(state.handle, state.app_data.elvin, state.error)) {
            elvin_error_fprintf(stderr, state.error);
            exit(1);
        }
    }

    /* Connect to the server */
    if (! elvin_xt_connect(state.handle, connect_cb, (void *)&state, state.error)) {
        elvin_error_fprintf(stderr, state.error);
        exit(1);
    }

    /* Start mainloop */
    XtAppMainLoop(state.app_context);

    /* Just to keep gcc happy */
    return 1;
}
