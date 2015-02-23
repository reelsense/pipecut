bin_PROGRAMS = pipecut 
pipecut_SOURCES = pipecut.c pcDB.c pipecut.h queue.h 
pipecut_LDADD = sz-0.9.2/libsz.a 
# If using TRE, append the following to the line above: tre-0.8.0/lib/.libs/libtre.a
pipecutdir = $(destdir)

# Holdover from TRE development SUBDIRS = tre-0.8.0 sz-0.9.2
SUBDIRS = sz-0.9.2
AM_LDFLAGS = -Lsz-0.9.2 -lmenu -lcurses -lsqlite3 
AM_CFLAGS = $(DEPS_CFLAGS)
AM_LIBS = $(DEPS_LIBS)

