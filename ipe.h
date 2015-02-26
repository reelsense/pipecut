// # vim: shiftwidth=4 tabstop=4 softtabstop=4 expandtab
// # indent: -bap -br -ce -ci4 -cli0 -d0 -di0 -i4 -ip -l79 -nbc -ncdb -ndj -ei -nfc1 -nlp -npcs -psl -sc -sob
// # Gnu indent: -bap -br -ce -ci4 -cli0 -d0 -di0 -i4 -ip -l79 -nbc -ncdb -ndj -ei -nfc1 -nlp -npcs -psl -sc -sob

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

/* This is the include header for building the 'pipecut' front-end. The curses
 * application that uses the libpipecut back end to provide a full-screen
 * pipeline editing experience
 */

#ifndef IPE_H
#define IPE_H

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
#include "sz-0.9.2/sz.h"

// Display routines
void printvisible(char *,int withHL, char *exp, int withLA, char *LAexp);
void clrtotop();

// Flag (mode) toggles
void toggleRe();
void toggleLA();
void togglenumbering();
void pc_togglecaches();

void newSummarize();
void pc_newCat(char *src);
void pc_newSTDIN();

// Global context structure for the frontend
struct ui_ctx {
	WINDOW *mainwin;
	int maxy;
	int maxx;
	int numbering; // Deprecated currently - 'cat -n' is a convenient alternative
	int statsthread;
} uigbl;

#endif

