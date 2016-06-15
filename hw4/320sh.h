#ifndef SH320
#define SH320

#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

// Assume no input line will be longer than 1024 bytes
#define MAX_INPUT 1024

// Define scan codes for keyboard key presses
#define BACKSPACE_KEY 0x7F
#define CTRL_C_CODE 0x3
#define ESCAPE_CODE 0x1B
#define HISTORY_FILE_NAME "320sh_history.txt"

/**
 * Check if the given program is in any of the folder paths in the given environment.
 * @param bin String name of program to check the folders in the PATH for.
 * @param sysenv Environment variable containing folder paths to check.
 * @param program_file_path Address of character buffer to store the absolute
 * filepath of the program if it exists in any of the folders in the given environment,
 * or NULL characters if the given program is not found.
 */
void load_program_path(char *bin, char *sysenv, char *program_file_path);

/**
 * Check if the given filepath exists.
 * @param filepath Filepath to which needs to be verified.
 * @return Returns true when the filepath exists, returns false otherwise.
 */
bool filepath_exists(char* filepath);

/**
 *check for errors in fork()
 *@returns the pid of fork
 */
pid_t Fork(void);

/**
 * Checks if the given string is a valid name for an environmental variable.
 * @param env_var_name Address of a Null-teminated string containing the name to
 * verify
 * @param length The character length of the given string to verify.
 * @return Returns true when the string is a valid name for an environmental
 * variable, namely containing only ASCII letters (upper and lowercase), ASCII
 * digits (0 - 9), and the underscore (_), but not starting with an ASCII digit,
 * and the passed length of the name string is greater than zero.  Returns false
 * otherwise.
 */
bool valid_env_var_name(char *env_var_name, size_t length);

/**
 * Saves the current command executed by the shell into the command history.
 * @param cmd Address of the character array containing the command that was
 * executed.
 * @param cmd_history Address of the shell command history array.
 * @param cmd_history_size Address of the variable counting the number of commands
 * saved into the command history.
 */
void save_current_cmd(char *cmd);

/**
 * Loads the command history of the shell into memory from the command history
 * file.
 * @param cmd_history Address of the shell command history array.
 * @param cmd_history_size Address of the variable counting the number of commands
 * saved into the command history.
 */
void load_cmd_history();

/**
 * Saves the contents of the command history into the command history file, then
 * frees the memmory allocated to store the command history.
 * @param cmd_history Address of the shell command history array.
 * @param cmd_history_size Address of the variable counting the number of commands
 * saved into the command history.
 */
void close_cmd_history();

/**
 * Print out the program usage string
 */
#define USAGE(name) do {                                                                  \
    fprintf(stderr,                                                                       \
        "\npwd [-LP]  \n"                                                                \
        "cd [-L|[-P [-e]] [-@]] [dir]\n"                                                  \
        "echo [-neE] [arg ...] \n"                                                        \
        "exec [-cl] [-a name] [command [argume>\n"                                        \
        "help [-dms] [pattern ...]  \n"                                                   \
        "wait [-n] [id ...]\n"                                                            \
        "suspend [-f]\n"                                                                  \
        "kill [-s sigspec | -n signum | -sigs>\n"                                         \
        "exit [n]\n"                                                                      \
        "set [-abefhkmnptuvxBCHP] [-o option->\n"                                         \
        "read [-ers] [-a array] [-d delim] [->\n"                                         \
        "break [n]\n"                                                                     \
    );                                                                                    \
} while(0)
#endif
