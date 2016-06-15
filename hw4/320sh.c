#include "320sh.h"

/* Define values for shell that have scope between all functions */
int dFlag = 0; /* Flag to print debug information */
int tFlag = 0; /*Flag for time option printing*/
int last_program_return_code = 0; /* Return value for the last prograam run in the shell */
char current_working_dir[MAX_INPUT] = { '\0' };
char last_working_dir[MAX_INPUT] = { '\0' };
char *cmd_history[50] = { NULL };
int cmd_history_size = 0;
int history_counter=0;
int up_arrow_counter=1;
int bottom=1;

/* Define multibyte keycodes for keyboard key inputs that generate multibyte input */
const char *UP_ARROW_KEY = "\e[A";
const char *DOWN_ARROW_KEY = "\e[B";
const char *LEFT_ARROW_KEY = "\e[D";
const char *RIGHT_ARROW_KEY = "\e[C";
const char *ALT_B_CODE = "\eb";
const char *ALT_F_CODE = "\ef";

void eval_cmd(char *cmdline, char **envp) {
    /* Define variables needed for command evaluation */
    pid_t pid; // Counter for Child Process ID
    int child_status = 0; // Holds status of child process
    char child_program_path[512] = { '\0' };
    char *child_program_args[514] = { "\0" };
    int num_child_program_args = 0;
    char *tok; // Holds tokenized argument from command line to evaluate
    char *saveptr = NULL;
    char *env = getenv("PATH"); // Retrieve PATH environmental variable

    /* Create buffer to hold modified command line */
    char cmd[MAX_INPUT];

    /* Copy given command line into command line buffer */
    strcpy(cmd, cmdline);

    /* Check for empty input to shell */
    if(cmd[0] == '\0') {
        /* Do nothing, as the command is empty */
        return;
    }

    /* Otherwise, start parsing the command line buffer and determine how to
     * evaluate it
     */
    else {
        /* Tokenize first argument of command line buffer */
        tok = strtok_r(cmd, " ", &saveptr);

        /* Command was just newline, so do nothing */
        if(tok == NULL) {
            return;
        }

        /* Otherwise, continue to tokenize the command into the arguments array */
        while (tok != NULL) {
            /* Store the program being executed in the argument array */
            child_program_args[num_child_program_args++] = tok;
            tok = strtok_r(NULL, " ", &saveptr);
        }
        /* Null-teminate child arguments */
        child_program_args[num_child_program_args] = 0;

        /* Check if program exists in the PATH */
        load_program_path(child_program_args[0], env, child_program_path);

        /* Check if first tokenized argument is a specific built-in or a filepath
         * of a program to execute */

        /* Check for absolute or relative file path of program to execute */
        if( *child_program_args[0] == '.' || *child_program_args[0] == '/' ) {
            /* Check if the tokenized file to execute ends with an invalid character */
            if(child_program_args[0][strlen(child_program_args[0]) - 1] == '.'
                || child_program_args[0][strlen(child_program_args[0]) - 1] == '/'
                || child_program_args[0][strlen(child_program_args[0]) - 1] == '\\') {

                /* Indicate invalid program path */
                fprintf(stderr, "\"%s\" is not a valid program executable path.\n",
                    child_program_args[0]);

                /* Set return code to indicate error occurred */
                /* Error is that command is not executable */
                last_program_return_code = 126;

                return;
            }

            /* Check whether parsing of command buffer and execution of the
             * tokenized first argument is possible */
            if(filepath_exists(child_program_args[0])) {
                /* Execute program in child process */
                if((pid = Fork()) == 0) {
                    /* Execute desired program with given arguments */
                    if(execv(*child_program_args, child_program_args) < 0) {
                        /* Print return code and ERRNO if child process fails */
                        fprintf(stderr, "%s\n", strerror(errno));

                        /* Terminate child process after indicating error occurred */
                        exit(EXIT_FAILURE);
                    }
                }
                /* Main shell process will wait until child process terminates */
                else {
                    /* Wait until the forked child process returns */
                    waitpid(pid, &child_status, 0);

                    /* Check if the child process exited normally */
                    if(WIFEXITED(child_status)) {
                        /* Set return code to code returned by the child */
                        last_program_return_code = WEXITSTATUS(child_status);
                    }
                    /* Otherwise, set return code to indicate child exited abnormally */
                    else{
                        /* Set return code to indicate error occurred */
                        last_program_return_code = EXIT_FAILURE;
                    }
                }
            }
            /* Specify that desired program does not exist */
            else {
                printf("Program \"%s\" does not exist.\n", child_program_args[0]);

                /* Set return code to indicate error occurred */
                /* Error is that command was not found */
                last_program_return_code = 127;
            }
        }

        /* Check if program to execute is 'pwd' */
        else if(strcmp(child_program_args[0], "pwd") == 0) {
            /* Retrieve the current working directory */
            char buffer[MAX_INPUT];
            char* current_path_name;
            current_path_name = getcwd(buffer, MAX_INPUT);

            /* Check if the buffer loaded with the current working directory */
            if(current_path_name != NULL) {
                /* Print retrieved current working directory */
                printf("%s\n",current_path_name);
                /* Set return code to indicate success */
                last_program_return_code = 0;
            }

            /* Otherwise, indicate error in retrieving current working directory */
            else {
                printf("Error retrieving current working directory.\n");
                /* Set return code to indicate error occurred */
                last_program_return_code = 1;
            }
        }
        /* Check if program to execute is 'cd' */
        else if(strcmp(child_program_args[0], "cd") == 0) {
            /* Temporarily store the current working directory */
            char *temp;
            char temp_buffer[MAX_INPUT];

            /* When the command arguments are empty */
            if(child_program_args[1] == NULL) {
                /* Retrieve the current working directory */
                if((temp = getcwd(temp_buffer, MAX_INPUT)) != NULL) {
                    /* Get current user home directory */
                    char *home_dir = getenv("HOME");

                    /* Update the working directory states only when directory change
                     * succeeds
                     */
                    if(chdir(home_dir) == 0) {
                        /* Update the previous working directory */
                        strcpy(last_working_dir, temp);
                        setenv("OLDPWD", temp, 1);

                        /* Update the current working directory */
                        char current_buffer[MAX_INPUT];
                        if(getcwd(current_buffer, MAX_INPUT) != NULL) {
                            strcpy(current_working_dir, current_buffer);
                            setenv("PWD", current_working_dir, 1);

                            /* Set return code to indicate success */
                            last_program_return_code = 0;
                        }
                        else {
                            fprintf(stderr, "Could not retrieve current working directory: %s\n", strerror(errno));

                            /* Set return code to indicate error occurred */
                            last_program_return_code = 1;
                        }
                    }
                    else {
                        fprintf(stderr, "Could not change directory for: \"%s\"\n",
                            child_program_args[0]);

                        /* Set return code to indicate error occurred */
                        last_program_return_code = 1;
                    }
                }
            }
            /* Check if the target directory is the last working directory */
            else if(strcmp(child_program_args[1], "-") == 0) {
                /* Retrieve the current working directory */
                if((temp = getcwd(temp_buffer, MAX_INPUT))  != NULL) {
                    /* Update the current working directory to the prior one when
                     * the directory change succeeds
                     */
                    if(chdir(last_working_dir) == 0) {
                        /* Print the previous working directory */
                        printf("%s\n", last_working_dir);

                        /* Update the last working directory */
                        strcpy(last_working_dir, temp);
                        setenv("OLDPWD", temp, 1);

                        /* Update the current working directory */
                        char current_buffer[MAX_INPUT];
                        if(getcwd(current_buffer, MAX_INPUT) != NULL ) {
                            strcpy(current_working_dir, current_buffer);
                            setenv("PWD", current_working_dir, 1);

                            /* Set return code to indicate success */
                            last_program_return_code = 0;
                        }
                        else {
                            fprintf(stderr, "Could not retrieve current working directory: %s\n", strerror(errno));

                            /* Set return code to indicate error occurred */
                            last_program_return_code = 1;
                        }
                    }
                    else {
                        printf("Could not navigate to previous working directory: %s\n", strerror(errno));

                        /* Set return code to indicate error occurred */
                        last_program_return_code = 1;
                    }
                }
                else {
                    fprintf(stderr, "Could not retrieve current working directory: %s\n", strerror(errno));

                    /* Set return code to indicate error occurred */
                    last_program_return_code = 1;
                }
            }
            /* Check if the target working directory is an absolute or relative
             * path
             */
            else {
                /* Retrieve the current working directory */
                if((temp = getcwd(temp_buffer, MAX_INPUT)) != NULL) {
                    /* Check if the target filepath exists */
                    if(filepath_exists(child_program_args[1])) {
                        /* Update the working directory states only when directory change
                         * succeeds
                         */
                        if(chdir(child_program_args[1]) == 0) {
                            /* Update the previous working directory */
                            strcpy(last_working_dir, temp);
                            setenv("OLDPWD", temp, 1);

                            /* Update the current working directory */
                            char current_buffer[MAX_INPUT];
                            if(getcwd(current_buffer, MAX_INPUT) != NULL ) {
                                strcpy(current_working_dir, current_buffer);
                                setenv("PWD", current_working_dir, 1);

                                /* Set return code to indicate success */
                                last_program_return_code = 0;
                            }
                            else {
                                fprintf(stderr, "Could not retrieve current working directory: %s\n", strerror(errno));

                                /* Set return code to indicate error occurred */
                                last_program_return_code = 1;
                            }
                        }
                        else {
                            fprintf(stderr, "Could not change directory for: \"%s\"\n", tok);

                            /* Set return code to indicate error occurred */
                            last_program_return_code = 1;
                        }
                    }
                    /* Indicate the target filepath does not exist */
                    else {
                        printf("Filepath \"%s\" does not exist.\n", tok);

                        /* Set return code to indicate error occurred */
                        last_program_return_code = 1;
                    }
                }
                else {
                    fprintf(stderr, "Could not retrieve current working directory: %s\n", strerror(errno));

                    /* Set return code to indicate error occurred */
                    last_program_return_code = 1;
                }
            }
        }
        /* Check if command to execute is 'set' */
        else if(strcmp(child_program_args[0], "set") == 0) {
            /* Retrieve contents of environmental variable to modify */
            char* envTest;
            char* envVar;
            envTest = getenv(child_program_args[0]);

            /* When the value of the environmental variable to modify is
             * successfully retrieved
             */
            if(envTest != NULL) {
                /* Store the name of environmental variable to modify */
                envVar = child_program_args[1];

                /* When the tokenized string is equal sign operator */
                if(strcmp(child_program_args[2], "=") == 0) {
                    /* When the value of the environmental variable does not
                     * update successfully
                     */
                    if(setenv(envVar, child_program_args[3], 1) != 0) {
                        /* Indicate that an error updating the value occurred */
                        fprintf(stderr, "%s\n", strerror(errno));

                        /* Set return code to indicate error occurred */
                        last_program_return_code = 1;

                        return;
                    }
                    /* Otherwise, complete the successful command execution */
                    else {
                        /* Set return code to indicate success */
                        last_program_return_code = 0;
                    }
                }
                /* Otherwise leave the environmental variable untouched */
                else {
                    /* Set return code to indicate error occurred */
                    last_program_return_code = 1;

                    return;
                }
            }
            /* Otherwise, attempt to define a new environmental variable */
            else {
                /* Attempt to define a new environmental variable when the
                 * given name is a valid environmental variable name
                 */
                if(valid_env_var_name(child_program_args[1], strlen(child_program_args[0]))) {
                    /* Store the name of environmental variable to create */
                    envVar = child_program_args[1];

                    /* When the tokenized string is equal sign operator */
                    if(strcmp(child_program_args[2], "=") == 0) {
                        /* When the value of the environmental variable does not
                         * update successfully
                         */
                        if(setenv(envVar, child_program_args[3], 1) != 0) {
                            /* Indicate that an error updating the value occurred */
                            fprintf(stderr, "%s\n", strerror(errno));

                            /* Set return code to indicate error occurred */
                            last_program_return_code = 1;

                            return;
                        }
                        /* Otherwise, complete the successful command execution */
                        else {
                            /* Set return code to indicate success */
                            last_program_return_code = 0;
                        }
                    }
                    /* Otherwise leave the environmental variable untouched */
                    else {
                        /* Set return code to indicate error occurred */
                        last_program_return_code = 1;

                        return;
                    }
                }
                /* Otherwise, indicate invalid environmental variable set */
                else {
                    fprintf(stderr, "\"%s\" is not a valid environmental variable name.\n",
                        child_program_args[1]);

                    /* Set return code to indicate error occurred */
                    last_program_return_code = 1;
                }
            }
        }
        /* Check if the special command to execute is 'help' */
        else if(strcmp(child_program_args[0], "help") == 0) {
            /* Print the shell usage */
            USAGE(argv[0]);

            /* Set return code to indicate success */
            last_program_return_code = 0;
        }
        /* Check if the program to execute is 'history' */
        else if(strcmp(child_program_args[0], "history") == 0) {
            /* Use for-loop to print the contents of the command history to the
             * console */
            for(int i = 0; i < cmd_history_size && cmd_history[i] != NULL; i++) {
                printf("%2d %s\n", (i + 1), cmd_history[i]);
            }

            /* Set return code to indicate success */
            last_program_return_code = 0;
        }
        /* Check if the profram to execute is 'clear-history' */
        else if(strcmp(child_program_args[0], "clear-history") == 0) {
            /* Use for-loop to free the memory allocated to store the command history */
            for(int i = cmd_history_size; i > 0 && cmd_history[i-1] != NULL; i--, cmd_history_size--) {
                free(cmd_history[i -1]);
            }

            /* Reopen and close the existing history file to clear its contents */
            FILE *history_file = fopen(HISTORY_FILE_NAME, "w");
            if(fclose(history_file) < 0) {
                fprintf(stderr, "Error in clearing command history: %s\n", strerror(errno));

                /* Set return code to indicate error */
                last_program_return_code = 1;
            }
            else {
                /* Set return code to indicate success */
                last_program_return_code = 0;
            }

            /* Finish the evaluation without saving the current command to
             * completely clear the command history */
            return;
        }
        /* Check if the program to execute is 'echo' */
        else if(strcmp(child_program_args[0], "echo") == 0) {
            /* When there is no string following the command 'echo' */
            if(child_program_args[1] == NULL) {
                /* Print new line */
                printf("\n");

                /* Set return code to indicate success */
                last_program_return_code = 0;
            }
            /* Otherwise, tokenize the argument input via a while-loop until none remains */
            else {
                int i = 1;
                while(i < num_child_program_args) {
                    if(child_program_args[i][0] == '$') {
                        /* Check if the variable indicated is the return code
                         * of the last running program */
                        if(strlen(child_program_args[i]) > 1 &&
                            strcmp((child_program_args[i] + 1), "?") == 0) {

                            /* Print the return code of the last running program */
                            printf("%d", last_program_return_code);
                            fflush(stdout);
                        }
                        /* Check if there is a possible variable name */
                        else if(strlen(child_program_args[i]) > 1) {
                            /* Retrieve the convents of the given environment variable */
                            char *env_print = getenv((child_program_args[i] + 1));

                            /* Ignore non-existent environmental */
                            if(env_print == NULL) {
                                i++;
                                continue;
                            }
                            else {
                                /* Print the convents of the given environment variable */
                                write(1, env_print, strlen(env_print));
                            }
                        }
                        /* Otherwise, just print '$' */
                        else {
                            write(1, "$", 1);
                        }
                    }
                    /* Otherwise, just print the contents of the tokenized string argument */
                    else {
                        write(1, child_program_args[i], strlen(child_program_args[i]));
                    }

                    /* Print space to seperate space seperated arguements to print */
                    write(1, " ", 1);

                    /* Increment index to access next argument */
                    i++;
                }
                /* Print ending newline */
                write(1, "\n", 1);

                /* Set return code to indicate success */
                last_program_return_code = 0;
            }
        }
        /* Check if we need to exit the shell */
        else if(strcmp(child_program_args[0], "exit") == 0) {
            /* Save the executed command into the history */
            save_current_cmd(cmdline);

            /* Close the command history */
            close_cmd_history(cmd_history, &cmd_history_size);

            /* Exit the shell */
            printf("Exiting...\n");
            _exit(0);
        }
        /* Otherwise, check if command is executable from the PATH */
        else {
            /* When the program exists in the PATH */
            if(strlen(child_program_path) != 0) {
                /* Store the full program path as the first argument */
                child_program_args[0] = child_program_path;

                /* Execute program in child process */
                if((pid = Fork()) == 0) {
                    /* Execute desired program with given arguments */
                    if(execv(*child_program_args, child_program_args) < 0) {
                        /* Print return code and ERRNO if child process fails */
                        fprintf(stderr, "%s\n", strerror(errno));

                        /* Terminate child process after indicating error occurred */
                        exit(EXIT_FAILURE);
                    }
                }
                /* Main shell process will wait until child process terminates */
                else {
                    /* Wait until the forked child process returns */
                    waitpid(pid, &child_status, 0);

                    /* Check if the child process exited normally */
                    if(WIFEXITED(child_status)) {
                        /* Set return code to code returned by the child */
                        last_program_return_code = WEXITSTATUS(child_status);
                    }
                    /* Otherwise, set return code to indicate child exited abnormally */
                    else{
                        /* Set return code to indicate error occurred */
                        last_program_return_code = EXIT_FAILURE;
                    }
                }
            }
            else {
                /* Otherwise, the command is not a valid executable from the path */
                printf("%s: command not found\n", child_program_args[0]);

                /* Set return code to indicate error occurred */
                /* Error is that command was not found */
                last_program_return_code = 127;
            }
        }

        /* Save the executed command into the history */
        save_current_cmd(cmdline);
    }
}

