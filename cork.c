#define _POSIX_C_SOURCE 200809L
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
typedef struct { char name[MAX_LEN]; char lines[MAX_LINES][MAX_LEN]; int nlines; } Cmd;

static Var  vars[MAX_VARS]; static int nv = 0;
static Cmd  cmds[MAX_CMDS]; static int nc = 0;

/* ── token types ────────────────────────────────────────── */
/*
   The lexer classifies each line purely by its content —
   no parser state is passed in. The parser then interprets
   tokens according to the current context (inside/outside cmd).
*/
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

/* ── string helpers ─────────────────────────────────────── */

static void rtrim(char *s) {
    int n = (int)strlen(s);
    while (n > 0 && (unsigned char)s[n-1] <= ' ') s[--n] = '\0';
}
static void ltrim(char *s) {
    int i = 0;
    while (s[i] && (unsigned char)s[i] <= ' ') i++;
    if (i) memmove(s, s + i, strlen(s) - i + 1);
}
static void trim(char *s) { rtrim(s); ltrim(s); }

static int valid_varname(const char *s) {
    if (!*s) return 0;
    /* first char: letter or underscore only */
    if (!((s[0] >= 'a' && s[0] <= 'z') || s[0] == '_')) return 0;
    for (int i = 1; s[i]; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return 0;
    }
    return 1;
}

/* ── lexer ──────────────────────────────────────────────── */

static Token lex_line(const char *raw, int lineno) {
    Token t;
    memset(&t, 0, sizeof t);
    t.lineno = lineno;

    char line[MAX_LEN];
    strncpy(line, raw, MAX_LEN - 1);
    line[MAX_LEN - 1] = '\0';
    rtrim(line);

    /* blank */
    {
        char tmp[MAX_LEN];
        strncpy(tmp, line, MAX_LEN - 1);
        ltrim(tmp);
        if (!tmp[0]) { t.type = TOK_BLANK; return t; }
    }

    /* trim for classification — preserve in line[] for BARE */
    char s[MAX_LEN];
    strncpy(s, line, MAX_LEN - 1);
    ltrim(s);

    /* comment */
    if (s[0] == '#') { t.type = TOK_COMMENT; return t; }

    int len = (int)strlen(s);

    /* command header: ends with ':', no '=' */
    if (s[len - 1] == ':' && !strchr(s, '=')) {
        s[len - 1] = '\0'; trim(s);
        if (!s[0]) {
            t.type = TOK_ERR;
            snprintf(t.msg, MAX_LEN, "empty command name");
            return t;
        }
        t.type = TOK_CMD_HDR;
        strncpy(t.a, s, MAX_LEN - 1);
        return t;
    }

    /* variable: has '=', and key passes varname rules */
    char *eq = strchr(s, '=');
    if (eq) {
        char tmp[MAX_LEN];
        strncpy(tmp, s, MAX_LEN - 1);
        char *teq = strchr(tmp, '=');
        *teq = '\0';
        trim(tmp); /* tmp is now just the key */
        if (valid_varname(tmp)) {
            char *val = teq + 1; ltrim(val);
            t.type = TOK_VAR;
            strncpy(t.a, tmp, MAX_LEN - 1);
            strncpy(t.b, val, MAX_LEN - 1);
            return t;
        }
        /* key not a valid varname → BARE (valid shell assignment or other) */
    }

    /* everything else is a bare line (shell command or assignment) */
    t.type = TOK_BARE;
    strncpy(t.a, s, MAX_LEN - 1);
    return t;
}

/* ── parser ─────────────────────────────────────────────── */
/*
   Context rules:
   - TOK_VAR   outside cmd block → Corkfile variable declaration
   - TOK_VAR   inside  cmd block → shell body line (e.g. X=hello)
   - TOK_CMD_HDR anywhere        → opens/reopens a new command block
   - TOK_BARE  outside cmd block → hard error (stray text)
   - TOK_BARE  inside  cmd block → shell body line
   - TOK_BLANK                   → closes current command block
*/

