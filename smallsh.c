/* Libraries */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <math.h>
#include <signal.h>
#include <limits.h>
#include <sys/wait.h>

/* Constants */
#define MAX_COMMAND_LENGTH 2048
#define MAX_ARGS 512
#define SUCCESS 0
#define FAILURE 1

/* Structs */
/* Struct: command
 * -----------------------------------------------------------------------------
 * Struct that represent user's command.
 *   arguments - points an array of command line arguments, 
 *               except for input/output redirection and background process.
 *   input_file - filename of input file for input redirection
 *   output_file - filename of output file for output redirection
 *   background - if the process should be in the background
 *                  WNOHANG for background process
 *                  0 for foreground process
 */
struct command {
  char *arguments[MAX_ARGS];
  char *input_file;
  char *output_file;
  int background;
};

/* Function Prototypes */
int get_command(struct command *user_command);
void reset_command(struct command *user_command, bool reset_command);
char *expand_variable(char *unexpanded_string);
void execute_command(struct command *user_command, int *status, bool *exit_program);
void report_status(int *status);
void change_directory(struct command *user_command);
void exit_and_cleanup(struct command *user_command, bool *exit_program);
int fork_and_execute(struct command *user_command, int *status);


/* Main */
int main(void) {

  bool exit_program = false;
  int status = SUCCESS;
  struct command user_command;
  reset_command(&user_command, true);
  
  while (!exit_program) {

    if (get_command(&user_command) == SUCCESS)
      execute_command(&user_command, &status, &exit_program);

  }
  return 0;
}

/*
 * Function: get_command
 * -----------------------------------------------------------------------------
 * Prompt user for a command input, parse it, and 
 *   store the information in the given pointer to user_command.
 * Returns SUCCESS (0) if the command has be parsed and saved in user_command
 * Returns FAILURE (1) if the user input is blank or a command.
 */
int get_command(struct command *user_command) {

  reset_command(user_command, false);

  // Prompt for command
  printf(": ");
  fflush(stdout);

  char *input_buffer = (char *)calloc(MAX_COMMAND_LENGTH, sizeof(char));
  size_t input_size = MAX_COMMAND_LENGTH;
  getline(&input_buffer, &input_size, stdin);

  // Handle comment or blank input
  bool is_comment = strncmp(input_buffer, "#", 1) == 0;
  bool is_blank_line = strncmp(input_buffer, "\n", 1) == 0;
  if (is_comment || is_blank_line) {
    free(input_buffer);
    return FAILURE;
  }

  // Parse user input and store information
  int arg_index = 0;
  char *token;
  char *save_ptr = input_buffer;
  token = strtok_r(input_buffer, " \n", &save_ptr);

  while (token != NULL) {

    // Input Redirection
    if (strcmp(token, "<") == 0) {
      token = strtok_r(NULL, " \n", &save_ptr);
      user_command->input_file = expand_variable(token);

    // Output Redirection
    } else if (strcmp(token, ">") == 0) {
      token = strtok_r(NULL, " \n", &save_ptr);
      user_command->output_file = expand_variable(token);

    // Run in the background
    } else if (strcmp(token, "&") == 0) {
      user_command->background = WNOHANG;

    // Command arguments
    } else {

      user_command->arguments[arg_index] = expand_variable(token);
      arg_index++;
    }
    token = strtok_r(NULL, " \n", &save_ptr);
  }
  
  free(input_buffer);
  return SUCCESS;
}

/*
 * Function: reset_command
 * -----------------------------------------------------------------------------
 * Get a pointer to user_command and initialize (boolean) as parameters,
 *   free all memory in user_command if they are allocated, 
 *   set all string pointers in arguments, input_file, and output_file to NULL, 
 *   and background to 0.
 */
void reset_command(struct command *user_command, bool initialize) {

  if (initialize) {
    for (int i = 0; i < MAX_ARGS; i++) {
      user_command->arguments[i] = NULL;
    }
  }
  
  if (!initialize) {
    for (int i = 0; i < MAX_ARGS; i++) {
      if (!user_command->arguments[i])
        break;
      free(user_command->arguments[i]);
      user_command->arguments[i] = NULL;
    }
  }
    
  if (!user_command->input_file)
    free(user_command->input_file);
  user_command->input_file = NULL;
    
  if (!user_command->output_file)
    free(user_command->output_file);
  user_command->output_file = NULL;

  user_command->background = 0;
}

