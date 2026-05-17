#include "runner.h"
#include "expander.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
  #include <direct.h>
#endif

static int run_builtin(char *cmd) {
    char *s = cmd + 1;
    while (*s == ' ') s++;

    if (strncmp(s, "rm ", 3) == 0) {
        char *path = s + 3;
        while (*path == ' ') path++;
        if (*path) remove(path);
        return 0;
    }
    if (strncmp(s, "mkdir ", 6) == 0) {
        char *path = s + 6;
        while (*path == ' ') path++;
        if (*path) {
#ifdef _WIN32
            _mkdir(path);
#else
            mkdir(path, 0777);
#endif
        }
        return 0;
    }
    fprintf(stderr, "CorkE: unknown built-in: %s\n", cmd);
    return 1;
}

int run_cmd(Cmd *c) {
    if (c->visited == 1) {
        fprintf(stderr, "CorkE: cyclic dependency detected at '%s'\n", c->name);
        return 1;
    }
    if (c->visited == 2) return 0;

    c->visited = 1;

    for (int i = 0; i < c->ndeps; i++) {
        Cmd *d = find_cmd(c->deps[i]);
        if (!d) {
            fprintf(stderr, "CorkE: unknown dependency '%s' for '%s'\n", c->deps[i], c->name);
            return 1;
        }
        int rc = run_cmd(d);
        if (rc != 0) return rc;
    }

    for (int l = 0; l < c->nlines; l++) {
        char expanded[MAX_LEN * 4];
        if (expand(c->lines[l], expanded, sizeof expanded) != 0) return 1;
        if (!expanded[0]) continue;

        printf("%s\n", expanded);
        fflush(stdout);

        if (expanded[0] == '@') {
            int rc = run_builtin(expanded);
            if (rc != 0) return rc;
            continue;
        }

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

    c->visited = 2;
    return 0;
}

Cmd *find_cmd(const char *name) {
    for (int i = 0; i < nc; i++)
        if (strcmp(cmds[i].name, name) == 0) return &cmds[i];
    return NULL;
}