int main (int argc, char ** argv, char **envp) {

    int finished = 0;
    int opt = 0;
    time_t begin_timer=0;
    time_t end_timer=0;
    double difference=0;
    char *prompt = "320sh> ";
    char cmd[MAX_INPUT] = { '\0' };
    char key_buff[4] = { '\0' };

    /* Use while-loop to check for optional arguments as shell starts */
    while((opt = getopt(argc, argv, "dt")) != -1) {
        switch(opt) {
            case 'd':
                /* The help menu was selected */
                dFlag = 1;
                break;
            case 't':
                /* time option */
                tFlag=1;
                break;
            case '?':
                /* Indicate bad argument passed to shell on startup and fails
                 * gracefully
                 */
                fprintf(stderr, "Invalid argument: -%c\n", opt);
                exit(EXIT_FAILURE);
                break;
            default:
                break;
        }
    }

    /* Load command history from file */
    load_cmd_history(cmd_history, &cmd_history_size);
    history_counter=cmd_history_size-1;

    /* Use while-loop to execute the shell */
    while (!finished) {
        /* Declare constants needed for shell program execution */
        char *cursor;
        char last_char;
        int rv;
        int count; // Number of bytes read from STDIN for the command

        /* Load the current working directory into the array when shell starts */
        if(strlen(current_working_dir) == 0) {
            char current_buffer[MAX_INPUT];
            getcwd(current_buffer, MAX_INPUT);
            strcpy(current_working_dir, current_buffer);
        }

        /* Print the working directory before the shell prompt */
        write(1, "[\e[1m", 5);
        write(1, current_working_dir, strlen(current_working_dir));
        write(1, "\e[0m]", 5);
        write(1, " ", 1);

        // Print the prompt
        rv = write(1, prompt, strlen(prompt));

        /* Exit the shell if there was a write error or no bytes written to STDOUT */
        if (!rv) {
            finished = 1;
            break;
        }

        /* Use for-loop to retrieve and parse input */
        for(rv = 1, count = 0, cursor = cmd, last_char = 1; (count < (MAX_INPUT - 2) &&
            last_char != '\n'); ){
            /* Read first byte of key press from the keyboard stdin */
            rv = fgetc(stdin);
            /* Check if the first byte was the escape code for a multibyte
             * single key press */
            if(rv == ESCAPE_CODE) {
                /* Store escape code into multibyte key buffer */
                last_char = (char) rv;
                key_buff[0] = last_char;

                /* Counter to track size of multibyte code of key pressed */
                int i = 1;

                /* Use while-loop to retrieve the bytes of the multibyte keypress
                 * until the end of input is reached */
                while(!feof(stdin) && !isalpha(key_buff[i-1]) && i < 4){
                    /* Retrieve next byte of multibyte code */
                    rv = fgetc(stdin);
                    /* Store the read byte into the multibyte key buffer */
                    last_char = (char) rv;
                    key_buff[i++] = last_char;
                }

                /* Check if the multibyte code is for the UP_ARROW_KEY */
                if(i == 3 && (strncmp(key_buff, UP_ARROW_KEY, i) == 0)) {
                    if(cmd_history_size>0 && history_counter>=0)
                    {
                      if(history_counter==cmd_history_size)
                      {
                        history_counter--;
                      }
                      for(char *temp_cursor=cursor;temp_cursor>cmd;temp_cursor--){
                        /*move terminal cursor on the screen left by one*/
                        write(1, "\e[D", 3);
                      }
                          //clear terminal line
                          write(1, "\e[K", 3);
                          cursor="\0";
                          count=strlen(cmd);
                          //print new history command
                          write(1,cmd_history[history_counter],strlen(cmd_history[history_counter]));
                          strcpy(cmd,cmd_history[history_counter]);
                          count=strlen(cmd_history[history_counter]);
                          cursor=cmd+count;
                          history_counter--;
                    }
                    else{
                    //    memset(cmd, 0, strlen(cmd));
                      }
                    //last_char = '\n';
                }
                /* Check if the multibyte code is for the DOWN_ARROW_KEY */
                else if(i == 3 && (strncmp(key_buff, DOWN_ARROW_KEY, i) == 0)) {
                    if(cmd_history_size>0&&history_counter<cmd_history_size)
                    {
                      if(history_counter<0)
                      {
                        history_counter++;
                      }
                      for(char *temp_cursor=cursor;temp_cursor>cmd;temp_cursor--){
                        /*move terminal cursor on the screen left by one*/
                        write(1, "\e[D", 3);
                      }
                          //clear out terminal line
                          write(1, "\e[K", 3);
                          cursor="\0";
                          count=strlen(cmd);
                          //print new history command
                          write(1,cmd_history[history_counter],strlen(cmd_history[history_counter]));
                          strcpy(cmd,cmd_history[history_counter]);
                          count=strlen(cmd_history[history_counter]);
                          cursor=cmd+count;
                          history_counter++;
                    }
                    else
                    {
                        for(char *temp_pointer = cursor; temp_pointer > cmd; temp_pointer--) {
                            write(1, "\e[D", 3);
                        }
                        write(1, "\e[K", 3);
                        memset(cmd, 0, strlen(cmd));
                        count = 0;
                        cursor = cmd;
                    }
                }
                /* Check if the multibyte code is for the LEFT_ARROW_KEY */
                else if(i == 3 && (strncmp(key_buff, LEFT_ARROW_KEY, i) == 0)) {
                    /* When the cursor is not at the start of the command buffer */
                    if(cursor > cmd) {
                        /* Move the cursor left by one and clear output contents after
                         * that position */
                        write(1, "\e[D\e[K", 6);

                        /* Decrement cursor position on command buffer */
                        cursor--;

                        /* Calculate the difference between cursor position and end
                         * of command buffer */
                        int buff_index_diff = (cmd + count) - cursor;

                        /* When the cursor is not at the end of the command buffer */
                        if(buff_index_diff > 0) {
                            /* Write the contents of the command buffer from after the
                             * cursor position to the output */
                            write(1, cursor, buff_index_diff);
                            /* Move the prompt cursor back to the current cursor position
                             * on the command buffer */
                            for(int temp = 0; temp < buff_index_diff; temp++) {
                                write(1, "\e[D", 3);
                            }
                        }
                    }
                }
                /* Check if the multibyte code is for the RIGHT_ARROW_KEY */
                else if(i == 3 && (strncmp(key_buff, RIGHT_ARROW_KEY, i) == 0)) {
                    /* When the cursor is not at the end of the command buffer */
                    if(cursor < (cmd + count)) {
                        /* Move the cursor left by one and clear output contents after
                         * that position */
                        write(1, "\e[C\e[K", 6);

                        /* Increment cursor position on command buffer */
                        cursor++;

                        /* Calculate the difference between cursor position and end
                         * of command buffer */
                        int buff_index_diff = (cmd + count) - cursor;

                        /* When the cursor is not at the end of the command buffer */
                        if(buff_index_diff > 0) {
                            /* Write the contents of the command buffer from after the
                             * cursor position to the output */
                            write(1, cursor, buff_index_diff);
                            /* Move the prompt cursor back to the current cursor position
                             * on the command buffer */
                            for(int temp = 0; temp < buff_index_diff; temp++) {
                                write(1, "\e[D", 3);
                            }
                        }
                    }
                }
                /* Check if the multibyte code is for alt-F */
                else if(i == 2 && (strncmp(key_buff, ALT_F_CODE, i) == 0)) {
                    /* Search for the next instance of the space character along
                     * the command buffer */
                    char *temp_pointer = memchr(cursor, ' ', strlen(cursor));

                    /* When returned value is non-NULL and the character in the
                     * next character in the command buffer is non-NULL */
                    if(temp_pointer != NULL) {
                        /* Check for the next instance of space after the current position on the command buffer */
                        char *temp_pointer_check = memchr((cursor + 1), ' ', strlen(cursor + 1));

                        /* When the cursor is currently at a space character and
                         * there is another space character after the current word
                         * in the command buffer  */
                        if(*cursor == ' ' && temp_pointer_check != NULL) {
                            /* Update the position to move the cursor */
                            temp_pointer = temp_pointer_check;

                            /* Calculate the difference between the current position
                             * along the command buffer  */
                            int cursor_diff = temp_pointer - cursor;

                            /* Use while-loop to move terminal cursor to the next
                             *  instance of space */
                            while(cursor_diff > 0) {
                                write(1, "\e[C", 3);
                                cursor_diff--;
                            }

                            /* Move the cursor to the position of the next instance
                             * of space */
                            cursor = temp_pointer;
                        }
                        /* When the current character is a space and there are
                         * no further instances of space along the command buffer */
                        else if(*cursor == ' ' && temp_pointer_check == NULL) {
                            /* Calculate the difference between cursor position and end
                             * of command buffer */
                            int cursor_diff = (cmd + count) - cursor;

                            /* Use while-loop to move terminal cursor to the end of
                             * the command  buffer */
                            while(cursor_diff > 0) {
                                write(1, "\e[C", 3);
                                cursor_diff--;
                            }

                            /* Move the cursor the end of the command buffer */
                            cursor = cmd + count;
                        }
                        /* Otherwise */
                        else {
                            /* Calculate the difference between the current position
                             * along the command buffer  */
                            int cursor_diff = temp_pointer - cursor;

                            /* Use while-loop to move terminal cursor to the next
                             *  instance of space */
                            while(cursor_diff > 0) {
                                write(1, "\e[C", 3);
                                cursor_diff--;
                            }

                            /* Move the cursor to the position of the next instance
                             * of space */
                            cursor = temp_pointer;
                        }
                    }
                    /* Otherwise, move the cursor to the end of the command buffer */
                    else {
                        /* Calculate the difference between cursor position and end
                         * of command buffer */
                        int cursor_diff = (cmd + count) - cursor;

                        /* Use while-loop to move terminal cursor to the end of
                         * the command  buffer */
                        while(cursor_diff > 0) {
                            write(1, "\e[C", 3);
                            cursor_diff--;
                        }

                        /* Move the cursor the end of the command buffer */
                        cursor = cmd + count;
                    }
                }
                else if(i == 2 && (strncmp(key_buff, ALT_B_CODE, i) == 0)) {
                    char *temp_pointer = cursor;
                    if((temp_pointer>cmd)&&(!isspace(*temp_pointer)) && (isspace(*(temp_pointer-1))))
                    {
                      temp_pointer--;
                    }
                    //spaces
                    while(*temp_pointer==' ' && (cmd<temp_pointer)){
                      if(*(temp_pointer-1)!=' '){
                          temp_pointer--;
                          break;
                      }
                      temp_pointer--;
                    }
                    //letters
                    while(*temp_pointer!=' ' && (cmd<temp_pointer))
                    {
                      if(*(temp_pointer-1)==' '){
                          break;
                      }
                      temp_pointer--;
                    }
                    int cursor_diff=cursor-temp_pointer;
                    /* Use while-loop to move terminal cursor to the end of
                     * the command  buffer */
                    while(cursor_diff > 0) {
                        write(1, "\e[D", 3);
                        cursor_diff--;
                    }
                    cursor=temp_pointer;
                }
                /* Otherwise */
                else {
                    // Do nothing, ignore key pressed
                    write(1, "\n", 1);
                }
            }
            /* Check for failure to read from stdin */
            /* Exit the shell if there was a read error or no bytes read from STDIN */
            else if(rv == EOF) {
                fprintf(stderr, "Error in attempt to retrieve input from keyboard. Exiting...\n");
                exit(EXIT_FAILURE);
            }
            /* Otherwise, input was single byte key press */
            else{
                /* Store read byte as a character */
                last_char = (char) rv;

                /* Check for the Start of Heading character -> ctrl-A */
                if(last_char == 1) {
                    /* Calculate the address difference from the start of the
                     * command buffer */
                    int tempInt = cursor - cmd;

                    /* When the cursor is not at the start of the command buffer */
                    if(cursor > cmd) {
                        /* Use while-loop to move the terminal cursor to the start
                         * of the command buffer on the terminal screen */
                        while(tempInt > 0) {
                            /* Move the on-screen terminal cursor left by one */
                            write(1,"\e[D",4);
                            tempInt--;
                        }

                        /* Set the cursor on the buffer to the start of the command
                         * buffer */
                        cursor = cmd;
                    }
                }
                /* Check for the End Of Text character -> ctrl-C */
                else if(last_char == 3) {
                    /* Indcate that ctrl-C was pressed */
                    write(1, "^c\n", 3);

                    /* Abort execution of the shell */
                    exit(0);
                }
                /* Check for the Vertical Tab character -> ctrl-K */
                else if(last_char == 11) {
                    /* Clear the contents of the terminal screen line after the
                     * current position of the terminal cursor */
                    write(1, "\e[K", 3);

                    /* Set the character at the current position on the command
                     * buffer to the null-character '\0' */
                    *cursor = '\0';

                    /* Update the count of characters stored in the the command
                     *  buffer */
                    count = strlen(cmd);
                }
                /* check for the New Page character -> ctrl-L */
                else if(last_char == 12) {
                    /* Check if program exists in the PATH */
                    int child_status;
                    char clear_prog_path[512] = { '\0' };
                    char *clear_prog_args[2] = { "\0" };
                    char *env = getenv("PATH"); // Retrieve PATH environmental variable
                    pid_t pid; // PID of the child process running the bnary 'clear'
                    load_program_path("clear", env, clear_prog_path);

                    /* When the program exists in the PATH */
                    if(strlen(clear_prog_path) != 0) {
                        /* Add the file path of 'clear' to the arguments */
                        clear_prog_args[0] = clear_prog_path;

                        /* Execute program in child process */
                        if((pid = Fork()) == 0) {
                            /* Execute desired program with given arguments */
                            if(execv(*clear_prog_args, clear_prog_args) < 0) {
                                /* Print return code and ERRNO if child process fails */
                                fprintf(stderr, "%s\n", strerror(errno));

                                /* Terminate child process after indicating error occurred */
                                exit(EXIT_FAILURE);
                            }
                        }
                        /* Main shell process will wait until child process terminates */
                        else {
                            /* Wait until the forked child process returns */
                            waitpid(pid, &child_status, 0);

                            /* Check if the child process exited normally */
                            if(WIFEXITED(child_status)) {
                                /* Set return code to code returned by the child */
                                last_program_return_code = WEXITSTATUS(child_status);
                            }
                            /* Otherwise, set return code to indicate child exited abnormally */
                            else{
                                /* Set return code to indicate error occurred */
                                last_program_return_code = EXIT_FAILURE;
                            }
                        }
                        /* Print the working directory before the shell prompt */
                        write(1, "[\e[1m", 5);
                        write(1, current_working_dir, strlen(current_working_dir));
                        write(1, "\e[0m]", 5);
                        write(1, " ", 1);

                        // Print the prompt
                        rv = write(1, prompt, strlen(prompt));
                        write(1, cmd, strlen(cmd));

                        /* Calculate the difference between cursor position and end
                         * of command buffer */
                        int buff_index_diff = (cmd + count) - cursor;

                        /* When the cursor is not at the end of the command buffer */
                        if(buff_index_diff > 0) {
                            /* Move the prompt cursor back to the current cursor position
                             * on the command buffer */
                            for(int temp = 0; temp < buff_index_diff; temp++) {
                                write(1, "\e[D", 3);
                            }
                        }
                    }
                }
                /* Check for Negative ACK character -> ctrl-U */
                else if(last_char == 21) {
                    /* Calculate the address difference from the start of the
                     * command buffer */
                    int tempInt = cursor - cmd;

                    /* When the cursor is not at the start of the command buffer */
                    if(cursor > cmd) {
                        /* Copy the contents of the buffer located from the current
                         * position to the end into a temporary array */
                        char temp_buffer[MAX_INPUT] = { '\0' };
                        strcpy(temp_buffer, cursor);

                        /* Use while-loop to move the terminal cursor to the start
                         * of the command buffer on the terminal screen */
                        while(tempInt > 0) {
                            /* Move the on-screen terminal cursor left by one */
                            write(1,"\e[D",4);
                            tempInt--;
                        }

                        /* Clear the contents of the terminal line */
                        write(1, "\e[K", 3);

                        /* Move the contents of the remaining characters back to
                         * the command buffer */
                        strcpy(cmd, temp_buffer);
                        cmd[strlen(temp_buffer)] = '\0';

                        /* Set the cursor on the buffer to the start of the command
                         * buffer */
                        cursor = cmd;

                        /* Update count of characters in the command buffer */
                        count = strlen(cmd);

                        /* Print the contents of the buffer to the teminal screen */
                        write(1, cmd, strlen(cmd));

                        /* Move the terminal cursor on the screen back to the
                         * beginning of the command buffer */
                        for(int i = strlen(cmd); i > 0; i--) {
                            /* Move the on-screen terminal cursor left by one */
                            write(1,"\e[D",4);
                        }
                    }
                }
                /* Check for the DEL character */
                else if(last_char == BACKSPACE_KEY) {
                    /* When the command buffer currently contains characters and
                     * the cursor is not at the begining of the command buffer */
                    if(count > 0 && cursor > cmd) {
                        /* Calculate difference between cursor position and end of
                         * command buffer */
                        int buff_index_diff = (cmd + count) - cursor;

                        /* When the cursor of the prompt is not at the end of the
                         * command buffer */
                        if(buff_index_diff > 0) {
                            /* Calculate index of previous character in command buffer */
                            int temp = cursor - cmd - 1;

                            /* Shift the all characters currently in the buffer one
                             * index backward */
                            while(temp < count) {
                                cmd[temp] = cmd[temp+1];
                                temp++;
                            }

                            /* Move cursor back to start of prompt */
                            for(char *temp_pointer = cursor; temp_pointer > cmd; temp_pointer--) {
                                write(1, "\e[D", 3);
                            }

                            /* Print the current command buffer contents to the prompt */
                            write(1, "\e[K", 3);
                            write(1, cmd, count-1);

                            /* Move the cursor back to the current character in the
                             * command buffer */
                            for(temp = 0; temp < buff_index_diff; temp++) {
                                write(1, "\e[D", 3);
                            }
                        }
                        else {
                            /* Delete the last character stored in the command buffer */
                            *(cursor-1) = '\0';

                            /* Move cursor back by one character and clear the line after
                             * the current cursor position */
                            write(1, "\e[D\e[K", 6);
                        }

                        /* Decrement count of command buffer and cursor on command
                         * buffer accordingly */
                        count--;
                        cursor--;
                    }
                }
                /* Check for NEWLINE character */
                else if(last_char == '\n') {
                    /* Store the character at the end of the command buffer */
                    cmd[count] = last_char;
                    count++;

                    /* Write character to stdout */
                    write(1, &last_char, 1);
                }
                /* Otherwise, store the character in the command buffer */
                else {
                    /* Calculate difference between cursor position and end of
                     * command buffer */
                    int buff_index_diff = (cmd + count) - cursor;

                    /* When the cursor of the prompt is not at the end of the
                     * command buffer */
                    if(buff_index_diff > 0) {
                        /* Calculate index of next character in command buffer */
                        int temp = count + 1;

                        /* Shift the all characters currently in the buffer one
                         * index forward */
                        while((cmd + temp) > cursor) {
                            cmd[temp] = cmd[temp-1];
                            temp--;
                        }

                        /* Store the entered character in the current position in
                         * the command buffer */
                        *cursor = last_char;

                        /* Print the current command buffer contents to the prompt */
                        write(1, "\e[K", 3);
                        write(1, cursor, buff_index_diff+1);

                        /* Move the cursor back to the current character in the
                         * command buffer */
                        for(temp = 0; temp < buff_index_diff; temp++) {
                            write(1, "\e[D", 3);
                        }
                    }
                    /* Otherwise, place character in the end of the character buffer */
                    else {
                        /* Store the character in the character buffer */
                        *cursor = last_char;

                        /* Write character to stdout */
                        write(1, &last_char, 1);
                    }

                    /* Increment the buffer character count and cursor position */
                    cursor++;
                    count++;
                }
            }
        }

        /* Set the trailing newline character to NULL character '\0' */
        *(cmd+count-1) = '\0';

        /* Print debug information as needed */
        if(dFlag == 1) {
            fprintf(stderr, "RUNNING: %s\n", cmd);
        }

        //time before
        if(tFlag==1)
        {
          time(&begin_timer);
        }

        /* Evaluate the entered command line */
        eval_cmd(cmd, envp);

        //time after
        if(tFlag==1)
        {
          time(&end_timer);
          difference=difftime(end_timer,begin_timer);
          //print time
          printf("%.2f secs\n",difference);
        }

        /* Print debug information as needed */
        if(dFlag == 1) {
          fprintf(stderr, "ENDED: %s (ret=%d)\n", cmd, last_program_return_code);
        }

        /* Zero out entered input to shell prior to next prompt of user */
        memset(cmd, 0, strlen(cmd));
    }

    return 0;
}

