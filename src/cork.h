#ifndef CORK_H
#define CORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #define POPEN      _popen
  #define PCLOSE     _pclose
  #define EXIT_RC(r) (r)
#else
  #include <sys/wait.h>
  #include <sys/stat.h>
  #define POPEN      popen
  #define PCLOSE     pclose
  #define EXIT_RC(r) (WIFEXITED(r) ? WEXITSTATUS(r) : 1)
#endif

#define MAX_VARS   128
#define MAX_CMDS   128
#define MAX_LINES   64
#define MAX_LEN    512
#define CORKFILE   "Corkfile"
#define DEFAULT_CMD "def"

/* ── types ──────────────────────────────────────────────── */

typedef struct { char key[MAX_LEN]; char val[MAX_LEN]; } Var;
typedef struct {
    char name[MAX_LEN];
    char deps[MAX_LINES][MAX_LEN];
    int  ndeps;
    char lines[MAX_LINES][MAX_LEN];
    int  nlines;
    int  visited; /* 0: unvisited, 1: visiting, 2: done */
} Cmd;

typedef enum {
    TOK_BLANK,    /* empty or whitespace-only                     */
    TOK_COMMENT,  /* starts with #                                */
    TOK_CMD_HDR,  /* trimmed line ends with ':', contains no '=' */
    TOK_VAR,      /* key = value  (key validated at lex time)     */
    TOK_BARE,     /* anything else: a shell line or stray text    */
    TOK_ERR       /* lexer-detected hard error                    */
} TokType;

typedef struct {
    TokType type;
    int     lineno;
    char    a[MAX_LEN]; /* key (VAR), name (CMD_HDR), text (BARE)  */
    char    b[MAX_LEN]; /* value (VAR only)                         */
    char    msg[MAX_LEN];
} Token;

/* ── global state (declared) ───────────────────────────── */

extern Var vars[MAX_VARS];
extern int nv;
extern Cmd cmds[MAX_CMDS];
extern int nc;

#endif /* CORK_H */
