#include "lexer.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>

Token lex_line(const char *raw, int lineno) {
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

    /* command header: has ':', no '=' before it */
    char *colon = strchr(s, ':');
    if (colon && (!strchr(s, '=') || strchr(s, '=') > colon)) {
        *colon = '\0';
        char *name = s; trim(name);
        char *deps = colon + 1; trim(deps);
        if (!name[0]) {
            t.type = TOK_ERR;
            snprintf(t.msg, MAX_LEN, "empty command name");
            return t;
        }
        t.type = TOK_CMD_HDR;
        strncpy(t.a, name, MAX_LEN - 1);
        strncpy(t.b, deps, MAX_LEN - 1);
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