pid_t Fork(void) {
    pid_t pid;

    /* Create child process */
    if((pid = fork()) < 0) {
        /* Print error in forking when pid is negative */
        fprintf(stderr, "Fork error %d \n", errno);
        exit(0);
    }

    return pid;
}

void load_program_path(char *bin, char *sysenv, char *program_file_path) {
    /* Declare constants for checking if the file exists */
    char env[strlen(sysenv)];
    strcpy(env, sysenv);

    char *envtok;
    char filepath[512] = {'\0'};

    /* Begin to tokenize the PATH environment string */
    envtok = strtok(env, ":");

    /* Use while-loop to check if given program exists in the PATH folders */
    while(envtok != NULL) {
        /* Build filepath from tokenized folder path from PATH */
        strcat(filepath, envtok);
        strcat(filepath, "/");
        strcat(filepath, bin);
        strcat(filepath, "\0");

        /* Check if the program exists */
        if(filepath_exists(filepath)) {
            /* Specify that the program exists */
            strcpy(program_file_path, filepath);
            return;
            break;
        }

        /* If not, tokenize the next folder in the PATH */
        envtok = strtok(NULL, ":");
        memset(filepath, 0, strlen(filepath));
    }
    /* Specify program doesn't exist with empty string file path */
    memset(program_file_path, 0, strlen(program_file_path));
    return;
}

