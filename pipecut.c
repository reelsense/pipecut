// # vim: shiftwidth=4 tabstop=4 softtabstop=4 expandtab
// # indent: -bap -br -ce -ci4 -cli0 -d0 -di0 -i4 -ip -l79 -nbc -ncdb -ndj -ei -nfc1 -nlp -npcs -psl -sc -sob
// # Gnu indent: -bap -nbad -br -ce -ci4 -cli0 -d0 -di0 -i4 -ip4 -l79 -nbc -ncdb -ndj -nfc1 -nlp -npcs -psl -sc -sob

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

/* This is the main source file for the pipecut front-end, interactive pipeline editor.
 * It uses libpipecut backend functions (which are still currently intermingled in this file,
 * but which will be split out)
 */

/* Naming conventions
 * Throughout the pipecut source code, the following naming conventions apply.
 * lpc_   Function designated to part of libpipecut. Must not refer to uigbl. Must not use
 *        curses functions
 *        or write tty output, except for major warnings / fatal errors to stderr.
 *
 * pc_    Data structures that are part of the pipecut frontend.  Note that functions
 *        in the frontend don't have to be labelled with any prefix.
 * 
 * other  Anything that asks for user input, prints output, or uses curses.
 *
 * The actual separation of libpipecut has not happened yet. UI elements from the proof of
 * concept are still being teased out of the code and split from the lpc_ functions.
 */

/* Development notes
 * The primary author of this code has not developed using the curses library before.
 * As a result, I'm sure I've made incorrect assumptions and non-portable implementation
 * choices.
 *
 * Review and patches from knowledgable curses programmers are welcome.
 */

#ifdef HAVE_BSD_STRING_H	// If we're on a BSD platform, strl* functions will be in string.h (in pipecut.h below)
#include <bsd/string.h>		// Required on Linux platforms - from bsd-dev package
#endif

// Macro for curses debugging. Should be off in production builds.
#define Q "";
//#define Q curs_set(1); refresh();

#include "pipecut.h"		// libpipecut backend include file
#include "ipe.h"		// Interactive pipeline editor - front-end include file
#include "pcDB.h"		// Database routines that will move to the back

struct termios oldt, newt;

char lpc_toolcmds[22][20] = {	// XXX Move this to the lpc library sourcee when it splits.
    "NULL '", "' NULL",
    "cat ", "",
    " | ", "",
    "BLACKBOX '", "",
    "egrep -v '", "'",
    "awk '{print \"", "\\n\"}'",
    "egrep '", "'",
    "wc ", "",
    "sort ", "",
    "uniq ", "",
    "tr ", "",
};

// UI Functions
void newBBox();			// Input a Blackbox literal from user
void editBlade();
void removeMid();
void updateStatus();

void terminalraw();
void terminalnormal();		// XXX Unused
void print_in_middle(WINDOW * win, int starty, int startx, int width, char *string);	// XXX Unused
void displayfilepage(int redraw, char *exp);
void start_background_thread(char *av1);
void dumprulefile();
void listExclude();
void toggleCurs();
void usage(char *av0) __attribute__ ((noreturn));
void version() __attribute__ ((noreturn));
void helpscreen();
void menu();

// Globals
// pthread_mutex_t linecount_mutex = PTHREAD_MUTEX_INITIALIZER;

char inputpipe[16384];

// Flags:
int parse_from_pipe = 0;

// libpipecut Functions
void pc_init(struct pipecut_ctx *ctx);	// Initialize Context
// Execution of functions
int pc_wc_w(char *str);		// WC - wordcount words
// Front-end instantiator functions for various blade types 
void newExclude();
void newInclude();
void newAwk();
void pc_newSTDIN();
void pc_newTranslate();

// History parser
void pc_text2ts(char *);

/* Database usage model: toolsets sit in DB. Load command asks for a toolset name,
 * Should also be able to bring up list of toolsets in DB.
 * (Save / discard existing toolset? Keep modification state? )
 * (Toolset name on status line? Modified state on status line?
 *  Save command asks for name.
 *  Ask for save on exit? Different quit methods?
 *	Initialize DB at startup if not found.
*/

// XXX Consider a TOOLINIT macro - not sure whether it would be used often enough.

struct thread_info {		/* Used as argument to thread_start() */
    pthread_t thread_id;	/* ID returned by pthread_create() */
    int thread_num;		/* Application-defined thread # */
    char *argv_string;		/* From command-line argument */
};

void bladeAction(struct toolelement *blade, char tmpbuf[BLADECACHE]);

/*
 * NAME:
 * pipecut is an alternative way to build up a UNIX shell command pipeline.
 * Instead of alternatinv between adding commands and testing output, pipecut allows you
 * to do this interactively, and provides functionality that can't be done on a
 * command line.
 *
 * ORIGINS:
 * pipecut was written to provide a framework to implement one concept originally - 
 * a line-oriented common pattern extractor. The pattern extractor's purpose is
 * to analyze sample text, and identify common elements that share the same 'skeleton'
 * but have different expressions as malleable fields within the line are changed.
 * For example given these two lines:
 * -rwxrwxr-x  1 david david 35063 May 23 15:30 pipecut
 * -rw-rw-r--  1 david david 15808 May 23 15:30 pipecut.c
 * 
 * The two lines have the same number of fields (though this wouldn't be true for all 'ls'
 * sample output) and have whitespace in the same column locations on each line. Some of the
 * fields are the same in both lines, some are different, and (given a sufficient sample set)
 * some fields have a limited character set. Many interesting actions can be performed on
 * this data (or on larger sample sets).
 *
 * We can extract a regular expression that matches all input lines with the same skeleton:
 * -rw[-x]rw[-x]r-[-x]  1 david david [13][35][08][60][38] May 23 15:30 pipecut(.c)?
 *
 * With some generalization rules, provided either by recognition of the skeleton type, or
 * through analyzing sufficient sample input, or by the user providing guidance, this
 * could become:
 *
 * [-lsp][-rwx][-rwxs][-rwxs] [0-9]+ [a-zA-Z0-9]+ [a-zA-Z0-9]+ [0-9]+ [A-Z][a-z][a-z] [0-9]+ 15:30 pipecut(.c)?
 * XXX
 *
 * OVERVIEW:
 * This full-screen curses utility reads a file and displays a sample of lines from it.
 * The user can then create a list of rules that modify the file data in ways that 
 * correspond to common manipulations needed in typical information formatting challenges.
 * 
 * The file is not modified in-place. As additional pipecut modifiers are added, the
 * resulting output is displyed interactively. Since each of the pipecut modifiers
 * corresponds to common UNIX command line utilities, a set of pipecut modifiers can be
 * output in a variety of programming lanaguages for the purpose of rapid-prototyping,
 * comparative performance analysis, or maintaining the modifiers in a language-abstract
 * way so that the implementation can migrate between languages as needed.
 *
 * Implemented output formats: shell script
 * Planned output formats: perl, python, C
 *
 * Pipecut can also be run with command line arguments (-t) to execute a pipecut
 * toolset and process
 * the provided input through a set of modifiers and output the result. This allows you
 * to save a particular transformation and reuse it, or share it with others.
 *
 * pipecut can also be run with a toolname, and the user can interactively single-step
 * through applying additional layers of modifiers. You can think of this like a
 * pipeline debugger, or the application of a photo-editing layer visibility functionality
 * to a text processing environment.
 *
 * XXX update text below for exclusion -> generic tool change
 * Implementation: exclusion rules are maintained as elements of a TAILQ.
 *   A second thread calculates statistics about the target file, and the impact of the
 *   various exclusion rules. The statistics are updated in the background while the
 *   user interacts with the main event loop. Since the interface between the threads
 *   is not performance critical, and the statistics thread calculates into temporary
 *   variables and does the update in a small crtitical section, simple mutexes are used
 *   to protect the shared variables.
*/

