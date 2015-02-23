// # vim: shiftwidth=4 tabstop=4 softtabstop=4 expandtab
// # indent: -bap -br -ce -ci4 -cli0 -d0 -di0 -i4 -ip -l79 -nbc -ncdb -ndj -ei -nfc1 -nlp -npcs - psl - sc - sob
// # Gnu indent: -bap -nbad -br -ce -ci4 -cli0 -d0 -di0 -i4 -ip4 -l79 -nbc -ncdb -ndj -nfc1 -nlp - npcs - psl - sc - sob
//TOUR: pcDB.c: pipecut database functions. These belong to the libpipecut library
/*
 * Copyright (c) 2015, David William Maxwell david_at_NetBSD_dot_org
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
#include "pipecut.h"
#include "pcDB.h"
#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#else
#include "queue.h"		// Included copy of NetBSD's queue.h
#endif
#include <ipe.h>		// XXX This should not be here. Needed until load/save become lpc_
extern struct pipecut_ctx lpc_ctx;

sqlite3 *db;
sqlite3_stmt *stmt = NULL;
char *zErrMsg = 0;
static int sqlDebug = 0;

int
callback(void *NotUsed, int argc, char **argv, char **azColName)
{

    int i;
    for (i = 0; i < argc; i++) {

	//printf("%s%s",azColName[i], " = ");
	if (argv[i]) {
	    printf("%s", argv[i]);
	} else {
	    printf("NULL");
	}
	printf("\n");
    }
    return 0;
}

void
initDB(int dbg)
{
    int rc;
    char *sql;
    char **sql_results;		// XXX for deprecated get_table interface
    int reccnt, colcnt;
    char *err_msg = NULL;
    // sqlite3_stmt *stmt = NULL;
    int tmp = 0;
    char dbpath[PATH_MAX];
    char *home;
    int commitMode = 0;

    sqlDebug = dbg;		// Set the debug state for all sql operations

    strcpy(dbpath, "");

    // Get Home directory.
    home = getenv("HOME");
    strlcpy(dbpath, home, PATH_MAX);
    strlcat(dbpath, "/.pipecut.db", PATH_MAX);

    rc = sqlite3_open(dbpath, &db);

    if (rc) {
	fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
	exit(0);
    } else {
	if (sqlDebug)
	    fprintf(stderr, "Opened database successfully\n");
    }

    commitMode = sqlite3_get_autocommit(db);
    if (sqlDebug)
	fprintf(stderr, "Autocommit MODE IS %d\n", commitMode);

// Check for existence of main tables. If they're not there, create them.
    sql =
	"select count(type) from sqlite_master where type='table' and name='toolset'";

// Deprecated interface - pulls a full table as char arrays, must
// be manually freed afterwards with sqlite3_free_table()
//   rc = sqlite3_get_table(db, sql, &sql_results, &reccnt, &colcnt, &err_msg);

    rc = sqlite3_prepare_v2(db, sql, 1024, &stmt, NULL);

    if (sqlDebug)
	printf("RC on prepare: %d\n", rc);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
	tmp = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);

    if (rc != SQLITE_OK) {
	fprintf(stderr, "RC %d\n", rc);
	fprintf(stderr, "Error: %s\n", err_msg);
	sqlite3_free(err_msg);
	goto stop;
    }
    if (sqlDebug)
	fprintf(stderr, "Success getting table, reccount:%d\n", reccnt);	// XXX

    if (tmp < 1) {
	sql = "CREATE TABLE meta ( k VARCHAR(50) UNIQUE, v VARCHAR(2048));";
	rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
	    if (rc == SQLITE_ERROR
		&& !strcmp(zErrMsg, "table meta already exists")) {
		if (sqlDebug)
		    fprintf(stderr, "Meta table already present : %s\n",
			zErrMsg);
	    } else {
		fprintf(stderr, "SQL error: %d : %s\n", rc, zErrMsg);
	    }
	    sqlite3_free(zErrMsg);
	} else {
	    if (sqlDebug)
		fprintf(stdout, "Meta Table created successfully\n");
	}

	sql = "INSERT INTO meta VALUES ('pipecut_db_version','1.0');";

	rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
	    if (rc == SQLITE_CONSTRAINT) {
		if (sqlDebug)
		    printf("Meta version already populated: %s\n", zErrMsg);
	    } else {
		printf("Never before seen error code from database! %s\n",
		    zErrMsg);
	    }
	    sqlite3_free(zErrMsg);
	} else {
	    if (sqlDebug)
		printf("Insert succeeded\n");
	}

	sql = "CREATE TABLE toolset ( "
	    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
	    "name VARCHAR(50), "
	    "bladecount INTEGER, " "desc VARCHAR(300) " ");";
	rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
	    if (rc == SQLITE_ERROR
		&& !strcmp(zErrMsg, "table toolset already exists")) {
		if (sqlDebug)
		    fprintf(stderr, "toolset table already present : %s\n",
			zErrMsg);
	    } else {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	    }
	} else {
	    if (sqlDebug)
		fprintf(stdout, "Toolset Table created successfully\n");
	}

    }
// XXX cleanup   // sqlite3_free_table(sql_results);
    //fprintf (stderr, "Freed table\n"); // XXX

//   rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
//   if( rc != SQLITE_OK ){
//   fprintf(stderr, "SQL error: %s\n", zErrMsg);
//      sqlite3_free(zErrMsg);
//   }else{
//      fprintf(stdout, "Table created successfully\n");
//   }

    /* Create SQL statement  */
    sql = "CREATE TABLE blade("
	"toolset REFERENCES toolset(id),"
	"component INT     NOT NULL," "type varchar(20)," "pattern BLOB );";

    /* Execute SQL statement  */
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
	if (rc == SQLITE_ERROR
	    && !strcmp(zErrMsg, "table blade already exists")) {
	    if (sqlDebug)
		fprintf(stderr, "blade table already present : %s\n", zErrMsg);
	} else {
	    fprintf(stderr, "SQL error: %s\n", zErrMsg);
	    sqlite3_free(zErrMsg);
	}
    } else {
	if (sqlDebug)
	    fprintf(stdout, "Table created successfully\n");
    }

  stop:
    return;
}

