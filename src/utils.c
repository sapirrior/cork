#include "utils.h"
#include <string.h>
#include <ctype.h>

void rtrim(char *s) {
    int n = (int)strlen(s);
    while (n > 0 && (unsigned char)s[n-1] <= ' ') s[--n] = '\0';
}

void ltrim(char *s) {
    int i = 0;
    while (s[i] && (unsigned char)s[i] <= ' ') i++;
    if (i) memmove(s, s + i, strlen(s) - i + 1);
}

void trim(char *s) { rtrim(s); ltrim(s); }

int valid_varname(const char *s) {
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
