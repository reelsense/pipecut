// # vim: shiftwidth=4 tabstop=4 softtabstop=4 expandtab
// # indent: -bap -br -ce -ci4 -cli0 -d0 -di0 -i4 -ip -l79 -nbc -ncdb -ndj -ei -nfc1 -nlp -npcs -psl -sc -sob
// # Gnu indent: -bap -br -ce -ci4 -cli0 -d0 -di0 -i4 -ip4 -l79 -nbc -ncdb -ndj -nfc1 -nlp -npcs -psl -sc -sob

/*
 * Copyright (c) 2014, David William Maxwell david_at_NetBSD_dot_org
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef PIPECUT_H
#define PIPECUT_H

#define PIPECUT_VERSION "0.5.0-alpha"

#define BLADECACHE 16000	// Needs to be dynamic - based on window size
			  // All references to char arrays[BLADECACHE] should become
			  // sz * pointers as all of the functions that reference them are converted
			  // to use the sz library calls.

#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <ctype.h>

#include <sys/wait.h>

#include <curses.h>
#include <menu.h>
#include <pthread.h>

// Development was done with both LIBTRE and native platform REGEX libs.
// TRE is not widely packaged at this time, so leaving this ifdef's out for now.
// We'll want to support an even wider variety of regex libs, at least on the codegen side,
// if not on the UI side.
// configure.am could test for availability, so label with config.h style define (that doesn't exist)
#ifdef HAVE_LIBTRE
#include "tre-0.8.0/lib/regex.h"
#include <tre.h>
#else
#include <regex.h>
#define REG_OK 0
#endif

#include <fcntl.h>
#include <sys/stat.h>

#include <limits.h>
#include "sz.h"

#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#else
#include "queue.h"		// Included copy of NetBSD's queue.h
#endif

// Wrappers for type conversion: Google search 'Making Wrong Code Look Wrong'/TypeFromType
#define SzFromStr(x) str2sz(x)
#define StrCopySz(x) szencode(x)
#define StrFromSz(x) szdata(x)

// Filter execution (in filter mode, and UI mode)
void fullrun(char lesspipe[BLADECACHE]);
void filterrun(char lesspipe[BLADECACHE]);
void runpipe(char *cmd, char tmpbuf[BLADECACHE]);

// Toolset -> text 
void updateTextPipeline(char pl[BLADECACHE], int script);

void lpc_newBB(char *cmd);
void lpc_newEX(char *excl);
void lpc_newIN(char *);
void lpc_newCat(char *src);

// lpc_ database persistance routines
void pc_loadToolset(int);	// Should be lpc, pending front/backend refactoring
void pc_saveToolset();

void regenCaches();

// Toolset AST manipulation routines
void lpc_removeTail();
void lpc_condprepend(char *prep);

// Libpipecut structures
enum lpc_pipestate {
    PNONE,			// At start of pipeline generation
    PIPE,			// Immediately following a pipeline
};
/* At the moment, the Mealy machine in lpc_pipe_transition only needs PNONE and PIPE.
 * Including these others causes compiler time warnings in switch statements that don't handle them.
        EGREP, // We have processed one or more EXCLUDEs, but could add more
        AWK, // We last processed an awk
*/

typedef enum lpc_pipestate Pipestate;

// Global context structure for the library
struct pipecut_ctx {
    int cacheon;
    int reon;
    int laon;
    int curs;			// State of UI rather than pipe.
    int fileoffset;
    int filepageend;
    int linecount;		// Populated by the stats thread
    int filtermode;		// When run with -t, set this flag, and store the toolset name in 'filter'
    int debug;
    char *filter;
    char tstext[BLADECACHE];	// XXX - size needs to be dynamic
    char tspart[BLADECACHE];	// XXX - size needs to be dynamic
    char sourcefile[PATH_MAX];
    struct toolelement *curBlade;
    struct toolelement *n1;
    struct toolelement *n2;
    struct toolelement *n3;
    struct toolelement *np;
} lpc_ctx;

enum tooltype {
    TNONE,			// Uninitialized tools shouldn't have a valid type.
    STDIN,			// Used only in filter mode (-t)
    CAT,			// Input file
    BLACKBOX,			// pipecut doesn't have an internal implementation of this blade
    EXCLUDE,			// grep -v
    FORMAT,			// field rearranging and formatted output
    INCLUDE,			// grep
    SUMMARIZE,			// wc
    ORDER,			// sort
    UNIQUE			// uniq
};

typedef enum tooltype Tooltype;

// Code generation function prototypes
void lpc_pipe_transition(Pipestate lpc_pipestate, Tooltype ttype, char *patt,
    char *pl, int script);

TAILQ_HEAD(tailhead, toolelement) head;
    struct tailhead *headp;	/* Tail queue head. */
    struct toolelement {
	TAILQ_ENTRY(toolelement) entries;	/* Tail queue. */
	Tooltype ttype;
	bool enabled;
	bool haseffect;		// IF this blade has no effect (e.g. cache = cache of prior blade), skip redraws etc.
	char *pattern;
	char *menuptr;
	char *menuline;
	int bladeoffset;
	int bladelen;
	char cache[BLADECACHE];	// XXX - size needs to be dynamic
	regex_t preg;
    };

#endif
