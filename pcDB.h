// # vim: shiftwidth=4 tabstop=4 softtabstop=4 expandtab
// # indent: -bap -br -ce -ci4 -cli0 -d0 -di0 -i4 -ip -l79 -nbc -ncdb -ndj -ei -nfc1 -nlp -npcs - psl - sc - sob
// # Gnu indent: -bap -nbad -br -ce -ci4 -cli0 -d0 -di0 -i4 -ip4 -l79 -nbc -ncdb -ndj -nfc1 -nlp - npcs - psl - sc - sob
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
#ifndef PIPECUTDB_H
#define PIPECUTDB_H
#include <stdio.h>
#include <sqlite3.h>
void initDB();
void closeDB();

int callback(void *NotUsed, int argc, char **argv, char **azColName);
void txtFromType(char *text, Tooltype tt);

#endif