bool filepath_exists(char* filepath) {
    /* Create stat instance to check if given program file path exists */
    struct stat filedata;

    /* Zero out memory data */
    memset(&filedata, 0, sizeof(filedata));

    /* Check if the given program file path exists */
    if(stat(filepath, &filedata) == 0) {
        /* Program file path exists */
        return true;
    }

    /* Otherwise, program file path does not exist */
    return false;
}

bool valid_env_var_name(char *env_var_name, size_t length) {
    /* Assume that variable name is valid */
    bool result = true;

    /* Specify invalid environmental variable name when the length of the name
     * is zero */
    if(length == 0) {
        result = false;
    }
    /* Otherwise, check each character of the given string for invalid characters */
    else {
        /* Check for invalid characters in the string using for-loop */
        for(size_t i = 0; i < length; i++) {
            /* Point to current character in the name string to check */
            char temp = env_var_name[i];

            /* Specify invalid environmental variable name when it starts with a digit */
            if(i == 0 && (temp >= 48 && temp <= 57)) {
                result = false;
                break;
            }

            /* Specify invalid environmental varible name when the given string
             * contains a character that is not an ASCII letter, ASCII digit, or the
             * underscore symbol
             */
            if(!((temp >= 48 && temp <= 57) || (temp >= 65 && temp <= 90)
                    || (temp >= 97 && temp <= 122) || (temp == 95))) {
                result = false;
                break;
            }
        }
    }

    return result;
}