int
main(int argc, char *argv[])
{
    int c;
    int dorefresh = 0;
    char l1[16384];		// XXX Don't need such large blocks on the stack - move to the heap.
    char l2[16384];
    char l3[16384];
    char *cp;
    FILE *fp;
    int ch;

    char lesspipe[BLADECACHE];	// XXX fixed size bad
    int k;
    lpc_ctx.debug = 0;

/* Handle the case of piped input */

    struct stat stats;

//sleep(10); // Give gdb a chance to attach

// Check command line arguments
    while ((ch = getopt(argc, argv, "ht:v")) != -1) {
	switch (ch) {

	case 'h':
	    usage(NULL);
	    break;
	case 't':
	    lpc_ctx.filtermode = 1;
	    lpc_ctx.filter = optarg;
	    break;
	case 'v':
	    version();
	    break;
	case '?':
	    usage(NULL);
	    break;
	default:
	    break;
	}
    }
    argc -= optind;
    argv += optind;

/* Would like to check for DB early - but we can't interact with the user until we know
   the mode we're running in. So get past those checks first. */

/* Figure out whether we've started running with piped input, or a terminal */
    int r = fstat(fileno(stdin), &stats);

    if (r) {
	printf("fstat of STDIN returned an error. Exiting.\n");
	exit(-1);
    }
    // Initialize libpipecut context structure
    pc_init(&lpc_ctx);

    // This stanza is only for debugging printfs, and a reminder of the other way to
    // identify our input type. We use the stats structure below to decide operational mode.
    if (isatty(fileno(stdin))) {
	if (lpc_ctx.debug)
	    fprintf(stderr, "stdin is a terminal\n");
    } else {
	if (lpc_ctx.debug)
	    fprintf(stderr, "stdin is a file or a pipe\n");
    }

    // Initialize the tool list
    TAILQ_INIT(&head);

    if (lpc_ctx.filtermode != 1) {
	uigbl.mainwin = initscr();	// Start curses mode
    }
    initDB(0);			// Setup Database (debugging off)

    if (S_ISFIFO(stats.st_mode)) {
	if (lpc_ctx.debug)
	    fprintf(stderr, "stdin is a pipeline\n");

	// Input is a pipeline. Either we're to consume command history, or run as a filter.
	// -t (lpc_ctx.filtermode) will tell us which.

	if (lpc_ctx.filtermode) {	// -t, load the toolset, process, and exit.
	    pc_loadToolset(2);	// 2 for filter mode
	    updateTextPipeline(lpc_ctx.tstext, 0);	// Prep the pipeline from the toolset
	    lpc_ctx.tstext[0] = ' ';	// XXX Cheesy hack to avoid rewriting the way a blade's text representation is generated
	    filterrun(lpc_ctx.tstext);
	    // Doit
	    exit(0);
	}
	//Test that it looks like a command history and consume the last
	// pipeline
	while (1) {
	    strcpy(l3, l2);
	    strcpy(l2, l1);
	    if (!fgets(l1, 16384, stdin)) {
		printf("fgets returned NULL\n");
	    }
	    //fscanf(stdin,"%s",l1);

	    if (feof(stdin)) {
		printf("End of stdin\n");
		break;
	    }
	}
	// Get our stdin pointed back to the user.
	fclose(stdin);
	freopen("/dev/tty", "r", stdin);

	printf("L1: %s\n", l1);
	printf("L2: %s\n", l2);
	printf("L3: %s\n", l3);

	// In some shells (tcsh, csh) the history format is:
	//  # hh:mm  cmd
	// And the last line in the history will be the executed command that ran pipecut - so we want the
	// command before that (l3)

	// In other shells (bash, zsh), the time is omitted. bash includes the history command, zsh omits it.
	// XXX Ignore the zsh case, and handle l3 for now.
	strcpy(inputpipe, l3);
	// Flag that we came from a pipe - we'll later need to open the input on the pipeline, instead of ARGV[1]
	parse_from_pipe = 1;

    } else {
	// Input is not a pipeline, so assume we're in interactive mode.

	/* We didn't get a pipe on input - so make sure we got a filename argument. */
	if (argc < 1) {
	    // XXX End curses, print error.
	    usage(argv[0]);
	    exit(-1);
	}
    }

    // setup Color
    start_color();		/* Start color                  */
    use_default_colors();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, -1, -1);
    attron(COLOR_PAIR(2));
    cbreak();
    noecho();
    erase();
    // find out the screen size
    uigbl.maxx = getmaxx(uigbl.mainwin);
    uigbl.maxy = getmaxy(uigbl.mainwin);
    uigbl.numbering = 0;
    uigbl.statsthread = 0;

    if (!parse_from_pipe) {
	// In the history|pipecut case, we already have the source filename.
	strlcpy(lpc_ctx.sourcefile, argv[0], PATH_MAX);
    } else {
	printf("Calling pc_text2ts %s\n", inputpipe);
	pc_text2ts(inputpipe);
    }

    // Must happen after we get maxy and maxx - lpc_newCat used to use them to size the buffer
    // That won't be an issue any more with the use of an sz object.
    fp = fopen(lpc_ctx.sourcefile, "r");
    if (!fp) {
	endwin();
	printf("Could not open file %s\n", lpc_ctx.sourcefile);
	exit(-1);
    }
    // Must know the filename before the lpc_newCat
    if (!parse_from_pipe) {
	lpc_newCat(lpc_ctx.sourcefile);
    }

    terminalraw();

    // XXX This is disabled for now. needs more portability and build testing on a variety of platforms
    // Spin off a thread to build some statistics (libmoremagic, etc)
    //start_background_thread(lpc_ctx.sourcefile);

    displayfilepage(1, NULL);

/* 
 * Items prefixed with X are not yet implemented
 * In the main event loop, the user can perform the following actions:
 * Add/modify/delete pipecut modifiers (grep, s///, awk, tr, sort)
 * X: Edit the pattern of an existing blade
 * X: Export the current pipecut set into a single-tool box, for emailing to another user
 * X: Import a pipecut single-tool, or toolbox into the user's toolbox

 */
    // Process Commands - main UI event loop starts here
    keypad(uigbl.mainwin, 1);	// Handle Esc sequences for us, thank you.
    timeout(-1);
    while ((c = getch()) != 'q') {
	// mvprintw(uigbl.maxy-3,0,"GETCH:%d:",c); // For debugging.
	if (c == '\x12') {	/* Control-R */
	    //erase();
	    move(uigbl.maxy, 0);
	    for (k = 0; k < uigbl.maxy; k++) {
		printf("\n\n");
	    }
	    move(0, 0);
	    clrtotop();
	    refresh();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == '\x08' || /* c == '\x7f' || */ c == KEY_BACKSPACE) {	/* Backspace or delete */
	    if (TAILQ_PREV(lpc_ctx.curBlade, tailhead, entries) != NULL) {
		lpc_removeTail();
		displayfilepage(1, NULL);
	    }
	    continue;
	}
	if (c == 'd') {
	    dumprulefile();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'P') {
	    toggleCurs();
	    continue;
	}
	if (c == 'R') {
	    toggleRe();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == '-') {		// Modify the existing blade (iff Blackbox)
	    if (lpc_ctx.curBlade->ttype != BLACKBOX) {
		beep();
		continue;
	    }
	    c = getch();
	    if (lpc_ctx.curBlade->pattern)
		cp = strchr(lpc_ctx.curBlade->pattern, ' ');
	    if (!cp) {
		realloc(lpc_ctx.curBlade->pattern,
		    strlen(lpc_ctx.curBlade->pattern) + 4);
		snprintf(l3, strlen(lpc_ctx.curBlade->pattern) + 4, "%s -%c",
		    lpc_ctx.curBlade->pattern, c);
		strcpy(lpc_ctx.curBlade->pattern, l3);
		regenCaches();
		displayfilepage(1, NULL);
	    } else {
		realloc(lpc_ctx.curBlade->pattern,
		    strlen(lpc_ctx.curBlade->pattern) + 2);
		snprintf(l3, strlen(lpc_ctx.curBlade->pattern) + 2, "%s%c",
		    lpc_ctx.curBlade->pattern, c);
		strcpy(lpc_ctx.curBlade->pattern, l3);
		regenCaches();
		displayfilepage(1, NULL);
	    }
	    continue;
	}
	if (c == '|') {
	    strcpy(lesspipe, lpc_ctx.tspart);
	    strcat(lesspipe, " | less");
	    fullrun(lesspipe);
	    continue;
	}
	if (c == 'L') {
	    toggleLA();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'C') {
	    pc_togglecaches();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == '!') {
	    FILE *so;
	    char scriptout[BLADECACHE];
	    updateTextPipeline(scriptout, 1);
	    so = fopen("script.sh", "w");
	    fprintf(so, "%s", scriptout);
	    fclose(so);
	}
	if (c == 'c') {
	    lpc_newBB("cat -n");
	    displayfilepage(1, NULL);
	    continue;
	}
// XXX TODO     if (c == 'e') { editBlade(); displayfilepage(1,NULL); continue; }
	if (c == 'h') {
	    lpc_newBB("hexdump -C");
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 's') {
	    lpc_newBB("sort");
	    displayfilepage(1, NULL);
	    continue;
	}
// XXX TODO     if (c == 't') { pc_newTranslate(); displayfilepage(1,NULL); continue; }
	if (c == 'u') {
	    lpc_condprepend("sort");
	    lpc_newBB("uniq");
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'U') {
	    lpc_condprepend("sort");
	    lpc_newBB("uniq -c");
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'H') {
	    lpc_newBB("sort -nr");
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'A') {
	    lpc_newBB("awk '{print $8, $9}'");
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == '?') {
	    helpscreen();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == '[') {
	    pc_loadToolset(0);	// 0 to replace
	    updateTextPipeline(lpc_ctx.tstext, 0);	// Rewrite the pipeline text from the toolset
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == '"') {
	    pc_loadToolset(1);	// 1 to append
	    updateTextPipeline(lpc_ctx.tstext, 0);	// Rewrite the pipeline text from the toolset
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == ']') {
	    pc_saveToolset();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'l') {
	    listExclude();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'm') {
	    menu();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'n') {		// Currently deprecated. use 'c' (cat -n)
	    togglenumbering();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'g') {
	    newInclude();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'w') {
	    newSummarize();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'x') {
	    newExclude();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == '\'') {
	    newBBox();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'a') {
	    newAwk();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == 'r') {
	    regenCaches();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == KEY_DC) {	// DC -> Delete Character -> DELETE
	    removeMid();
	    displayfilepage(1, NULL);
	    continue;
	}
	if (c == KEY_LEFT) {
	    mvwchgat(uigbl.mainwin, uigbl.maxy - 2,
		lpc_ctx.curBlade->bladeoffset + 2,
		lpc_ctx.curBlade->bladelen - 2, A_NORMAL, 0, NULL);
	    dorefresh = lpc_ctx.curBlade->haseffect;
	    lpc_ctx.n1 = TAILQ_PREV(lpc_ctx.curBlade, tailhead, entries);
	    if (lpc_ctx.n1) {
		lpc_ctx.curBlade = lpc_ctx.n1;
	    }			// Can't go left of the first blade
	    // XXX Optimization to add here - if secondblade !haseffect, no need to redraw when moving right
	    if (dorefresh)
		displayfilepage(1, NULL);
	    else
		displayfilepage(1, NULL);	// second was 0
	}
	if (c == KEY_RIGHT) {
	    mvwchgat(uigbl.mainwin, uigbl.maxy - 2,
		lpc_ctx.curBlade->bladeoffset + 2,
		lpc_ctx.curBlade->bladelen - 2, A_NORMAL, (short)0, NULL);
	    lpc_ctx.n1 = TAILQ_NEXT(lpc_ctx.curBlade, entries);
	    if (lpc_ctx.n1) {
		lpc_ctx.curBlade = lpc_ctx.n1;
	    }			// Can't go right of the last blade
	    dorefresh = lpc_ctx.curBlade->haseffect;
	    // XXX Optimization to add here - if secondblade !haseffect, no need to redraw when moving left
	    if (dorefresh)
		displayfilepage(1, NULL);
	    else
		displayfilepage(1, NULL);
	}
	if (c == KEY_PPAGE) {
	    lpc_ctx.fileoffset = 0;
	    regenCaches();
	    displayfilepage(1, NULL);
	}
	if (c == KEY_NPAGE) {
	    lpc_ctx.fileoffset = lpc_ctx.filepageend;	// Move the seek point to just past where we've been displaying
	    regenCaches();
	    displayfilepage(1, NULL);
	}
	// We shouldn't have to decode escape sequences manually, but
	// I'm leaving this here until I know I don't need to abuse keyok()
	if (c == 27) {		// Escape character may begin an escape sequence.
	    c = getch();
	    if (c == '[') {
		c = getch();
		if (c == '3') {
		    c = getch();
		    if (c == '~') {	// Delete
			removeMid();
			displayfilepage(1, NULL);
			continue;
		    }
		}
		if (c == 'A') {	// up arrow
		    //dothing();
		}
		if (c == 'B') {	// down arrow
		    //dothing();
		}
		if (c == 'C') {	// right arrow
		    mvwchgat(uigbl.mainwin, uigbl.maxy - 2,
			lpc_ctx.curBlade->bladeoffset + 2,
			lpc_ctx.curBlade->bladelen - 2, A_NORMAL, (short)0,
			NULL);
		    dorefresh = lpc_ctx.curBlade->haseffect;
		    lpc_ctx.n1 = TAILQ_NEXT(lpc_ctx.curBlade, entries);
		    if (lpc_ctx.n1) {
			lpc_ctx.curBlade = lpc_ctx.n1;
		    }		// Can't go right of the last blade
		    // XXX Optimization to add here - if secondblade !haseffect, no need to redraw when moving right
		    if (dorefresh)
			displayfilepage(1, NULL);
		    else
			displayfilepage(1, NULL);
		}
		if (c == 'D') {	// left arrow
		    mvwchgat(uigbl.mainwin, uigbl.maxy - 2,
			lpc_ctx.curBlade->bladeoffset + 2,
			lpc_ctx.curBlade->bladelen - 2, A_NORMAL, (short)0,
			NULL);
		    lpc_ctx.n1 =
			TAILQ_PREV(lpc_ctx.curBlade, tailhead, entries);
		    if (lpc_ctx.n1) {
			lpc_ctx.curBlade = lpc_ctx.n1;
		    }		// Can't go left of the first blade
		    dorefresh = lpc_ctx.curBlade->haseffect;
		    // XXX Optimization to add here - if secondblade !haseffect, no need to redraw when moving left
		    if (dorefresh)
			displayfilepage(1, NULL);
		    else
			displayfilepage(1, NULL);
		}
		if (c == '5') {
		    c = getch();
		    if (c == '~') {	// PageUP
			lpc_ctx.fileoffset = 0;
			regenCaches();
			displayfilepage(1, NULL);
		    }
		}
		if (c == '6') {
		    c = getch();
		    if (c == '~') {	// PageDn
			lpc_ctx.fileoffset = lpc_ctx.filepageend;	// Move the seek point to just past where we've been displaying
			regenCaches();
			displayfilepage(1, NULL);
		    }
		}
	    }
	    // Since not all escape sequences are explicitly handled, throw away any
	    // unconsumed input at this point.
	    flushinp();
	}
	//if (c > 'a' && c < 'z' && c!= 'x') putchar(c);                 
	//if (c > '0' && c < '9') putchar(c);                 
    }

    printf("\n");
    endwin();			/* End curses mode                */

    return 0;

}