static void parse(void) {
    FILE *f = fopen(CORKFILE, "r");
    if (!f) { fprintf(stderr, "CorkE: cannot open Corkfile\n"); exit(1); }
#ifndef _WIN32
    {
        struct stat st;
        if (fstat(fileno(f), &st) == 0 && S_ISDIR(st.st_mode)) {
            fclose(f);
            fprintf(stderr, "CorkE: Corkfile is a directory, not a file\n");
            exit(1);
        }
    }
#endif

    char raw[MAX_LEN + 2]; /* +2 to detect overlong lines */
    int  lineno     = 0;
    int  in_cmd     = 0;
    int  vars_closed = 0;

    while (fgets(raw, sizeof raw, f)) {
        lineno++;

        if (strlen(raw) == MAX_LEN + 1 && raw[MAX_LEN] != '\n') {
            fprintf(stderr, "CorkE: line %d: line too long (max %d chars)\n",
                    lineno, MAX_LEN);
            exit(1);
        }

        Token tok = lex_line(raw, lineno);

        switch (tok.type) {

        case TOK_BLANK:
            in_cmd = 0;
            break;

        case TOK_COMMENT:
            break;

        case TOK_CMD_HDR:
            /* a new header always closes any open block and starts a new one */
            for (int i = 0; i < nc; i++) {
                if (strcmp(cmds[i].name, tok.a) == 0) {
                    fprintf(stderr, "CorkE: line %d: duplicate command '%s'\n",
                            lineno, tok.a);
                    exit(1);
                }
            }
            if (nc >= MAX_CMDS) {
                fprintf(stderr, "CorkE: too many commands (max %d)\n", MAX_CMDS);
                exit(1);
            }
            memset(&cmds[nc], 0, sizeof cmds[nc]);
            strncpy(cmds[nc].name, tok.a, MAX_LEN - 1);
            in_cmd     = 1;
            vars_closed = 1;
            nc++;
            break;

        case TOK_VAR:
            if (in_cmd) {
                /* inside a command: treat as a shell body line */
                Cmd *c = &cmds[nc - 1];
                if (c->nlines >= MAX_LINES) {
                    fprintf(stderr, "CorkE: line %d: too many lines in '%s'\n",
                            lineno, c->name);
                    exit(1);
                }
                /* reconstruct the original assignment for the shell */
                char body[MAX_LEN];
                snprintf(body, MAX_LEN, "%.254s=%.254s", tok.a, tok.b);
                strncpy(c->lines[c->nlines++], body, MAX_LEN - 1);
                break;
            }
            if (vars_closed) {
                fprintf(stderr,
                    "CorkE: line %d: variable '%s' declared after a command "
                    "(all variables must be at the top of Corkfile)\n",
                    lineno, tok.a);
                exit(1);
            }
            for (int i = 0; i < nv; i++) {
                if (strcmp(vars[i].key, tok.a) == 0) {
                    fprintf(stderr, "CorkE: line %d: duplicate variable '%s'\n",
                            lineno, tok.a);
                    exit(1);
                }
            }
            if (nv >= MAX_VARS) {
                fprintf(stderr, "CorkE: too many variables (max %d)\n", MAX_VARS);
                exit(1);
            }
            strncpy(vars[nv].key, tok.a, MAX_LEN - 1);
            strncpy(vars[nv].val, tok.b, MAX_LEN - 1);
            nv++;
            break;

        case TOK_BARE:
            if (!in_cmd) {
                fprintf(stderr, "CorkE: line %d: unexpected text outside command block: %s\n",
                        lineno, tok.a);
                exit(1);
            }
            {
                Cmd *c = &cmds[nc - 1];
                if (c->nlines >= MAX_LINES) {
                    fprintf(stderr, "CorkE: line %d: too many lines in '%s'\n",
                            lineno, c->name);
                    exit(1);
                }
                strncpy(c->lines[c->nlines++], tok.a, MAX_LEN - 1);
            }
            break;

        case TOK_ERR:
            fprintf(stderr, "CorkE: line %d: %s\n", lineno, tok.msg);
            exit(1);
        }
    }
    fclose(f);
}

