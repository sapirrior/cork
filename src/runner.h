#ifndef RUNNER_H
#define RUNNER_H

#include "cork.h"

int run_cmd(Cmd *c);
Cmd *find_cmd(const char *name);

#endif /* RUNNER_H */
