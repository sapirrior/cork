# CORK - MINIMAL COMMAND RUNNER

1. OVERVIEW
Cork is a lightweight, minimal command runner written in C. It parses a 
configuration file named "Corkfile" to execute predefined shell commands and 
evaluate variables.

2. COMPILATION
Compile the source code using a standard C compiler. It is compatible with both 
POSIX and Windows environments.

       gcc -o cork cork.c

3. USAGE
Execute the compiled binary from the directory containing your "Corkfile".

       cork <command>      Execute the specified command block.
       cork -c, --cmds     List all available commands defined in the Corkfile.
       cork -h, --help     Display usage instructions.

4. CORKFILE SYNTAX
The "Corkfile" must adhere to the following formatting rules:

  * Variables: Declared at the top of the file before any commands. 
               Format: key=value
               Constraints: Keys must contain only lowercase letters, digits, 
               or underscores.
  * Commands:  Declared with a trailing colon (e.g., build:). Subsequent 
               lines form the command body. A blank line terminates the body.
  * Expansion: Variables are referenced within command bodies using the 
               syntax: ${key}
  * Comments:  Lines beginning with '#' are strictly ignored.

5. EXAMPLE CORKFILE
### Variable declarations
compiler=gcc

target=program

### Command definitions
build:
    ${compiler} -o ${target} main.c

clean:
    rm -f ${target}g