void
togglenumbering()
{				// Currently deprecated.
    uigbl.numbering++;
    if (uigbl.numbering > 2) {	// valid values are 0 (no numbering) 1 (current numbering), 2 (source numbering)
	uigbl.numbering = 0;
    }
    return;
}

void
toggleCurs()
{
    lpc_ctx.curs++;
    if (lpc_ctx.curs > 2) {	// valid values are 0 (hide) 1 (low vis ), 2 (high vis )
	lpc_ctx.curs = 0;
    }
    curs_set(lpc_ctx.curs);
    return;
}

void
toggleRe()
{
    lpc_ctx.reon ^= 1;
    return;
}

void
toggleLA()
{
    lpc_ctx.laon ^= 1;
    return;
}

void
pc_togglecaches()
{
    lpc_ctx.cacheon ^= 1;
    return;
}

void
dumprulefile()
{

    FILE *fp;

    fp = fopen("rulefile.out", "w");
    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
	fprintf(fp, "action NAMEXXX severity:XXX __%s_ type:%d store:XXX:$1 alert:template/foo.template\n", lpc_ctx.np->pattern, lpc_ctx.np->ttype);	// XXX need to humanize ttype
    }

    fclose(fp);
}

void
newSummarize()
{
    char *ma;

    printw("| wc ");
    refresh();

/* Insert the new entry into the toolset list */
    lpc_ctx.n1 = malloc(sizeof(struct toolelement));	/* Insert at the head. */
    memset(lpc_ctx.n1->cache, 0, BLADECACHE);
    ma = malloc(1);		// Although Summarize has no 'pattern' - initialize a null string so that code everywhere else doesn't need special cases.
    strcpy(ma, "");
    lpc_ctx.n1->enabled = 1;
    lpc_ctx.n1->pattern = ma;
    lpc_ctx.n1->ttype = SUMMARIZE;
    lpc_ctx.n1->menuptr = NULL;	// We don't use this until the menu is called. NULL it to a known state now.

    TAILQ_INSERT_TAIL(&head, lpc_ctx.n1, entries);

    lpc_ctx.curBlade = lpc_ctx.n1;	// Current Blade follows the newly created Blade.

    return;
}

void
newInclude()
{
    char incl[1024];		// XXX Not okay to use fixed length field
    char blade[200];		// XXX Not okay to use fixed length field
    char *cp;
    char *ma;
    int rc, nlen;
    int c;
    int y, x;
    cp = incl;
    sprintf(blade, "| egrep '");
    mvprintw(uigbl.maxy - 2, strlen(lpc_ctx.tstext), blade);	// on NetBSD, this could just be printw'd. Curses inconsistency between platforms
    refresh();
    // We have two modes of input. In the noRe case, getstr does all the work for us. In the Re case, we have to do it.
    if (!lpc_ctx.reon) {
	echo();
	curs_set(1);
	//getstr(cp);
	getnstr(cp, 1024);
    } else {
	while (1) {		// XXX Need to rework the pointer handling here.
	    c = getch();
	    if (c == '\n') {
		break;
	    }
	    *(cp++) = (char)c;	// Accumulate characters.
	    if (c == KEY_BACKSPACE || c == '\x08' || c == '\x7f') {
		if (cp > incl + 1) {	// No backspacing past the start of the string
		    printw(" ");
		    move(getcury(uigbl.mainwin), getcurx(uigbl.mainwin) - 1);
		    *cp = '\0';	// Always leave a clean tail on the string.
		    cp--;
		    *cp = '\0';
		    cp--;
		    *cp = '\0';
		    curs_set(0);
		    getyx(uigbl.mainwin, y, x);
		    displayfilepage(0, incl);
		    move(y, x);
		    curs_set(1);
		} else {
		    *cp = '\0';
		    cp--;
		    *cp = '\0';
		}
	    } else {
		printw("%c", *(cp - 1));	// print, since echo is off.
		*cp = '\0';
		curs_set(0);
		// This is where we update the display for interactive behavior
		// like Regular Expression mode.
		// Record where the cursor is, and restore it afterwards,
		// since displayfilepage doesn't do so for us.
		getyx(uigbl.mainwin, y, x);
		displayfilepage(0, incl);	// No redraw, so we don't overwrite incl
		move(y, x);
		curs_set(1);
	    }
	}
	*cp = '\0';
	cp--;
    }
    curs_set(0);
    noecho();
    //printw("\nNew Inclusion is:%s\n", incl);
    refresh();

/* XXX This check applies in the single-tool case, within a range of toolelements. e.g. don't grep-v the same
   string out twice, with no other changes in betwen.
   XXX The code needs to be updated to reflect that. */
/* Check that the new entry is not a duplicate of an existing entry */
    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
	if (!strcmp(incl, lpc_ctx.np->pattern)) {
	    printw
		("WARNING: The supplied value is a duplicate of an existing inclusion (not added).\n");
	    printw("\nHit any key to continue.\n");
	    getch();
	    return;
	}
    }

    if (!strncmp(incl, "", 1024)) {
	// Empty string? Just return without adding.
	return;
    }
    lpc_newIN(incl);
    return;

}

