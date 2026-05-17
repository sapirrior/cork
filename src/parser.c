#include "parser.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
  #include <sys/stat.h>
#endif

Var  vars[MAX_VARS]; int nv = 0;
Cmd  cmds[MAX_CMDS]; int nc = 0;

void parse(void) {
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

            /* parse dependencies */
            {
                char dcopy[MAX_LEN];
                strncpy(dcopy, tok.b, MAX_LEN - 1);
                dcopy[MAX_LEN-1] = '\0';
                char *d = strtok(dcopy, " \t");
                while (d && cmds[nc].ndeps < MAX_LINES) {
                    strncpy(cmds[nc].deps[cmds[nc].ndeps++], d, MAX_LEN - 1);
                    d = strtok(NULL, " \t");
                }
            }

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
