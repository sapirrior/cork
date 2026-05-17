#include "cork.h"
#include "parser.h"
#include "runner.h"
#include <stdio.h>
#include <string.h>

static void help(void) {
    puts("Cork - Core Operations & Runtime Kernel");
    puts("Usage: cork [-h] [-c] <command>");
    puts("  -h, --help   show this message");
    puts("  -c, --cmds   list available commands");
    puts("  (no args)    run the 'def' command if defined in Corkfile");
}

static void inject_var(const char *arg) {
    char *eq = strchr(arg, '=');
    if (!eq) return;
    char key[MAX_LEN], val[MAX_LEN];
    size_t klen = eq - arg;
    if (klen >= MAX_LEN) klen = MAX_LEN - 1;
    strncpy(key, arg, klen);
    key[klen] = '\0';
    strncpy(val, eq + 1, MAX_LEN - 1);
    val[MAX_LEN - 1] = '\0';

    for (int i = 0; i < nv; i++) {
        if (strcmp(vars[i].key, key) == 0) {
            strncpy(vars[i].val, val, MAX_LEN - 1);
            return;
        }
    }
    if (nv < MAX_VARS) {
        strncpy(vars[nv].key, key, MAX_LEN - 1);
        strncpy(vars[nv].val, val, MAX_LEN - 1);
        nv++;
    }
}

int main(int argc, char **argv) {
    parse();

    char *target = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            help(); return 0;
        }
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cmds") == 0) {
            for (int j = 0; j < nc; j++) puts(cmds[j].name);
            return 0;
        }
        if (strchr(argv[i], '=')) {
            inject_var(argv[i]);
        } else if (!target) {
            target = argv[i];
        }
    }

    if (!target) target = DEFAULT_CMD;

    Cmd *c = find_cmd(target);
    if (c) return run_cmd(c);

    if (strcmp(target, DEFAULT_CMD) == 0) {
        help(); return 0;
    }

    fprintf(stderr, "CorkE: unknown command: %s\n", target);
    return 1;
}