void
newExclude()
{
    char excl[200];		// XXX Not okay to use fixed length field
    char blade[200];		// XXX Not okay to use fixed length field
    char *cp;
    char *ma;
    int rc, nlen;
    int y, x;
    int c;
    cp = excl;
    echo();
    getyx(uigbl.mainwin, y, x);
    sprintf(blade, "| egrep -v '");
    mvprintw(uigbl.maxy - 2, strlen(lpc_ctx.tstext), blade);	// on NetBSD, this could just be printw'd. Curses inconsistency between platforms
    refresh();
    // We have two modes of input. In the noRe case, getstr does all the work for us. In the Re case, we have to do it.
    if (!lpc_ctx.reon) {
	echo();
	curs_set(1);
	getstr(cp);
    } else {
	noecho();
	curs_set(1);
	while (1) {		// XXX Need to rework the pointer handling here.
	    c = getch();
	    if (c == '\n') {
		break;
	    }
	    *(cp++) = (char)c;	// Accumulate characters.
	    if (c == KEY_BACKSPACE || c == '\x08' || c == '\x7f') {
		if (cp > excl + 1) {	// No backspacing past the start of the string
		    printw(" ");
		    move(getcury(uigbl.mainwin), getcurx(uigbl.mainwin) - 1);
		    *cp = '\0';	// Always leave a clean tail on the string.
		    cp--;
		    *cp = '\0';
		    cp--;
		    *cp = '\0';
		    curs_set(0);
		    getyx(uigbl.mainwin, y, x);
		    displayfilepage(0, excl);
		    move(y, x);
		    curs_set(1);
		} else {
		    *cp = '\0';
		    cp--;
		    *cp = '\0';
		}
	    } else {
		printw("%c", *(cp - 1));	// print, since echo is off.
		*cp = '\0';
		curs_set(0);
		// This is where we update the display for interactive behavior
		// like Regular Expression mode.
		// Record where the cursor is, and restore it afterwards,
		// since displayfilepage doesn't do so for us.
		getyx(uigbl.mainwin, y, x);
		displayfilepage(0, excl);	// No redraw, so we don't overwrite excl
		move(y, x);
		curs_set(1);
	    }
	}
	*cp = '\0';
	cp--;
    }
    curs_set(1);
    noecho();
    //printw("\nNew Exclusion is:%s\n", excl);
    refresh();

/* XXX This check applies in the single-tool case, within a range of toolelements. e.g. don't grep-v the same
   string out twice, with no other changes in betwen.
   XXX The code needs to be updated to reflect that. */
/* Check that the new entry is not a duplicate of an existing entry */
    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
	if (!strcmp(excl, lpc_ctx.np->pattern)) {
	    printw
		("WARNING: The supplied value is a duplicate of an existing exclusion (not added).\n");
	    printw("\nHit any key to continue.\n");
	    getch();
	    return;
	}
    }

    if (!strncmp(excl, "", 1024)) {
	// Empty string? Just return without adding.
	return;
    }
    lpc_newEX(excl);
    return;
}

void
lpc_newEX(char *excl)
{
    int rc, nlen;
    char *ma;

/* Insert the new entry into the toolset list */
    lpc_ctx.n1 = malloc(sizeof(struct toolelement));	/* Insert at the head. */
    memset(lpc_ctx.n1->cache, 0, BLADECACHE);
    nlen = (strlen(excl) + 1);
    ma = malloc(nlen);
    lpc_ctx.n1->enabled = 1;
    lpc_ctx.n1->pattern = ma;
    lpc_ctx.n1->ttype = EXCLUDE;
    lpc_ctx.n1->menuptr = NULL;	// We don't use this until the menu is called. NULL it to a known state now.
    strlcpy(lpc_ctx.n1->pattern, excl, nlen);	// Copy excl into new list entry
    rc = regcomp(&lpc_ctx.n1->preg, excl, REG_EXTENDED);
    if (rc) {
	endwin();
	printf("Regex compile failed %s = %d\n", excl, rc);
	printf
	    ("This has been observed when you include control characters in your\n"
	    "regular expression. If this is not the case, try your expression again\n"
	    "and report a bug if it doesn't have control characters, but still fails to compile.\n");

	exit(-1);
    }

    TAILQ_INSERT_TAIL(&head, lpc_ctx.n1, entries);

    lpc_ctx.curBlade = lpc_ctx.n1;	// Current Blade follows the newly created Blade.

    return;
}

void
lpc_newIN(char *excl)
{
    int rc, nlen;
    char *ma;

/* Insert the new entry into the toolset list */
    lpc_ctx.n1 = malloc(sizeof(struct toolelement));	/* Insert at the head. */
    memset(lpc_ctx.n1->cache, 0, BLADECACHE);
    nlen = (strlen(excl) + 1);
    ma = malloc(nlen);
    lpc_ctx.n1->enabled = 1;
    lpc_ctx.n1->pattern = ma;
    lpc_ctx.n1->ttype = INCLUDE;
    lpc_ctx.n1->menuptr = NULL;	// We don't use this until the menu is called. NULL it to a known state now.
    strlcpy(lpc_ctx.n1->pattern, excl, nlen);	// Copy excl into new list entry
    rc = regcomp(&lpc_ctx.n1->preg, excl, REG_EXTENDED);
    if (rc) {
	endwin();
	printf("Regex compile failed %s = %d\n", excl, rc);
	printf
	    ("This has been observed when you include control characters in your\n"
	    "regular expression. If this is not the case, try your expression again\n"
	    "and report a bug if it doesn't have control characters, but still fails to compile.\n");

	exit(-1);
    }

    TAILQ_INSERT_TAIL(&head, lpc_ctx.n1, entries);

    lpc_ctx.curBlade = lpc_ctx.n1;	// Current Blade follows the newly created Blade.

    return;
}

void
lpc_newCat(char *file)
{
    char *ma;
    int nlen;

    lpc_ctx.n1 = malloc(sizeof(struct toolelement));	/* Insert at the head. */
    memset(lpc_ctx.n1->cache, 0, BLADECACHE);
    nlen = (strlen(file) + 1);
    ma = malloc(nlen);
    lpc_ctx.n1->haseffect = 1;
    lpc_ctx.n1->bladeoffset = -2;
    lpc_ctx.n1->bladelen = 7 + strlen(file);
    lpc_ctx.n1->enabled = 1;
    lpc_ctx.n1->pattern = ma;
    lpc_ctx.n1->ttype = CAT;
    lpc_ctx.n1->menuptr = NULL;	// We don't use this until the menu is called. NULL it to a known state now.
    strlcpy(lpc_ctx.n1->pattern, file, nlen);	// Copy filename into new list entry // XXX leaking 'ma'?
    TAILQ_INSERT_TAIL(&head, lpc_ctx.n1, entries);

    lpc_ctx.curBlade = lpc_ctx.n1;	// Current Blade follows the newly created Blade.

    return;

}

void
pc_newSTDIN()
{
    char *ma;
    int nlen;

    lpc_ctx.n1 = malloc(sizeof(struct toolelement));	/* Insert at the head. */
    memset(lpc_ctx.n1->cache, 0, BLADECACHE);
    nlen = 1;
    ma = malloc(nlen);
    *ma = '\0';
    lpc_ctx.n1->haseffect = 1;
    lpc_ctx.n1->bladeoffset = 0;
    lpc_ctx.n1->bladelen = 0;
    lpc_ctx.n1->enabled = 1;
    lpc_ctx.n1->pattern = ma;
    lpc_ctx.n1->ttype = STDIN;
    lpc_ctx.n1->menuptr = NULL;	// We don't use this until the menu is called. NULL it to a known state now.
    TAILQ_INSERT_TAIL(&head, lpc_ctx.n1, entries);

    lpc_ctx.curBlade = lpc_ctx.n1;	// Current Blade follows the newly created Blade.

    return;
}

void
newAwk()
{
    // XXX Needs libpc vs pc split.
    char awk[200];		// XXX Not okay to use fixed length field
    char *cp;
    char *ma;
    int nlen;

    cp = awk;
    echo();
    // Prompt to enter Awk output format
    printw("awk '{print \"");
    curs_set(1);
    refresh();
    while ((*(cp++) = getch()) != '\n')
	if (*(cp - 1) == 8 || *(cp - 1) == 127) {
	    cp--;
	    cp--;
	};
    cp--;
    *cp = '\0';
    noecho();
    printw("\nNew Awk format is:%s\n", awk);
    refresh();

/* XXX This check applies in the single-tool case, within a range of toolelements. e.g. don't grep-v the same
   string out twice, with no other changes in betwen.
   XXX The code needs to be updated to reflect that. */
/* Awk is not idempotent. You can apply the same awk multiple times and get different results */

/* Insert the new entry into the toolset list */
    lpc_ctx.n1 = malloc(sizeof(struct toolelement));	/* Insert at the head. */
    memset(lpc_ctx.n1->cache, 0, BLADECACHE);
    nlen = (strlen(awk) + 1);
    ma = malloc(nlen);
    lpc_ctx.n1->enabled = 1;
    lpc_ctx.n1->haseffect = 1;
    lpc_ctx.n1->bladeoffset = 0;
    lpc_ctx.n1->bladelen = 0;
    lpc_ctx.n1->pattern = ma;
    lpc_ctx.n1->ttype = FORMAT;
    lpc_ctx.n1->menuptr = NULL;	// We don't use this until the menu is called. NULL it to a known state now.
    strlcpy(lpc_ctx.n1->pattern, awk, nlen);	// Copy awk into new list entry
    TAILQ_INSERT_TAIL(&head, lpc_ctx.n1, entries);

    lpc_ctx.curBlade = lpc_ctx.n1;	// Current Blade follows the newly created Blade.

    return;

}

// This function is used to conditionally prepend one pipeline step that's required for the step to be added next.
// If it's already there, do nothing.
void
lpc_condprepend(char *prep)
{
    char prevblade[BLADECACHE];

    if (lpc_ctx.curBlade->ttype == BLACKBOX) {
	if (!strcmp(lpc_ctx.curBlade->pattern, prep)) {	// Previous element meets pre-req.
	    return;
	}
    }
    lpc_newBB(prep);
    return;
}

void
newBBox()
{
    char cp[1024];
    mvprintw(uigbl.maxy - 2, strlen(lpc_ctx.tstext), "| ");
    echo();
    curs_set(1);
    getnstr(cp, 1024);		// Get user input of literal blade
    noecho();
    curs_set(0);
    lpc_newBB(cp);
    return;
}

void
lpc_newBB(char *cmd)
{
    char *ma;
    int nlen;

/* XXX This check applies in the single-tool case, within a range of toolelements. e.g. don't grep-v the same
   string out twice, with no other changes in betwen.
   XXX The code needs to be updated to reflect that. */

/* Insert the new entry into the list of blades in the toolset */
    lpc_ctx.n1 = malloc(sizeof(struct toolelement));	/* Insert at the head. */
    memset(lpc_ctx.n1->cache, 0, BLADECACHE);
    nlen = (strlen(cmd) + 1);
    ma = malloc(nlen);
    lpc_ctx.n1->enabled = 1;
    lpc_ctx.n1->haseffect = 1;	// We don't know - so assume it does.
    lpc_ctx.n1->bladeoffset = 1;
    lpc_ctx.n1->bladelen = 2 + nlen;
    lpc_ctx.n1->pattern = ma;
    lpc_ctx.n1->ttype = BLACKBOX;
    lpc_ctx.n1->menuptr = NULL;	// We don't use this until the menu is called. NULL it to a known state now.
    strlcpy(lpc_ctx.n1->pattern, cmd, nlen);	// Copy cmd into new list entry

    TAILQ_INSERT_TAIL(&head, lpc_ctx.n1, entries);

    lpc_ctx.curBlade = lpc_ctx.n1;	// Current Blade follows the newly created Blade.

    return;

}