void
closeDB()
{
    sqlite3_close(db);
}

// XXX Needs to be split front/back.
void
pc_loadToolset(int mode)
{
    char inc[1024];		// XXX Not okay to use fixed length field
    int bladecount = 0;
    int toolset_id;
    int component;
    const unsigned char *typetmp;
    char type[20];
    const unsigned char *patterntmp;
    char *pattern;
    int patternlen;
    char *sql;
    int rc = 0;
    int tmp = -1;

    if (mode == 0) {		// Load
	mvprintw(uigbl.maxy - 1, 0, "Load toolset named: ");
    } else if (mode == 1) {	// Append
	mvprintw(uigbl.maxy - 1, 0, "Append toolset named: ");
    }

    if (mode == 2) {		// Filter
	strlcpy(inc, lpc_ctx.filter, 1024);
    } else {
	echo();
	getnstr(inc, 1024);
	noecho();
    }

// check for the existence of the toolset name XXX
    initDB(0);

    sql = "SELECT id, bladecount from toolset where name = ? ;";

    rc = sqlite3_prepare_v2(db, sql, 1024, &stmt, NULL);
    if (sqlDebug)
	printf("RC on prepare: %d\n", rc);

    sqlite3_bind_text(stmt, 1, (const char *)inc, -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
	toolset_id = sqlite3_column_int(stmt, 0);
	bladecount = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);

    if (bladecount < 1) {
	mvprintw(uigbl.maxy - 1, 0, "No toolset with that name exists.");
	if (mode == 2)
	    fprintf(stderr,
		"Pipecut Error: No toolset with specified name in ~/.pipecut.db\n");
	getnstr(inc, 1024);
	return;
    } else {
	if (sqlDebug)
	    printf("Toolset with that name exists.");
    }

    sql =
	"SELECT component, type, pattern from blade where toolset = ? ORDER by component ASC;";

// Clear out any existing blades. XXX May want to allow to retain old CAT blade?
    if (mode == 0) {
	TAILQ_FOREACH_SAFE(lpc_ctx.np, &head, entries, lpc_ctx.n3) {
	    TAILQ_REMOVE(&head, lpc_ctx.np, entries);
	    free(lpc_ctx.np->pattern);
	    free(lpc_ctx.np);
	}
    }

    rc = sqlite3_prepare_v2(db, sql, 1024, &stmt, NULL);
    if (sqlDebug)
	printf("RC on prepare: %d\n", rc);

    sqlite3_bind_int(stmt, 1, toolset_id);
    // Iterate over the blades
    while (sqlite3_step(stmt) == SQLITE_ROW) {
	component = sqlite3_column_int(stmt, 0);
	typetmp = sqlite3_column_text(stmt, 1);
	patterntmp = sqlite3_column_text(stmt, 2);
	patternlen = sqlite3_column_bytes(stmt, 2) + 1;
	pattern = malloc(patternlen);
	strlcpy(pattern, (char *)patterntmp, patternlen);
	if (!strncmp((char *)typetmp, "NONE", 20)) {
	}			// We should never have these in a DB. Ignore. 
	if (!strncmp((char *)typetmp, "STDIN", 20)) {
	}			// We should never have these in a DB. Ignore. 
	if (!strncmp((char *)typetmp, "CAT", 20)) {
	    if (mode == 1) {	// Skip the CAT blade when appending.
		if (lpc_ctx.debug)
		    printf("Skipping CAT blade\n");
	    } else if (mode == 2) {	// Turn the CAT into a STDIN when filtering
		pc_newSTDIN();
	    } else {
		strlcpy(lpc_ctx.sourcefile, pattern, PATH_MAX);	/* XXX If this copy happens, then source filesname come
								 * with a toolset being loaded. This is probably the opposite
								 * of the common case. We should support both, but this
								 * make it easy to demo for now. */
		lpc_newCat(pattern);
	    }
	}
	if (!strncmp((char *)typetmp, "BLACKBOX", 20)) {
	    lpc_newBB(pattern);
	}
	if (!strncmp((char *)typetmp, "EXCLUDE", 20)) {
	    lpc_newEX(pattern);
	}
	if (!strncmp((char *)typetmp, "INCLUDE", 20)) {
	    lpc_newIN(pattern);
	}			// YYY 
	if (!strncmp((char *)typetmp, "SUMMARIZE", 20)) {
	    lpc_newEX(pattern);
	}
	if (!strncmp((char *)typetmp, "ORDER", 20)) {
	    lpc_newEX(pattern);
	}
	if (!strncmp((char *)typetmp, "UNIQUE", 20)) {
	    lpc_newEX(pattern);
	}

	free(pattern);
    }
    sqlite3_finalize(stmt);

// Sanity check that the new toolset exists in the database and is intact.

// XXX TODO move the existing Toolset into the undo buffer, and load the new one in.

    return;
}