/* ── expand ${var} ──────────────────────────────────────── */

static int expand(const char *in, char *out, int outsz) {
    int i = 0, o = 0;
    while (in[i] && o < outsz - 1) {
        if (in[i] == '$' && in[i+1] == '{') {
            int j = i + 2;
            while (in[j] && in[j] != '}') j++;
            if (!in[j]) {
                fprintf(stderr, "CorkE: unclosed ${ in: %s\n", in);
                return -1;
            }
            int len = j - (i + 2);
            if (len <= 0 || len >= MAX_LEN) {
                fprintf(stderr, "CorkE: invalid variable reference\n");
                return -1;
            }
            char vname[MAX_LEN] = {0};
            strncpy(vname, in + i + 2, (size_t)len);
            trim(vname); /* tolerate spaces inside ${ } */
            const char *found = NULL;
            for (int k = 0; k < nv; k++)
                if (strcmp(vars[k].key, vname) == 0) { found = vars[k].val; break; }
            if (!found) {
                fprintf(stderr, "CorkE: undefined variable: %s\n", vname);
                return -1;
            }
            int fl = (int)strlen(found);
            if (o + fl >= outsz - 1) {
                fprintf(stderr, "CorkE: expansion too long\n");
                return -1;
            }
            memcpy(out + o, found, (size_t)fl);
            o += fl;
            i = j + 1;
        } else {
            out[o++] = in[i++];
        }
    }
    if (in[i]) { fprintf(stderr, "CorkE: expansion too long\n"); return -1; }
    out[o] = '\0';
    return 0;
}

/* ── runner ─────────────────────────────────────────────── */

static int run_cmd(Cmd *c) {
    for (int l = 0; l < c->nlines; l++) {
        char expanded[MAX_LEN * 4];
        if (expand(c->lines[l], expanded, sizeof expanded) != 0) return 1;
        if (!expanded[0]) {
            fprintf(stderr, "CorkE: command expands to empty string\n");
            return 1;
        }
        printf("%s\n", expanded);
        fflush(stdout);
        FILE *p = POPEN(expanded, "r");
        if (!p) { fprintf(stderr, "CorkE: failed to run: %s\n", expanded); return 1; }
        char buf[4096];
        while (fgets(buf, sizeof buf, p));
        int raw_rc = PCLOSE(p);
        int rc     = EXIT_RC(raw_rc);
        if (rc != 0) {
            fprintf(stderr, "CorkE: failed (exit %d): %s\n", rc, expanded);
            return rc;
        }
    }
    return 0;
}

static Cmd *find_cmd(const char *name) {
    for (int i = 0; i < nc; i++)
        if (strcmp(cmds[i].name, name) == 0) return &cmds[i];
    return NULL;
}

/* ── help ───────────────────────────────────────────────── */

static void help(void) {
    puts("Cork - Core Operations & Runtime Kernel");
    puts("Usage: cork [-h] [-c] <command>");
    puts("  -h, --help   show this message");
    puts("  -c, --cmds   list available commands");
    puts("  (no args)    run the 'def' command if defined in Corkfile");
}

/* ── main ───────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc >= 2 &&
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        help(); return 0;
    }

    if (argc < 2) {
        parse();
        Cmd *def = find_cmd(DEFAULT_CMD);
        if (!def) { help(); return 0; }
        return run_cmd(def);
    }

    parse();

    if (strcmp(argv[1], "--cmds") == 0 || strcmp(argv[1], "-c") == 0) {
        for (int i = 0; i < nc; i++) puts(cmds[i].name);
        return 0;
    }

    Cmd *c = find_cmd(argv[1]);
    if (c) return run_cmd(c);

    fprintf(stderr, "CorkE: unknown command: %s\n", argv[1]);
    return 1;
}
