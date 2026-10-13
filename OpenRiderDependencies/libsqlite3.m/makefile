# SQLite (public domain) amalgamation, fetched from sqlite.org and built as
# a dedicated static archive, linked statically by its users.
# @impl 0f87-6acd-24ce-8c9e
LIB=sqlite3
IMPORT=fetch:sqlite
FETCH_URL=https://sqlite.org/2026/sqlite-amalgamation-3540000.zip
SRCS=sqlite3.c
IMPORT_HEADERS=sqlite3.h
LIB_SHARED=NO
WARN=none
CFLAGS += -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DEFAULT_FOREIGN_KEYS=1
.include "mk.lib.mk"