void save_current_cmd(char *cmd) {
    /* Create a pointer to navigate the command buffer */
    char *cursor = cmd;

    /* Do not save the command array when the command is invalid */
    if(*cursor == '\0' || *cursor == ' ') {
        return;
    }

    /* Otherwise, begin to save the command into the history array */
    char *cmd_temp;
    /* Save the command at the end of the history array, while removing the oldest
     * waved command, when it is full */
    if(cmd_history_size == 50) {
        /* Get the reference to the oldest saved command */
        cmd_temp = cmd_history[0];

        /* Use for-loop to move the references of the saved commands back by one
         * index */
        for(int i = 0; i < 49; i++) {
            cmd_history[i] = cmd_history[i+1];
        }

        /* Allocate heap memory to store the current command to save */
        char *cmd_new = calloc(MAX_INPUT, 1);

        /* Copy the contents of the current commmand into the allocated memory */
        strcpy(cmd_new, cmd);

        /* Store the copied command into the command history */
        cmd_history[49] = cmd_new;
        /* free */
        free(cmd_temp);
    }
    /* Otherwise, save the command at the end of the history without removing any
     * saved commands */
    else {
        /* Allocate heap memory to store the current command to save */
        cmd_temp = calloc(MAX_INPUT, 1);

        /* Copy the contents of the current command into the allocated memory */
        strcpy(cmd_temp, cmd);

        /* Store the copied command into the command history */
        cmd_history[cmd_history_size] = cmd_temp;

        /* Increment command history size */
        cmd_history_size++;
    }
    history_counter=cmd_history_size-1;
    up_arrow_counter=0;
}