void
pc_saveToolset()
{
    char inc[1024];		// XXX Not okay to use fixed length field
    char *cp;
    char *sql;
    int tmp = -1;
    int rc = 0;
    sqlite3_int64 id;
    int blade_id;
    int bladecount;

    char texttype[20];

    cp = inc;

// Ask for the desired save name XXX
    mvprintw(uigbl.maxy - 1, 0, "Save toolset as: ");
    echo();
    getnstr(cp, 1024);
    noecho();

// check for the existence of the toolset name XXX
    initDB(0);

    sql = "SELECT * from toolset where name = ? ;";

    rc = sqlite3_prepare_v2(db, sql, 1024, &stmt, NULL);
    if (sqlDebug)
	printf("RC on prepare: %d\n", rc);

    sqlite3_bind_text(stmt, 1, (const char *)cp, -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
	tmp = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);

    if (tmp == -1) {
	printf("No toolset with that name exists.");
    } else {
	printf("Toolset with that name exists.");
    }

// Start a DB transaction. XXX
    sql = "BEGIN;";
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
	printf("RC %d\n", rc);
	fprintf(stderr, "**Error: %s\n", zErrMsg);
	sqlite3_free(zErrMsg);
    }
// Erase the existing toolset (if it exists) (prompt for overwrite first? XXX)

    sql =
	"DELETE FROM blade where toolset = (select id from toolset where name = ? ;";
    rc = sqlite3_prepare_v2(db, sql, 1024, &stmt, NULL);
    if (sqlDebug)
	printf("RC on prepare: %d\n", rc);
    sqlite3_bind_text(stmt, 1, (const char *)cp, -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
	;
    }
    sqlite3_finalize(stmt);

    sql =
	"DELETE from toolset where id = (select id from toolset where name = ? ;";
    rc = sqlite3_prepare_v2(db, sql, 1024, &stmt, NULL);
    if (sqlDebug)
	printf("RC on prepare: %d\n", rc);
    sqlite3_bind_text(stmt, 1, (const char *)cp, -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
	;
    }
    sqlite3_finalize(stmt);

    // Count blades
    bladecount = 0;
    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {
	bladecount++;
    }