// Remove the last blade of the toolset. 
void
lpc_removeTail()
{
    lpc_ctx.np = TAILQ_LAST(&head, tailhead);
    free(lpc_ctx.np->pattern);
    lpc_ctx.curBlade = TAILQ_PREV(lpc_ctx.np, tailhead, entries);	// XXX Is this right?!
    TAILQ_REMOVE(&head, lpc_ctx.np, entries);
    free(lpc_ctx.np);
    lpc_ctx.np = lpc_ctx.n1;
    regenCaches();		// XXX This should not be needed, if we've cleaned up properly. Investigate.

}

// Remove the current blade of the toolset.  (Unless it's the starting CAT)
void
removeMid()
{
    lpc_ctx.np = lpc_ctx.curBlade;
    if (TAILQ_PREV(lpc_ctx.curBlade, tailhead, entries) != NULL) {
	free(lpc_ctx.np->pattern);
	lpc_ctx.n2 = TAILQ_NEXT(lpc_ctx.curBlade, entries);	// After deletion, curBlade moves to next, if there is one, otherwise to prev.
	if (lpc_ctx.n2) {
	    lpc_ctx.curBlade = lpc_ctx.n2;
	} else {
	    lpc_ctx.curBlade = TAILQ_PREV(lpc_ctx.np, tailhead, entries);
	}
	TAILQ_REMOVE(&head, lpc_ctx.np, entries);
	free(lpc_ctx.np);
	lpc_ctx.np = lpc_ctx.n1;
	regenCaches();		// XXX This should not be needed, if we've cleaned up properly. Investigate.
    }
}

void
listExclude()
{
//char excl[200];
//char *cp;

//cp=excl;
    int i = 0;

    erase();
    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
	i++;
	printw("%2d: %s\n", i, lpc_ctx.np->pattern);
    }
    refresh();
    printw("\nHit any key to return to file display\n");

    getch();
    return;
}

void
helpscreen()
{
/* Clear the display and show the help text. After the user hits a key, return to the main display. */
    erase();
    refresh();
    printw("Pipecut help screen: pipecut has a curses based UI with hotkey input. The following commands are available:\n" "\n" "Data Management operations:\n" " C: Toggle caching on or off (regenerates visible blade and predecessors at every action)\n" "\n" "Viewing / Browsing existing Blades\n" " l: List the defined exclusions (regexes which are grep -v'd out of the input)\n"	// XXX
	" ^R: Redraw the display\n"
	" q: Quit the application\n"
	"\n"
	"Miscellaneous\n"
	" ?: Display this help screen\n"
	"\n"
	"Blade Operations (Apply new transformations)\n"
	" a: Define a new formatting (awk) ** Incomplete function **\n"
	" g: Define a new inclusion (grep)\n"
	" x: Define a new exclusion (grep -v)\n"
	" h: Define a new hexdump (hexdump -C)\n"
	" w: Summarize (wc)\n"
	" Backspace: Delete the last blade\n"
	" Delete: Delete the current blade (except the first)\n"
	" ': Type any Unix command freehand (hit enter to finish)\n"
	"\n"
	"Execution\n"
	" |: Run the current toolset externally, piped through PAGER\n"
	"\n"
	"Navigation\n"
	" Cursor_Left:  focus on the previous blade to the current one\n"
	" Cursor_Right: focus on the next blade after the current one\n"
	"\n"
	"Load/Save toolset\n"
	" [: Load a toolset\n"
	" ]: Save a toolset\n"
	"\n"
	"Debugging\n"
	" P: Toggle cursor visiblity for curses debugging\n"
	"Press any key to return to the file contents display\n");

    refresh();

    getch();
    return;
}

void
updateTextPipeline(char pl[BLADECACHE], int script)
{

    // XXX Here is where we will decide whether to generate the optimized or unoptimized pipeline
    // XXX The optimized pipeline code is not yet written.

    memset(pl, 0, BLADECACHE);
    //int more=0;
    Pipestate lpc_pipestate;

    // This state machine tracks two things - the next blade type, and the current state of the pipeline at each step

    if (lpc_ctx.filtermode != 1) {
	if (script) {
	    snprintf(pl, BLADECACHE, "#!/bin/sh\ncat %s ", lpc_ctx.sourcefile);
	} else {		// Cat the source(s)
	    snprintf(pl, BLADECACHE, "cat %s ", lpc_ctx.sourcefile);
	}
	// Save the pipeline up to here, for execution
	strlcpy(lpc_ctx.tspart, pl, PATH_MAX);
    }
    lpc_pipestate = PIPE;

    // strcat(pl, "egrep -v '");
    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
	switch (lpc_ctx.np->ttype) {
	case BLACKBOX:
	case INCLUDE:
	case EXCLUDE:
	case FORMAT:
	case SUMMARIZE:
	    //pc_pipe_transition( lpc_pipestate, (Tooltype)EXCLUDE, lpc_ctx.np->pattern, pl);
	    lpc_ctx.np->bladeoffset = strlen(pl);
	    lpc_pipe_transition(lpc_pipestate, lpc_ctx.np->ttype,
		lpc_ctx.np->pattern, pl, script);
	    if (lpc_ctx.np == lpc_ctx.curBlade) {
		// Save the pipeline up to here, for execution
		strcpy(lpc_ctx.tspart, lpc_ctx.tstext);
	    }
	    lpc_ctx.np->bladelen = strlen(pl) - lpc_ctx.np->bladeoffset;
	    continue;
	default:
	    // For now, we let CAT (and STDIN) fall through here mid-toolset cats are not yet supported.
	    continue;
	}
    }
}

/* This function handles generating the text representation of the blades in their native
 * text format, implementing a Mealy State Machine function. mapping {state,next token} -> {action,state}
 *
 * Currently, where all blades are interposed by |, PIPE is the only valid input state. If we support
 * subexpressions | ( a; b; ) | etc, that may change, sp leaving the Mealy structure here for when
 * it may be needed later.
 */

void
lpc_pipe_transition(Pipestate lpc_pipestate, Tooltype ttype, char *patt,
    char *pl, int script)
{

    char bladetext[100];	// XXX Can't be fixed size

    switch (ttype) {

    case CAT:
	strcat(pl, " cat ");
	strcat(pl, patt);
	lpc_pipestate = PIPE;
	return;
    case EXCLUDE:
	switch (lpc_pipestate) {
	case PIPE:
	    if (!script) {
		strcat(pl, "| egrep -v '");
	    } else {
		strcat(pl, "| \\\negrep -v '");
	    }
	    strcat(pl, patt);
	    strcat(pl, "' ");
	    lpc_pipestate = PIPE;
	    return;
	    break;
	case PNONE:
	default:
	    fprintf(stderr,
		"\nPipecut Error: Unexpected state encounted in lpc_pipe_transition()\n");
	    exit(-1);
	    break;
	}
    case FORMAT:
	switch (lpc_pipestate) {
	case PIPE:
	    snprintf(bladetext, 100, "%s%s%s", lpc_toolcmds[(ttype * 2) + 0],
		patt, lpc_toolcmds[(ttype * 2) + 1]);
	    strcat(pl, " | ");
	    strcat(pl, bladetext);
	    lpc_pipestate = PIPE;
	    return;
	    break;
	case PNONE:
	default:
	    fprintf(stderr,
		"\nPipecut Error: Unexpected state encounted in lpc_pipe_transition()\n");
	    exit(-1);
	    break;
	}
    case BLACKBOX:
	switch (lpc_pipestate) {	// XXX Migrate from specialcase in updateTextPipeline
	case PIPE:
	    if (!script) {
		strcat(pl, "| ");
	    } else {
		strcat(pl, "| \\\n");
	    }
	    strcat(pl, lpc_ctx.np->pattern);
	    strcat(pl, " ");
	    return;
	    break;
	case PNONE:
	default:
	    fprintf(stderr,
		"\nPipecut Error: Unexpected state encounted in lpc_pipe_transition()\n");
	    exit(-1);
	    break;
	}
	return;
    case INCLUDE:
	switch (lpc_pipestate) {
	case PIPE:
	    if (!script) {
		strcat(pl, "| egrep '");
	    } else {
		strcat(pl, "| \\\negrep '");
	    }
	    strcat(pl, patt);
	    strcat(pl, "' ");
	    lpc_pipestate = PIPE;
	    return;
	    break;
	case PNONE:
	default:
	    fprintf(stderr,
		"\nPipecut Error: Unexpected state encounted in lpc_pipe_transition()\n");
	    exit(-1);
	    break;
	}
	return;
    case SUMMARIZE:
	switch (lpc_pipestate) {
	case PIPE:
	    if (!script) {
		strcat(pl, "| wc ");
	    } else {
		strcat(pl, "| \\\nwc ");
	    }
	    lpc_pipestate = PIPE;
	    return;
	    break;
	case PNONE:
	default:
	    fprintf(stderr,
		"\nPipecut Error: Unexpected state encounted in lpc_pipe_transition()\n");
	    exit(-1);
	    break;
	}
	return;
	break;
    case TNONE:		// None of these last four can occur yet. 2 are special cases, 2 unimplemented.
    case STDIN:
    case ORDER:
    case UNIQUE:
	return;
	break;
    }
}

