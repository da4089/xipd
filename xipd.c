#include "config.h"

#if defined(HAVE_ERRNO_H)
# include <errno.h>
#endif

#if defined(HAVE_FCNTL_H)
# include <fcntl.h>
#endif

#if defined(HAVE_GETOPT_H)
# include <getopt.h>
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

#include <elvin/elvin.h>
#include <elvin/memory.h>
#include <elvin/sync_api.h>
#include <elvin/errors/elvin.h>
#include <elvin/errors/unix.h>
#include <elvin/utils.h>


#define PROC_BUFFER   (1024)
#define ACTIVE_DELAY  (300)


static elvin_error_t error;
static elvin_handle_t handle;
static elvin_notification_t nfn;



/* The list of long options */
static struct option long_options[] =
{
    { "elvin", required_argument, NULL, 'e' },
    { "scope", required_argument, NULL, 'S' },
    { "user", required_argument, NULL, 'u' },
    { "domain", required_argument, NULL, 'D' },
    { "location", required_argument, NULL, 'l' },
    { "groups", required_argument, NULL, 'g' },
    { "standalone", no_argument, NULL, 's' },
    { "debug", no_argument, NULL, 'd' },
    { "version", no_argument, NULL, 'v' },
    { "help", no_argument, NULL, 'h' },
    { NULL, no_argument, NULL, '\0' }
};



/* Prints out the usage message */
static void usage(int argc, char *argv[])
{
    fprintf(stderr, "Usage: %s [OPTION ...]\n", argv[0]);
    fprintf(stderr, "  -e elvin-url, --elvin=elvin-url\n");
    fprintf(stderr, "  -S scope,     --scope=scope\n");
    fprintf(stderr, "  -u user,      --user=user\n");
    fprintf(stderr, "  -D domain,    --domain=domain\n");
    fprintf(stderr, "  -l location,  --location=location\n");
    fprintf(stderr, "  -g groups,    --groups=group[,group...]\n");
    fprintf(stderr, "  -s,           --standalone\n");
    fprintf(stderr, "  -d,           --debug\n");
    fprintf(stderr, "  -v,           --version\n");
    fprintf(stderr, "  -h,           --help\n");
}


/* Read Linux' /proc/interrupts and extract keyboard/mouse counts */
int read_proc(long *keyboard, long *mouse)
{
    FILE *interrupts;
    char *s;
    int intr = 0;
    long count = 0;
    char *dev;

    if (! (interrupts = fopen("/proc/interrupts", "r"))) {
        return 0;
    }

    if (! (s = (char *)malloc(PROC_BUFFER))) {
        return 0;
    }

    if (! (dev = (char *)malloc(PROC_BUFFER))) {
        return 0;
    }

    do {
        if (! fgets(s, PROC_BUFFER, interrupts)) {
            break;
        }

        if (sscanf(s, " %d : %ld XT-PIC %s ", &intr, &count, dev)) {
            /*printf("intr %d\tcount %ld\tdev %s\n", intr, count, dev);*/

            if (strcmp("keyboard", dev) == 0) {
                *keyboard = count;
            }

            if (strcmp("PS/2", dev) == 0) {
                *mouse = count;
            }
        }
    } while (s);

    free(s);
    free(dev);
    fclose(interrupts);

    return 1;
}


/* Send notification of state change */
int send_event(int now_active)
{
    struct timeval tv;

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

    return 1;
}



int main(int argc, char *argv[])
{
    int	getopt_error = 0;
    char c;
    char host[64], addr[64], varbinds[8192];
    char *space, *cr;
    struct hostent *this_host;

    int is_active = 1;


    /* Initialise our error handle */
#if ! defined(ELVIN_VERSION_AT_LEAST)
    if (! (error = elvin_sync_init_default())) {
        fprintf(stderr, "Failed to initialise Elvin library\n");
        exit(1);
    }
#else
    if (! (error = elvin_error_alloc(NULL))) {
        fprintf(stderr, "Failed to allocate error context!\n");
        exit(1);
    }

    if (! elvin_sync_init_default(error)) {
        fprintf(stderr, "Failed to initialise Elvin library\n");
        exit(1);
    }
#endif

    if (! (handle = elvin_handle_alloc(error))) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }
    
    /* Process the arguments */
    optarg = NULL;
    while (!getopt_error && (-1 != (c = getopt_long(argc, 
                                                    argv, 
                                                    "e:S:u:d:l:g:sdvh",
                                                    long_options,
                                                    NULL)))) {
        switch(c) {

        case 'e':
            if (! elvin_handle_append_url(handle, optarg, error)) {
                elvin_error_fprintf(stderr, error);
                exit(1);
            }
            break;

        case 'S':
	    if (! elvin_handle_set_discovery_scope(handle, optarg, error)) {
		elvin_error_fprintf(stderr, error);
		exit(1);
	    }
            break;

        case 'v':
            fprintf(stdout, VERSION "\n");
            exit(0);
            break;

        default:
            getopt_error++;
        }
    }

    if (getopt_error) {
        usage(argc, argv);
        exit(1);
    }

    /* Unlimit connection retries */
    if (! elvin_handle_set_connection_retries(handle, -1, error)) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

    /* Connect to the server */
    if (! elvin_sync_connect(handle, error)) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

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


    /* */
    if (0) {


    } else {

        long keyboard, last_keyboard = 0;
        long mouse, last_mouse = 0;

        /* Loop, looking for change and no-change */
        while (1) {

            /* Get interrupt counts */
            if (! (read_proc(&keyboard, &mouse))) {
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

    if (! elvin_notification_free(nfn, error)) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

    /* Disconnect and clean up */
    if (! elvin_sync_disconnect(handle, error) ) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

    if (! elvin_handle_free(handle, error) || ! elvin_sync_cleanup(1, error)) {
        elvin_error_fprintf(stderr, error);
        exit(1);
    }

#if defined(DEBUG)
    elvin_memory_report();
#endif

    exit(0);
}
