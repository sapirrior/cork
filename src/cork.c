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
  #define POPEN      popen
  #define PCLOSE     pclose
  #define EXIT_RC(r) (WIFEXITED(r) ? WEXITSTATUS(r) : 1)
#endif

#define MAX_VARS   128
#define MAX_CMDS   128
#define MAX_LINES   64
#define MAX_LEN    512
#define CORKFILE   "Corkfile"

typedef struct { char key[MAX_LEN]; char val[MAX_LEN]; } Var;
typedef struct { char name[MAX_LEN]; char lines[MAX_LINES][MAX_LEN]; int nlines; } Cmd;

static Var vars[MAX_VARS]; static int nv = 0;
static Cmd cmds[MAX_CMDS]; static int nc = 0;

/* ── helpers ──────────────────────────────────────────── */

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

/* validate var name: lowercase letters, digits, underscore only */
static int valid_varname(const char *s) {
    if (!*s) return 0;
    for (int i = 0; s[i]; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return 0;
    }
    return 1;
}

/* ── expand ${var} ────────────────────────────────────── */

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
                fprintf(stderr, "CorkE: invalid variable name\n");
                return -1;
            }
            char vname[MAX_LEN] = {0};
            strncpy(vname, in + i + 2, (size_t)len);
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
    /* if input wasn't exhausted, the output buffer was too small */
    if (in[i]) {
        fprintf(stderr, "CorkE: expansion too long\n");
        return -1;
    }
    out[o] = '\0';
    return 0;
}

/* ── parser ───────────────────────────────────────────── */

static void parse(void) {
    FILE *f = fopen(CORKFILE, "r");
    if (!f) { fprintf(stderr, "CorkE: cannot open Corkfile\n"); exit(1); }

    char line[MAX_LEN + 2]; /* +2 to detect overlong lines */
    int in_cmd    = -1;
    int vars_closed = 0;
    int lineno    = 0;

    while (fgets(line, sizeof line, f)) {
        lineno++;

        /* detect line too long: fgets filled the buffer without hitting \n */
        if (strlen(line) == MAX_LEN + 1 && line[MAX_LEN] != '\n') {
            fprintf(stderr, "CorkE: line %d too long (max %d chars)\n", lineno, MAX_LEN);
            exit(1);
        }

        /* FIX BUG 5: blank line closes current command body */
        rtrim(line);
        if (!line[0]) { in_cmd = -1; continue; }

        /* comments never affect state */
        if (line[0] == '#') continue;

        ltrim(line);
        if (!line[0]) continue;

        int len = (int)strlen(line);

        /* command header: ends with ':', contains no '=' */
        if (line[len - 1] == ':' && !strchr(line, '=')) {
            if (nc >= MAX_CMDS) { fprintf(stderr, "CorkE: too many commands\n"); exit(1); }
            line[len - 1] = '\0'; trim(line);
            if (!line[0]) { fprintf(stderr, "CorkE: line %d: empty command name\n", lineno); exit(1); }
            /* FIX BUG 3: duplicate command name */
            for (int i = 0; i < nc; i++) {
                if (strcmp(cmds[i].name, line) == 0) {
                    fprintf(stderr, "CorkE: line %d: duplicate command '%s'\n", lineno, line);
                    exit(1);
                }
            }
            Cmd *c = &cmds[nc];
            memset(c, 0, sizeof *c);
            strncpy(c->name, line, MAX_LEN - 1);
            in_cmd = nc++;
            vars_closed = 1;
            continue;
        }

        /* FIX BUG 1: shell body line — check in_cmd BEFORE variable checks
           so body lines containing '=' are never misread as var declarations */
        if (in_cmd >= 0) {
            if (cmds[in_cmd].nlines >= MAX_LINES) {
                fprintf(stderr, "CorkE: too many lines in '%s'\n", cmds[in_cmd].name);
                exit(1);
            }
            strncpy(cmds[in_cmd].lines[cmds[in_cmd].nlines++], line, MAX_LEN - 1);
            continue;
        }

        /* variable: only allowed before any command */
        char *eq = strchr(line, '=');
        if (eq && !vars_closed) {
            if (nv >= MAX_VARS) { fprintf(stderr, "CorkE: too many variables\n"); exit(1); }
            *eq = '\0';
            char *key = line, *val = eq + 1;
            trim(key); trim(val);
            if (!valid_varname(key)) {
                fprintf(stderr, "CorkE: line %d: invalid variable name '%s' (lowercase, digits, _ only)\n", lineno, key);
                exit(1);
            }
            /* FIX BUG 2: duplicate variable name */
            for (int i = 0; i < nv; i++) {
                if (strcmp(vars[i].key, key) == 0) {
                    fprintf(stderr, "CorkE: line %d: duplicate variable '%s'\n", lineno, key);
                    exit(1);
                }
            }
            strncpy(vars[nv].key, key, MAX_LEN - 1);
            strncpy(vars[nv].val, val, MAX_LEN - 1);
            vars[nv].key[MAX_LEN-1] = vars[nv].val[MAX_LEN-1] = '\0';
            nv++;
            continue;
        }

        /* stray '=' outside command body and after vars section */
        if (eq && vars_closed) {
            fprintf(stderr, "CorkE: line %d: variable declaration after command\n", lineno);
            exit(1);
        }

        /* unrecognized line outside any command — skip silently */
    }
    fclose(f);
}

/* ── runner ───────────────────────────────────────────── */

static int run_cmd(Cmd *c) {
    for (int l = 0; l < c->nlines; l++) {
        char expanded[MAX_LEN * 4];
        if (expand(c->lines[l], expanded, sizeof expanded) != 0) return 1;
        printf("%s\n", expanded);
        fflush(stdout);
        FILE *p = POPEN(expanded, "r");
        if (!p) { fprintf(stderr, "CorkE: failed to run: %s\n", expanded); return 1; }
        char buf[4096];
        while (fgets(buf, sizeof buf, p));
        int raw = PCLOSE(p);
        int rc  = EXIT_RC(raw);
        if (rc != 0) { fprintf(stderr, "CorkE: failed (exit %d): %s\n", rc, expanded); return rc; }
    }
    return 0;
}

/* ── main ─────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc < 2
     || strcmp(argv[1], "--help") == 0
     || strcmp(argv[1], "-h")     == 0) {
        puts("cork - minimal command runner");
        puts("Usage: cork [-h] [-c] <command>");
        puts("  -h, --help   show this message");
        puts("  -c, --cmds   list available commands");
        return 0;
    }

    parse();

    if (strcmp(argv[1], "--cmds") == 0 || strcmp(argv[1], "-c") == 0) {
        for (int i = 0; i < nc; i++) puts(cmds[i].name);
        return 0;
    }

    for (int i = 0; i < nc; i++)
        if (strcmp(cmds[i].name, argv[1]) == 0)
            return run_cmd(&cmds[i]);

    fprintf(stderr, "CorkE: unknown command: %s\n", argv[1]);
    return 1;
}
