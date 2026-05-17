#include "expander.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>

int expand(const char *in, char *out, int outsz) {
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
