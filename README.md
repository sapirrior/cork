# Cork

Cork - Core Operations & Runtime Kernel

Cork is a minimal command runner. You define variables and commands in a file called `Corkfile`, and run them by name. It is designed to be simple, predictable, and easy to use from day one.

---

## Installation

You need a C compiler and `make`. Run:

```sh
make
```

Then move the binary somewhere on your `PATH`, such as `/usr/local/bin`:

```sh
mv cork /usr/local/bin/cork
```

On Windows with MSVC:

```sh
cl cork.c /Fe:cork.exe
```

---

## Quick Start

Create a file named `Corkfile` in your project directory:

```
name = myapp
src = main.c

def:
gcc -o ${name} ${src}

clean:
rm -f ${name}
```

Then run:

```sh
cork          # compiles (runs the default command)
cork clean    # removes the binary
cork -c       # lists all available commands
```

---

## The Corkfile

A `Corkfile` has two parts: variables at the top, and commands below.

### Variables

Variables are declared before any command. A variable name must start with a lowercase letter or underscore, followed by lowercase letters, digits, or underscores.

```
name = myapp
version = 1.0
build_dir = ./out
```

Whitespace around the `=` sign is trimmed automatically. These are not allowed:

- Declaring a variable more than once
- Declaring a variable after a command has been defined
- Using uppercase letters in a variable name

### Commands

A command starts with its name followed by a colon. Every non-blank line beneath it is a shell command that will be run in order.

```
build:
gcc -o ${name} ${src}
echo done

test:
./run_tests.sh
```

A blank line or the start of a new command ends the current block. Duplicate command names are not allowed.

### Variable Expansion

Use `${varname}` inside a command body to insert a variable's value. Lookup is case-sensitive and must match the declared name exactly. Referencing a variable that was not declared is an error.

```
name = cork

greet:
echo Hello from ${name}
```

Shell variable assignments inside a command body (such as `X=hello`) are passed directly to the shell and are not treated as Corkfile variable declarations.

### The Default Command

A command named `def` runs automatically when you type `cork` with no arguments. If no `def` command is defined, the help message is shown.

```
def:
echo this runs by default
```

### Comments

Lines beginning with `#` are ignored.

```
# build the project
build:
gcc -o ${name} ${src}
```

---

## Reference

### Command Line

```
cork [-h] [-c] <command>
```

| Flag | Long form | What it does |
|------|-----------|--------------|
| `-h` | `--help`  | Show the help message |
| `-c` | `--cmds`  | List all commands defined in Corkfile |

### Limits

| Item | Limit |
|------|-------|
| Variables | 128 |
| Commands | 128 |
| Lines per command | 64 |
| Line length | 512 characters |

---

## Error Reference

All errors are printed to stderr in the format `CorkE: <message>`. Cork always exits with a non-zero status on error.

| Error | What caused it |
|-------|----------------|
| `cannot open Corkfile` | No Corkfile found in the current directory |
| `Corkfile is a directory, not a file` | A directory named Corkfile exists instead of a file |
| `line N: line too long` | A line exceeds 512 characters |
| `line N: duplicate variable 'x'` | The same variable name was declared twice |
| `line N: variable 'x' declared after a command` | A variable appears after the first command |
| `line N: unexpected text outside command block` | A line appears outside any command and is not a variable or comment |
| `line N: duplicate command 'x'` | The same command name was declared twice |
| `undefined variable: x` | A `${x}` reference has no matching variable declaration |
| `unclosed ${ in: ...` | A `${` is missing its closing `}` |
| `command expands to empty string` | A command line expanded to nothing after variable substitution |
| `failed (exit N): ...` | A shell command exited with a non-zero status; execution stops immediately |
| `unknown command: x` | The command does not exist in Corkfile |

---

## A Full Example

```
# Corkfile
name = myapp
src = main.c
flags = -Wall -O2

def:
gcc ${flags} -o ${name} ${src}

clean:
rm -f ${name}

run:
./${name}

check:
gcc ${flags} -fsyntax-only ${src}
echo syntax ok
```

```sh
cork          # compiles myapp (runs def)
cork clean    # removes myapp
cork run      # runs myapp
cork check    # checks syntax only
cork -c       # lists: def, clean, run, check
```
