#include "config.h"

#if defined(HAVE_STDLIB_H)
# include <stdlib.h>
#endif

#if defined(HAVE_STDIO_H)
# include <stdio.h>
#endif

#if defined(HAVE_STRING_H)
# include <string.h>
#endif


#define PROC_BUFFER   (1024)


int xipd_have_proc(void)
{
    FILE *proc;
    int res = 0;

    proc = fopen("/proc/interrupts", "r");
    if (proc) {
        fclose(proc);
        res = 1;
    }

    return res;
}
    

/* Read Linux' /proc/interrupts and extract keyboard/mouse counts */
int xipd_read_proc(long *keyboard, long *mouse)
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