void load_cmd_history() {
    /* Create file pointer to store the reference for the file stream for history file */
    FILE *history_file;

    /* Load the contents of the history file into the command history when the
     * file can be opened */
    if((history_file = fopen(HISTORY_FILE_NAME, "r")) != NULL) {
        //fprintf(stderr, "HISTORY_FILE loaded successfully.\n");
        /* Use while-loop to load the contents of the history file into the
         * command history */
        char *temp = calloc(MAX_INPUT, 1);
        while(fgets(temp, MAX_INPUT, history_file) != NULL) {
            /* Allocate memory to store the loaded line from history file */
            char *history_line = calloc(MAX_INPUT, 1);

            /* Copy loaded history line into the allocated memory */
            strcpy(history_line, temp);

            /* Remove the trailing newline from the loaded line */
            char *cursor = history_line + strlen(history_line) - 1;
            *cursor = '\0';

            /* Store the loaded line into the command history */
            cmd_history[cmd_history_size] = history_line;
            (cmd_history_size)++;

            /* Clear out the memory used to load the history file */
            memset(temp, 0, MAX_INPUT);
        }
        /* Free the allocated memory used for loading the history file */
        free(temp);

        /* Close the file stream associated with the history file */
        fclose(history_file);
    }
    /* History file did not exist, so create the file to save the command
     * history at the end of the program */
    else{
        //fprintf(stderr, "HISTORY_FILE load failed.\n");

        /* Create the history file to store the contents of the command history */
        int history_fd = open(HISTORY_FILE_NAME, O_RDONLY | O_CREAT);

        /* Empty history file is created, so close it. */
        if(close(history_fd) < 0) {
            //fprintf(stderr, "HISTORY_FILE created, but not closed.\n");
        }
        else {
            //fprintf(stderr, "HISTORY_FILE created and closed.\n");
        }
    }
}

void close_cmd_history() {
    /* Create file descriptor to store the reference for the for history file */
    FILE *history_file = fopen(HISTORY_FILE_NAME, "w");

    /* Use for-loop to iterate through the command history */
    for(int i = 0; i < cmd_history_size && cmd_history[i] != NULL; i++) {
        /* Retrieve the reference of the current command to save from history */
        char *cursor = cmd_history[i];

        /* Write the contents of the current command line to the history file */
        fputs(cursor, history_file);

        /* Terminate command line written to the history file with a newline */
        fputs("\n", history_file);

        /* Free the allocated memory storing the saved command line */
        free(cursor);
    }

    /* Close the history file */
    if(fclose(history_file) < 0) {
        //fprintf(stderr, "File is not closing!!!\n");
    }
}