// Allocate the next toolset id for the toolset being saved. (SQLite automatically assigns the id if it's left to the default.)
    sql = "INSERT into toolset (name,bladecount) values ( ?, ? )";
    rc = sqlite3_prepare_v2(db, sql, 1024, &stmt, NULL);
    if (rc) {
	fprintf(stderr, "Can't prepare stmt: %s: %s\n", sql,
	    sqlite3_errmsg(db));
    }
    rc = sqlite3_bind_text(stmt, 1, (const char *)cp, -1, SQLITE_STATIC);
    if (rc) {
	fprintf(stderr, "Can't bind variable: %s: %s: %s\n", sql, cp,
	    sqlite3_errmsg(db));
    }
    rc = sqlite3_bind_int(stmt, 2, bladecount);
    if (rc) {
	fprintf(stderr, "Can't bind variable: %s: %s: %s\n", sql, cp,
	    sqlite3_errmsg(db));
    }

    while (1) {
	rc = sqlite3_step(stmt);
	if (rc == SQLITE_OK) {
	    continue;
	}
	if (rc == SQLITE_ROW) {
	    continue;
	}
	if (rc == SQLITE_DONE) {
	    break;
	}

	printf("_step returned error: %s: %d, %s\n", sql, rc,
	    sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);

    id = sqlite3_last_insert_rowid(db);

    // printf("ID %ld\n",id);

// Loop over the current toolset, and write it out XXX
    blade_id = 0;
    TAILQ_FOREACH(lpc_ctx.np, &head, entries) {

	sql =
	    "INSERT INTO blade (component, toolset, type, pattern) VALUES ( ?, ?, ?, ?);";
	rc = sqlite3_prepare_v2(db, sql, 1024, &stmt, NULL);
	if (rc) {
	    fprintf(stderr, "Can't prepare stmt: %s: %s\n", sql,
		sqlite3_errmsg(db));
	}
	rc = sqlite3_bind_int(stmt, 1, blade_id);
	if (rc) {
	    fprintf(stderr, "Can't bind variable: %s: %s: %s\n", sql, cp,
		sqlite3_errmsg(db));
	}
	rc = sqlite3_bind_int(stmt, 2, id);
	if (rc) {
	    fprintf(stderr, "Can't bind variable: %s: %s: %s\n", sql, cp,
		sqlite3_errmsg(db));
	}
	txtFromType(texttype, lpc_ctx.np->ttype);
	rc = sqlite3_bind_text(stmt, 3, texttype, -1, SQLITE_STATIC);
	if (rc) {
	    fprintf(stderr, "Can't bind variable: %s: %s: %s\n", sql, cp,
		sqlite3_errmsg(db));
	}
	rc = sqlite3_bind_text(stmt, 4, lpc_ctx.np->pattern, -1,
	    SQLITE_STATIC);
	if (rc) {
	    fprintf(stderr, "Can't bind variable: %s: %s: %s\n", sql, cp,
		sqlite3_errmsg(db));
	}

	blade_id++;
//              fprintf(fp,
//                    "action NAMEXXX severity:XXX __%s_ type:%d store:XXX:$1 alert:t
//emplate/foo.template\n",
//                    lpc_ctx.np->pattern,lpc_ctx.np->ttype); // XXX need to humanize ttype
//         
	while (1) {
	    rc = sqlite3_step(stmt);
	    if (rc == SQLITE_OK) {
		continue;
	    }
	    if (rc == SQLITE_ROW) {
		continue;
	    }
	    if (rc == SQLITE_DONE) {
		break;
	    }

	    printf("_step returned error: %s: %d, %s\n", sql, rc,
		sqlite3_errmsg(db));
	}
    }
    sqlite3_finalize(stmt);

    sql = "COMMIT;";

    rc = sqlite3_prepare_v2(db, sql, 1024, &stmt, NULL);
    if (rc) {
	fprintf(stderr, "Can't prepare stmt: %s: %s\n", sql,
	    sqlite3_errmsg(db));
    }
    sqlite3_bind_text(stmt, 1, (const char *)cp, -1, SQLITE_STATIC);
    if (rc) {
	fprintf(stderr, "Can't bind variable: %s: %s: %s\n", sql, cp,
	    sqlite3_errmsg(db));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
	;
    }
    sqlite3_finalize(stmt);

}

void
txtFromType(char *text, Tooltype tt)
{
    switch (tt) {
    case TNONE:
	strcpy(text, "NONE");
	break;
    case STDIN:		/* This should never appear in a database. It's only
				 * a temporary placeholder in filter mode. Implemented
				 * in case it has another future use case */
	strcpy(text, "STDIN");
	break;
    case CAT:
	strcpy(text, "CAT");
	break;
    case BLACKBOX:
	strcpy(text, "BLACKBOX");
	break;
    case EXCLUDE:
	strcpy(text, "EXCLUDE");
	break;
    case FORMAT:
	strcpy(text, "FORMAT");
	break;
    case INCLUDE:
	strcpy(text, "INCLUDE");
	break;
    case SUMMARIZE:
	strcpy(text, "SUMMARIZE");
	break;
    case ORDER:
	strcpy(text, "ORDER");
	break;
    case UNIQUE:
	strcpy(text, "UNIQUE");
	break;

    }
    return;
}