/*
 * Function: expand_variable
 * -----------------------------------------------------------------------------
 * Get a pointer to a string as parameter,
 *   replace all instances of "$$" with the process ID of the program.
 * Reallocate enough memory for the new string as necessary and
 *   return the pointer to the expanded string.
 */
char *expand_variable(char *unexpanded_string) {

  int num_digits_pid = floor(log10(getpid())) + 1;
  int variable_start;
  char *expanded_string = NULL;
  char *current_string = calloc(strlen(unexpanded_string), sizeof(char));
  strcpy(current_string, unexpanded_string);
  variable_start = strstr(current_string, "$$") - current_string;

  while (variable_start >= 0 && variable_start < strlen(current_string)) {

    // Substring on the left of $$
    char expanded_string_left[variable_start+1];
    strncpy(expanded_string_left, "", sizeof(expanded_string_left));
    strncpy(expanded_string_left, current_string, variable_start);

    // Substring on the right of $$
    char expanded_string_right[strlen(current_string) - variable_start - 1];
    strncpy(expanded_string_right, "", sizeof(expanded_string_right));
    strncpy(expanded_string_right, current_string + variable_start + 2, strlen(current_string) - variable_start - 2);

    // Create expanded string
    int length_expanded_string = strlen(current_string) + num_digits_pid - 1;
    if (expanded_string) {
      free(expanded_string);
    }
    expanded_string = calloc(length_expanded_string, sizeof(char));
    sprintf(expanded_string, "%s%d%s", expanded_string_left, getpid(), expanded_string_right);

    current_string = expanded_string;
    variable_start = strstr(current_string, "$$") - current_string;
  };
  
  return current_string;
}

/*
 * Function: execute_command
 * -----------------------------------------------------------------------------
 * Take a pointer to user_command, status variable, and 
 *   the pointer to the exit_program (boolean) variable as parameters.
 * Execute status, cd, and exit commands in the foreground and
 *   create a new process and execute for other commands.
 */
void execute_command(struct command *user_command, int *status, bool *exit_program) {

  char *first_argument = user_command->arguments[0];
  
  if (strcmp(first_argument, "status") == 0) {
    report_status(status);

  } else if (strncmp(first_argument, "cd", 2) == 0) {
    change_directory(user_command);

  } else if (strcmp(first_argument, "exit") == 0) {
    exit_and_cleanup(user_command, exit_program);

  } else {
    fork_and_execute(user_command, status);
  }
}

/*
 * Function: change_directory
 * -----------------------------------------------------------------------------
 * Take a pointer to the user_command and change the current working directory.
 * If there is no argument after cd, 
 *   it will change the current working directory to the home directory.
 * Otherwise, it will change it to the first argument after cd.
 */
void change_directory(struct command *user_command) {

  char *path = user_command->arguments[1];

  if (!path) {
    path = getenv("HOME");
  }

  chdir(path);
}

/*
 * Function: report_status
 * -----------------------------------------------------------------------------
 * Take a pointer to and report the exit status or terminating signal of 
 *   the last foregound process.
 */
void report_status(int *status) {

  printf("exit value %d\n", *status);
  fflush(stdout);
}

/*
 * Function: exit_and_cleanup
 * -----------------------------------------------------------------------------
 * Take a pointer to user_command as parameter, 
 *   free all allocated memory and kill all child processes.
 * Also take a pointer to exit_program flag as parameter and change it to true.
 */
void exit_and_cleanup(struct command *user_command, bool *exit_program) {

  reset_command(user_command, false);
  *exit_program = true;
}

/*
 * Function: fork_and_execute
 * -----------------------------------------------------------------------------
 * Take a pointer to user_command and status as parameters, 
 *   create a child process to execute the arguments
 *   in the background or the foreground based on user input.
 * Also update the last foreground process exit status appropriately.
 */
int fork_and_execute(struct command *user_command, int *status) {

  int child_exit_method;
  pid_t spwan_pid = fork();

  switch(spwan_pid) {
    case -1:
      perror("fork() failed\n");
      exit(FAILURE);

    case 0:

      if (execvp(user_command->arguments[0], user_command->arguments) < 0) {
        exit(FAILURE);
      }
		  
      exit(SUCCESS);

    default:
      waitpid(spwan_pid, &child_exit_method, user_command->background);

      if (WIFEXITED(child_exit_method))
        *status = WEXITSTATUS(child_exit_method);

  }
  return SUCCESS;
}
