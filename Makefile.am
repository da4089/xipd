sbin_PROGRAMS = xipd

if !HAVE_GETOPT_LONG
GETOPT_C=getopt.c getopt1.c
else
GETOPT_C=
endif

xipd_SOURCES = $(GETOPT_C) dpms.c proc.c xipd.c 
man_MANS = xipd.8
EXTRA_DIST = $(man_MANS)