void
regenCaches()
{
    int i = 0;
    char tmpbuf[BLADECACHE];

    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
	if (lpc_ctx.debug)
	    printw("Processing node %d\n", i++);
	if (lpc_ctx.np->ttype == CAT) {
	    // Cache is irrelevant - we source new input here.
	    bladeAction(lpc_ctx.np, tmpbuf);
	    lpc_ctx.n2 = lpc_ctx.np;
	} else {
	    // This node is not cached, but the prior one was (or we regnerated it)                 
	    strcpy(tmpbuf, lpc_ctx.n2->cache);
	    // Now apply the current node to the buffer
	    bladeAction(lpc_ctx.np, tmpbuf);
	    lpc_ctx.n2 = lpc_ctx.np;
	    strcpy(lpc_ctx.np->cache, tmpbuf);	// XXX Added this later - check correctness?
	}
	strcpy(lpc_ctx.np->cache, tmpbuf);
    }
}

/* This function displays an excerpt from the pipeline. When available, it displays a cached version of 
   the contents at the point of a particular blade. When not available, it calls for calculation of
   those contents. */
// XXX Needs a logic review after too many refactorings. What's the context here, what gets repainted?
// lpc_ctx.curBlade vs np vs n2 ...
void
displayfilepage(int redraw, char *exp)
{

    int i = 0;
    char tmpbuf[BLADECACHE];
    int x1, y1;
    int withregexpHL = 0;
    int withlaHL = 0;
    char LAexp[1024];

    if (exp) {
	// We were supplied a regex to highlight - XXX Test that it is well formed.
	withregexpHL = 1;

    }

    if (lpc_ctx.laon) {
	// Determine whether the next blade is eligible for LA highlighting (exists, has an RE pattern)
	withlaHL = 1;
	lpc_ctx.n2 = TAILQ_NEXT(lpc_ctx.curBlade, entries);
	if (!lpc_ctx.n2) {
	    withlaHL = 0;
	} else {		// No next blade to laHL 
	    if (lpc_ctx.n2->pattern == NULL) {
		withlaHL = 0;
	    }			// No pattern in next blade 
	    if (lpc_ctx.n2->ttype != INCLUDE && lpc_ctx.n2->ttype != EXCLUDE) {
		withlaHL = 0;
	    }			// No pattern in next blade 

	}
	if (withlaHL) {
	    strlcpy(LAexp, lpc_ctx.n2->pattern, 1024);
	}
    }
    //erase();
    getyx(uigbl.mainwin, y1, x1);
    refresh();

    //printw("EX %d WHY %d\n",x1,y1);
    if (lpc_ctx.cacheon && lpc_ctx.curBlade->cache[0] != '\0') {
	if (lpc_ctx.debug)
	    printw("Blade is CACHED\n");
    } else {
	if (lpc_ctx.debug)
	    printw("Regenerating all blades\n");
	// Walk forward through the toolset to find the first uncached blade. Regenerate forwards from that point.
	// (toolsets are unlikely to get longer than 30 nodes, at least until splitting is supported.

	// Are we at the head?
	if (lpc_ctx.debug)
	    printw("lpc_ctx.curBlade %x head %x\n", &lpc_ctx.curBlade, &head);
	//if (&lpc_ctx.curBlade == &head) {
	//      printw("Current blade is head, and not cached. Regen.\n");
	//      exit(-1);
	//}
	TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
	    if (lpc_ctx.debug)
		printw("Processing node %d\n", i++);
	    if (lpc_ctx.np->ttype == CAT) {
		// Cache is irrelevant - we source new input here.
		bladeAction(lpc_ctx.np, tmpbuf);
		lpc_ctx.n2 = lpc_ctx.np;
		strcpy(lpc_ctx.np->cache, tmpbuf);
	    } else {
		if (lpc_ctx.cacheon && lpc_ctx.np->cache[0] != '\0') {
		    if (lpc_ctx.debug)
			printw("Node is cached\n");
		    lpc_ctx.n2 = lpc_ctx.np;
		} else {
		    // This node is not cached, but the prior one was (or we regnerated it)                 
		    strcpy(tmpbuf, lpc_ctx.n2->cache);
		    // Now apply the current node to the buffer
		    bladeAction(lpc_ctx.np, tmpbuf);
		    lpc_ctx.n2 = lpc_ctx.np;
		    strcpy(lpc_ctx.np->cache, tmpbuf);
		}
	    }
	}
    }
  OUT:
    if (1 || redraw) {
	//erase ();
	Q refresh();		//YY
	clrtotop();
	Q refresh();		//YY
	printvisible(lpc_ctx.curBlade->cache, withregexpHL, exp, withlaHL,
	    LAexp);
	Q refresh();
    }
    //sleep (5);
    if (redraw) {		// Only do this when redrawing - leave status area along during RE mode...
	move(uigbl.maxy - 2, 0);
	clrtobot();		// If commented out, shorter status lines leave garbage
	Q refresh();		//YY
	updateStatus();
	Q refresh();		//YY
	move(uigbl.maxy - 2, 0);
	//clrtobot();
	// Rewrite the textPipeline
	updateTextPipeline(lpc_ctx.tstext, 0);
	curs_set(0);
	Q printw("%s", lpc_ctx.tstext);
	Q refresh();
	// XXX This fails in interesting ways when the toolset linewraps. Needs a plan for how long toolsets are displayed.
	mvwchgat(uigbl.mainwin, uigbl.maxy - 2,
	    lpc_ctx.curBlade->bladeoffset + 2, lpc_ctx.curBlade->bladelen - 3,
	    A_STANDOUT, (short)0, NULL);
    }
    Q refresh();
}

// This function prints lines no wider than the screen, no longer 
// than the screen minus the two status lines.
// This allows the buffer cache to be as large as we want, but only show
// a portion of it. (For now, only the beginning)
void
printvisible(char tmpbuf[BLADECACHE], int withHL, char *exp, int withLA,
    char *LAexp)
{
    char *eol;
    char *bol;
    char *eosz;

    char fmt[20];
    sz *szline;

    int linelen;
    int row = 0;
    regex_t disreg;
    regex_t lareg;
    int rc;
    int tabbedcount = 0;
    int rem;
    char *cp;

    char *tabcheck;
    int printedchars;

    regmatch_t matchesHL[10];
    regmatch_t matchesLA[10];
    int nMatchesHL;
    int nMatchesLA;

    szline = SzFromStr("");

    int HL = withHL;
    int LA = withLA;

    if (withHL) {
	// Compile the regex
	rc = regcomp(&disreg, exp, REG_EXTENDED);
	// If the RE doesn't compile - turn off HL within this function, so we don't use it (and crash)
	nMatchesHL = disreg.re_nsub + 1;
	if (rc)
	    HL = 0;
    }
    if (withLA) {
	// Compile the regex
	rc = regcomp(&lareg, LAexp, REG_EXTENDED);
	// If the RE doesn't compile - turn off LA within this function, so we don't use it (and crash)
	nMatchesLA = lareg.re_nsub + 1;
	if (rc)
	    LA = 0;
    }
    move(0, 0);
    bol = tmpbuf;
  TOPPVLOOP:
    if (*bol == '\0') {
	goto ENDPVLOOP;
    }
    eol = strchr(bol, '\n');
    if (!eol) {			// No \n at end of buffer. special case 
	eol = bol + strlen(bol);
    }
    szncpy(szline, bol, eol - bol + 1);
    tabbedcount = 0;
    tabcheck = StrFromSz(szline);	// Walk a pointer down the line to be printed, and count tab spacing
    eosz = tabcheck + szlen(szline);
    printedchars = 0;
    while (tabcheck < eosz && tabbedcount < uigbl.maxx) {
	if (*tabcheck == '\t') {	// Adjust the count depending on how far the tab will move us forward
	    rem = tabbedcount % 8;
	    tabbedcount += 8 - rem;
	    printedchars++;
	} else {
	    tabbedcount++;
	    printedchars++;
	}
	tabcheck++;
    }
    linelen = printedchars;
    if (linelen == uigbl.maxx - 1) {
	snprintf(fmt, 20, "%%.%ds\n", linelen);
    } else {
	snprintf(fmt, 20, "%%.%ds", linelen);
	refresh();		// Sometimes useful for debugging.
    }
    //printw("%s\n",fmt);
    mvprintw(row, 0, fmt, bol);
    if (HL) {
	// Now that the line is printed, go back and highlight it.
	rc = regexec(&disreg, StrFromSz(szline), nMatchesHL, matchesHL, 0);
	if (rc != REG_OK && rc != REG_NOMATCH) {
	    printw("Failure to process regex that compiled OK?!\n");
	    exit(-3);
	} else {
	    if (rc == REG_OK) {
		tabbedcount = 0;
		//printw("MATCHES 1:%s\n",line+matchesHL[0].rm_so,line+matchesHL[1].rm_so,bol+matchesHL[2].rm_so);
		// Calculate TABS ... annoying.
		tabcheck = StrFromSz(szline);
		while (tabcheck < &StrFromSz(szline)[matchesHL[0].rm_so]) {
		    if (*tabcheck == '\t') {	// Adjust the count depending on how far the tab will move us forward
			rem = tabbedcount % 8;
			tabbedcount += 8 - rem;
		    } else {
			tabbedcount++;
		    }
		    tabcheck++;
		}
		mvwchgat(uigbl.mainwin, row, tabbedcount,
		    matchesHL[0].rm_eo - matchesHL[0].rm_so, A_STANDOUT,
		    (short)0, NULL);
	    }
	}

    }
    if (LA) {
	// Now that the line is printed, go back and highlight it.
	// For now, we just find and highlight the first match. We should loop over all matches XXX
	rc = regexec(&lareg, StrFromSz(szline), nMatchesLA, matchesLA, 0);
	if (rc != REG_OK && rc != REG_NOMATCH) {
	    printw("Failure to process regex that compiled OK?!\n");
	    exit(-3);
	} else {
	    if (rc == REG_OK) {
		tabbedcount = 0;
		// Calculate TABS ... annoying
		tabcheck = bol;
		while (tabcheck < bol + matchesLA[0].rm_so) {
		    if (*tabcheck == '\t') {	// Adjust the count depending on how far the tab will move us forward
			rem = tabbedcount % 8;
			tabbedcount += 8 - rem;
		    } else {
			tabbedcount++;
		    }
		    tabcheck++;
		}
		mvwchgat(uigbl.mainwin, row, tabbedcount,
		    matchesLA[0].rm_eo - matchesLA[0].rm_so, A_STANDOUT,
		    (short)0, NULL);
	    }
	}
	// szfree(szline); XXX This needs to be freed - just need to make sure it's in the right place.
    }
    row++;
    if (row >= uigbl.maxy - 3) {
	return;
    }
    bol = eol + 1;
    goto TOPPVLOOP;
  ENDPVLOOP:
    return;

}

// This is the function where the actual processing of a blade's data transformation happens.
// This is where you need to teach pipecut about anything you don't want treated like a blackbox.
void
bladeAction(struct toolelement *blade, char tmpbuf[BLADECACHE])
{
    FILE *fp;
    char *rcc;
    int lc = 0;
    char buf[240];
    char tmpbuf2[BLADECACHE];
    char line[1024];

    int rc;
    long wcl = 0, wcw = 0, wcc = 0;

    char *eol;
    char *bol;
    blade->haseffect = 0;
    memset(tmpbuf2, 0, BLADECACHE);

    // Note: The ENDLOOP target is shared.
    switch (blade->ttype) {
    case CAT:
	fp = fopen(lpc_ctx.np->pattern, "r");	// XXX Need to test for success here
	if (!fp) {
	    printf("Failed to open file %s\n", lpc_ctx.np->pattern);
	    exit(-1);
	}
	fseek(fp, lpc_ctx.fileoffset, SEEK_SET);
	memset(tmpbuf, 0, BLADECACHE);
	while (!feof(fp) && lc < uigbl.maxy - 2) {
	    rcc = fgets(buf, 239, fp);
	    if (!rcc)
		break;
	    strcat(tmpbuf, buf);
	    lc++;
	}
	lpc_ctx.filepageend = ftell(fp);
	fclose(fp);
	break;
    case INCLUDE:
	bol = tmpbuf;
      INCLUDELINELOOP:
	// Read line-oriented input
	if (*bol == '\0') {
	    goto ENDLOOP;
	}
	eol = strchr(bol, '\n');
	if (!eol) {		// No \n at end of buffer. special case 
	    eol = bol + strlen(bol);
	}
	strlcpy(line, bol, eol - bol + 1);
	bol = eol + 1;
	rc = regexec(&blade->preg, line, 0, NULL, 0);
	if (rc != REG_OK && rc != REG_NOMATCH) {
	    endwin();
	    printf
		("Regex execution failed on exclusion %s = %d\n",
		lpc_ctx.np->pattern, rc);
	    exit(-1);
	}
	if (rc == REG_OK) {	// Matching lines are copied in an INCLUDE
	    strcat(tmpbuf2, line);
	    strcat(tmpbuf2, "\n");
	    goto INCLUDELINELOOP;
	}
	if (rc == REG_NOMATCH) {	// Non-matching lines DO NOT carry over to output buffer
	    blade->haseffect = 1;
	    goto INCLUDELINELOOP;
	}
	break;
    case EXCLUDE:
	bol = tmpbuf;
      EXCLUDELINELOOP:
	// Read line-oriented input
	if (*bol == '\0') {
	    goto ENDLOOP;
	}
	eol = strchr(bol, '\n');
	if (!eol) {		// No \n at end of buffer. special case 
	    eol = bol + strlen(bol);
	}
	strlcpy(line, bol, eol - bol + 1);
	bol = eol + 1;
	rc = regexec(&blade->preg, line, 0, NULL, 0);
	if (rc != REG_OK && rc != REG_NOMATCH) {
	    endwin();
	    printf
		("Regex execution failed on exclusion %s = %d\n",
		lpc_ctx.np->pattern, rc);
	    exit(-1);
	}
	if (rc == REG_OK) {	// Matching lines are NOT copied in an EXCLUDE
	    blade->haseffect = 1;
	    goto EXCLUDELINELOOP;
	}
	if (rc == REG_NOMATCH) {	// Non-matching lines carry over to output buffer
	    strcat(tmpbuf2, line);
	    strcat(tmpbuf2, "\n");
	    goto EXCLUDELINELOOP;
	}
	break;
    case SUMMARIZE:
	bol = tmpbuf;
      SUMMARIZELINELOOP:
	// Read line-oriented input
	if (*bol == '\0') {
	    blade->haseffect = 1;
	    snprintf(tmpbuf2, 80, "   %ld   %ld   %ld\n", wcl, wcw, wcc);
	    goto ENDLOOP;
	}
	eol = strchr(bol, '\n');
	if (!eol) {		// No \n at end of buffer. special case 
	    eol = bol + strlen(bol);
	}
	strlcpy(line, bol, eol - bol + 1);
	wcc += eol - bol + 1;	// Count characters
	wcl++;			// Count a line
	wcw += pc_wc_w(line);	// XXX Count words
	bol = eol + 1;
	goto SUMMARIZELINELOOP;
	break;
    case FORMAT:
	bol = tmpbuf;
      FORMATLINELOOP:
	// Read line-oriented input
	if (*bol == '\0') {
	    blade->haseffect = 1;	// Could use more sophisticated method in this case.
	    goto ENDLOOP;
	}
	eol = strchr(bol, '\n');
	if (!eol) {		// No \n at end of buffer. special case 
	    eol = bol + strlen(bol);
	}
	strlcpy(line, bol, eol - bol + 1);
	bol = eol + 1;
	// Format the line as per the awk arguments, and copy it to tmpbuf2
	// XXX Implement FORMAT
	strcat(tmpbuf2, line);
	strcat(tmpbuf2, "\n");
	goto FORMATLINELOOP;
      ENDLOOP:
	memset(tmpbuf, 0, BLADECACHE);
	strcpy(tmpbuf, tmpbuf2);
	break;
    case BLACKBOX:		// Here's the fun part - running the bladecache through external commands.
	// Create the pipe  - first, run input through pipe to child, let child dump to stdout.
	//sleep(1);
	runpipe(blade->pattern, tmpbuf);
	//strcmp(tmpbuf,
	//sleep(1);
	break;
    default:
	break;

    }
    return;

}

void
updateStatus()
{
    char status[100];
    int xoff = uigbl.maxx;
    xoff--;			// Final space
    xoff -= 8;
    strcpy(status, "Status: ");
    if (lpc_ctx.reon == 1) {
	xoff -= 5;		// filters
	strcat(status, "Re ");
    } else {
	xoff -= 6;		// nofilters
	strcat(status, "noRe ");
    }

    if (lpc_ctx.laon == 1) {
	xoff -= 5;		// filters
	strcat(status, "LA ");
    } else {
	xoff -= 6;		// nofilters
	strcat(status, "noLA ");
    }

    if (lpc_ctx.cacheon == 1) {
	xoff -= 6;		// Cache
	strcat(status, "Cache ");
    } else {
	xoff -= 8;		// noCache
	strcat(status, "noCache ");
    }
    mvprintw(uigbl.maxy - 1, xoff, status);
}

// Retained for reference from example curses code. Not currently used.
void
terminalnormal()
{
    /*
     * restore the old settings
     */
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

}

void
terminalraw()
{

    /*
     * tcgetattr gets the parameters of the current terminal
     * STDIN_FILENO will tell tcgetattr that it should write the settings
     * of stdin to oldt
     */
    tcgetattr(STDIN_FILENO, &oldt);
    /*
     * now the settings will be copied
     */
    newt = oldt;

    /*
     * ICANON normally takes care that one line at a time will be processed
     * that means it will return if it sees a "\n" or an EOF or an EOL
     */
    newt.c_lflag &= ~(ICANON | ECHO);

    /*
     * Those new settings will be set to STDIN
     * TCSANOW tells tcsetattr to change attributes immediately. 
     */
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

}

// Not currently used, but potentially useful for modal messages.
void
print_in_middle(WINDOW * win, int starty, int startx, int width, char *string)
{
    int length, x, y;
    float temp;

    if (win == NULL)
	win = stdscr;
    getyx(win, y, x);
    if (startx != 0)
	x = startx;
    if (starty != 0)
	y = starty;
    if (width == 0)
	width = 163;

    length = strlen(string);
    temp = (width - length) / 2;
    x = startx + (int)temp;
    attron(COLOR_PAIR(1));
    mvwprintw(win, y, x, "%s", string);
    attron(COLOR_PAIR(2));
    refresh();
}

#define handle_error_en(en, msg) \
	do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
       do { perror(msg); exit(EXIT_FAILURE); } while (0)

void *
stat_thread(void *arg)
{
    // Removed while I resolve pthread portability issues
    return NULL;
}

// XXX pthread_create arguments are different between BSD and Linux.
// Comment out for now.
void
start_background_thread(char *av1)
{
    // XXX Initial implementation removed while cross-platform debugging continues.
}

void
usage(char *av0)
{
    fprintf(stderr, "pipecut usage:\n");
    fprintf(stderr,
	"\nA file to slice must be provided as the first argument.\n");
    fprintf(stderr,
	"\nCommand line formats for pipecut:\n"
	"a) pipecut filename    (enters fullscreen mode)\n"
	"b) pipecut -t toolset  (loads toolset from ~/.pipecut.db (ignoring CAT) and acts as a filter)\n"
	"c) history | pipecut   (pipecut consumes shell history and creates toolset from last cmd)\n"
	"\n");
    //printf("%s filename\n", av0);
    exit(-1);
}

void
version(char *av0)
{
    fprintf(stderr, "Pipecut version: %s\n",PIPECUT_VERSION);
    exit(-1);
}

// XXX This is out of date and has some bugs. Needs cleanup.
void
menu()
{
    ITEM **my_items;
    int c, linelen;
    int targetitem = 0;
    MENU *my_menu;
    int n_choices = 0, i = 0;
    //ITEM   *cur_item;

    WINDOW *menuwin;

    //menuwin = initscr();
    menuwin = dupwin(uigbl.mainwin);
    //return; // XXXX
    cbreak();
    noecho();
    erase();
    keypad(stdscr, TRUE);

    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
	n_choices++;
    }

    my_items = (ITEM **) calloc(n_choices + 1, sizeof(ITEM *));

    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
	lpc_ctx.np->menuptr = malloc(4);	// These must live while the menu is in use.
	linelen = strlen(lpc_ctx.np->pattern) + 26;	// XXX Needs to be the longest pattern
	lpc_ctx.np->menuline = malloc(linelen);	// These must live while the menu is in use.
	snprintf(lpc_ctx.np->menuptr, 4, "%2d:", i + 1);
	snprintf(lpc_ctx.np->menuline, linelen, "%s%s%s",
	    lpc_toolcmds[(lpc_ctx.np->ttype * 2) + 0], lpc_ctx.np->pattern,
	    lpc_toolcmds[(lpc_ctx.np->ttype * 2) + 1]);
	my_items[i] = new_item(lpc_ctx.np->menuptr, lpc_ctx.np->menuline);
	i++;
    }
    my_items[n_choices] = (ITEM *) NULL;

    my_menu = new_menu((ITEM **) my_items);
    set_menu_back(my_menu, A_DIM);
    mvprintw(LINES - 2, 0, "'q' to quit");
    post_menu(my_menu);
    refresh();

    keypad(menuwin, 1);		// Handle Esc sequences for us, thank you.
    while ((c = getch()) != 'q') {
	switch (c) {
	case KEY_DOWN:
	case 'j':
	    menu_driver(my_menu, REQ_DOWN_ITEM);
	    break;
	case KEY_UP:
	case 'k':
	    menu_driver(my_menu, REQ_UP_ITEM);
	    break;
	case 'd':

	    mvprintw(20, 20, "Pressed delete");
	    targetitem = item_index(current_item(my_menu));
	    if (targetitem == 0) {
		// Don't allow deletion of the first CAT
		break;
	    }
	    i = -1;
	    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
		i++;
		if (targetitem == i) {
		    TAILQ_REMOVE(&head, lpc_ctx.np, entries);
		    break;
		}
	    }			// Call a fresh menu on the updated toolset - we'll fall through and cleanup afterwards.
	    menu();
	    goto menu_cleanup;
	    break;

	}
    }

  menu_cleanup:
    free_menu(my_menu);		// Release the menu
    for (i = 0; i < n_choices; i++)
	free_item(my_items[i]);	// Release the menu items
    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
	free(lpc_ctx.np->menuptr);
	lpc_ctx.np->menuptr = NULL;
	free(lpc_ctx.np->menuline);
	lpc_ctx.np->menuline = NULL;
    }				// Release the menu item strings
    delwin(menuwin);
    refresh();
}

// This function consumes the 'history | pipecut' input
void
pc_text2ts(char *inp)
{
    char *cp;
    char *cp2;
    char *cp3;
    regex_t histRegex;

    int rc;
    int lastround = 0;
    char bld[1024];
    //regmatch_t *matches;
    regmatch_t matches[10];

    int nMatches;

    //const char foo[]="^[  ]+\([0-9]+\)[   ]+\([0-9][0-9]:[0-9][0-9]\)     \(.*\)$";
    const char foo[] =
	"^[ 	]+([0-9]+)[ 	]+([0-9]?[0-9]:[0-9][0-9])	(.*)$";
//      const char *foo="^[     ]+\([0-9]+\)[   ]+\([0-9][0-9]:[0-9][0-9]\)     \(.*\)$";

    // We're called to create a new toolset from the text of a history line.

    rc = regcomp(&histRegex, foo, REG_EXTENDED);

    if (rc) {
	endwin();
	printf("Regex compile failed %s = %d\n", foo, rc);
	printf
	    ("This has been observed when you include control characters in your\n"
	    "regular expression. If this is not the case, try your expression again\n"
	    "and report a bug if it doesn't have control characters, but still fails to compile.\n");

	exit(-1);
    }
    nMatches = histRegex.re_nsub + 1;
    //matches =  malloc (nMatches * sizeof(regmatch_t));
    rc = regexec(&histRegex, inp, nMatches, matches, 0);
    printf("RC %d\n", rc);
    printf("MATCHES 1:%s 2:%s 3:%s\n", inp + matches[0].rm_so,
	inp + matches[1].rm_so, inp + matches[2].rm_so);

    cp = inp + matches[3].rm_so;

    while (1) {
	// XXX Note: This doesn't handle blades with quoted |s.
	// Will probably require a full-blown parser (from tcsh maybe?)
	cp2 = strchr(cp, '|');
	if (!cp2) {
	    lastround = 1;
	    cp2 = strchr(cp, '\n');	// The last blade ends with \n, not |
	    cp2++;
	}
	if (!lastround) {
	    // Tweak the char pointer. Skip spaces
	    while (isspace((int)*cp))
		cp++;
	    strlcpy(bld, cp, cp2 - cp);
	} else {
	    // Tweak the char pointer. Skip spaces
	    while (isspace((int)*cp))
		cp++;
	    strlcpy(bld, cp, cp2 - cp);
	}
	if (!strncmp(bld, "cat", 3)) {	// XXX This will break on cat blades except at first blade
	    cp3 = &bld[4];
	    strlcpy(lpc_ctx.sourcefile, cp3, PATH_MAX);
	    lpc_newCat(cp3);
	} else {
	    lpc_newBB(bld);
	}
	if (lastround) {
	    goto OUTLR;
	}
	cp = cp2 + 1;
    }

  OUTLR:
    regenCaches();

    // Cleanup/release histRegex
    regfree(&histRegex);
    return;

}

void
clrtotop()
{

    int i;
    for (i = 0; i < uigbl.maxy - 3; i++) {
	move(i, 0);
	clrtoeol();
    }
    return;
}

int
pc_wc_w(char *str)
{
    int n = 0;
    for (str = strtok(str, " -.!,;"); str; str = strtok(NULL, " -.!,;"))
	n++;
    return n;
}

void
fullrun(char lesspipe[BLADECACHE])
{
    WINDOW *systemwin;
    int k;

    // XXX not changing this for now, but the ncurses intro has a Shelling out example
    // worth exaluating/comparing to this approach.
    systemwin = dupwin(uigbl.mainwin);

    werase(systemwin);
    wrefresh(systemwin);
    system(lesspipe);
    delwin(systemwin);
    wrefresh(uigbl.mainwin);
    move(uigbl.maxy, 0);
    for (k = 0; k < uigbl.maxy; k++) {
	printf("\n\n");
    }
    move(0, 0);
    clrtotop();
    refresh();
    displayfilepage(1, NULL);
    return;

}

void
filterrun(char lesspipe[BLADECACHE])
{
    system(lesspipe);
    return;
}

void
runpipe(char *cmd, char tmpbuf[BLADECACHE])
{
    int pid;

    int fdA[2];			// pipe from child stdout->fdA[1] to parent fdA[0]
    int fdB[2];			// pipe from parent fdB[1] to child fdB[0]->stdin

    int n;
    int noargs = 0;
    char tmpbuf2[BLADECACHE];
    char prog[1024];
//char line[1024];
    char *eprog;
    char *mol;
    char errtxt[1024];
    int tbytes = 0;

    eprog = strpbrk(cmd, " \t");

    if (!eprog) {
	noargs = 1;
    }

    move(0, 0);

    strlcpy(prog, cmd, eprog - cmd + 1);
    eprog++;

    pipe(fdA);
    pipe(fdB);

    if (noargs) {
	//printf("CMD %s\n",prog);
    } else {
	//printf("CMD %s, ARGS %s\n",prog,eprog);
    }
    pid = fork();

    //sleep(10);

    if (pid == 0) {
	//sleep(10);
	dup2(fdA[1], 1);	// stdout goes to fdA[1] for delivery to parent
	dup2(fdB[0], 0);	// stdin comes from fdB[0] from parent
	close(fdA[0]);
	close(fdB[0]);
	close(fdA[1]);
	close(fdB[1]);
	//sleep(10);
	strcpy(errtxt, "Exec failed: ");
	strcat(errtxt, prog);
	strcat(errtxt, " ");
	if (noargs) {
	    execlp(prog, prog, NULL);
	} else {
	    strcat(errtxt, eprog);
	    strcat(errtxt, " ");
	    execlp(prog, prog, eprog, NULL);
	}
	perror(errtxt);
	printf("HOWD THIS HAPPEN\n");
	exit(-1);
    }

    if (pid != 0) {

	write(fdB[1], tmpbuf, strlen(tmpbuf));
	//write(fdB[1],"Yet another test\n",15); // fd handling is complex
	//write(fdB[1],"One more test\n",15);
	close(fdA[1]);
	close(fdB[1]);
	close(fdB[0]);
	mol = tmpbuf2;
	while (1) {
	    n = read(fdA[0], mol, BLADECACHE - tbytes);
	    if (n == 0) {
		//printf("Child closed pipe\n");
		goto WHILEOUT;
	    }
	    if (n < 0) {
		//printf("Pipe error\n");
		goto WHILEOUT;
	    }
	    tbytes += n;
	    mol = &tmpbuf2[tbytes];
	    /*if (fputs(line,stdout) == EOF) {
	     * //printf("ERROR ON OUTPUT\n");
	     * exit(-3);
	     * } */
	}
      WHILEOUT:
	close(fdA[0]);
	tmpbuf2[tbytes] = '\0';
	strlcpy(tmpbuf, tmpbuf2, BLADECACHE);
	wait(&pid);
    }

}

void
pc_init(struct pipecut_ctx *ctx)
{
    // Don't set debug - it's set at the top of main()
    ctx->cacheon = 1;		// Cache is enabled by default
    ctx->reon = 0;		// RE mode is off by default
    ctx->laon = 0;		// LA mode is off by default
    ctx->curs = 1;		// UI state
    ctx->fileoffset = 0;
    ctx->filepageend = 0;
    memset(ctx->tstext, 0, BLADECACHE);
    memset(ctx->tspart, 0, BLADECACHE);
    memset(ctx->sourcefile, 0, PATH_MAX);
}
