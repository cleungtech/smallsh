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

/* Constants */
#define MAX_COMMAND_LENGTH 2048
#define MAX_ARGS 512
#define SUCCESS 0
#define FAILURE -1

/* Structs */
/* Struct: command
 * -----------------------------------------------------------------------------
 * Struct that represent user's command.
 *   arguments - points the command line arguments, 
 *               except for input/output redirection and background process.
 *   input_file - filename of input file for input redirection
 *   output_file - filename of output file for output redirection
 *   is_background - True if the process should be ran in the background. False otherwise.
 */
struct command {
  char *arguments;
  char *input_file;
  char *output_file;
  bool is_background;
};

/* Function Prototypes */
int get_command(struct command *user_command);
void reset_command(struct command *user_command);
char *expand_variable(char *unexpanded_string);
void execute_command(struct command *user_command, int *status, bool *exit_program);

/* Main */
int main(void) {

  bool exit_program = false;
  int status = 0;
  struct command user_command;
  reset_command(&user_command);
  
  while (!exit_program) {

    if (get_command(&user_command) == SUCCESS)
      execute_command(&user_command, &status, &exit_program);

    printf("arguments: %s\ninput: %s\noutput: %s\nbg: %i\n", 
           user_command.arguments, user_command.input_file, user_command.output_file, user_command.is_background);

    reset_command(&user_command);
  }
  printf("arguments: %s\ninput: %s\noutput: %s\nbg: %i\n", 
         user_command.arguments, user_command.input_file, user_command.output_file, user_command.is_background);
  return 0;
}

/*
 * Function: get_command
 * -----------------------------------------------------------------------------
 * Prompt user for a command input, parse it, and 
 *   store the information in the given pointer to user_command.
 * Returns SUCCESS (0) if the command has be parsed and saved in user_command
 * Returns FAILURE (-1) if the user input is blank or a command.
 */
int get_command(struct command *user_command) {

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
  bool first_token = true;
  char *token;
  char *expanded_token;
  char *save_ptr = input_buffer;
  token = strtok_r(input_buffer, " \n", &save_ptr);
  user_command->arguments = (char *)calloc(MAX_COMMAND_LENGTH, sizeof(char));

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
      user_command->is_background = true;

    // Command arguments
    } else {

      expanded_token = expand_variable(token);

      // Add space in between arguments except for the first one
      if (first_token) {
        strcpy(user_command->arguments, expanded_token);
        first_token = false;
      } else {
        sprintf(user_command->arguments, "%s %s", user_command->arguments, expanded_token);
      }

      free(expanded_token);
    }

    token = strtok_r(NULL, " \n", &save_ptr);
  }
  free(input_buffer);

  return SUCCESS;
}

/*
 * Function: reset_command
 * -----------------------------------------------------------------------------
 * Get a pointer to a struct command (user_command) as parameter,
 *   free all memory if they are allocated, 
 *   set arguments, input_file, and output_file to NULL, 
 *   and is_background to false.
 */
void reset_command(struct command *user_command) {

  if (!user_command->arguments)
    free(user_command->arguments);
  user_command->arguments = NULL;
    
  if (!user_command->input_file)
    free(user_command->input_file);
  user_command->input_file = NULL;
    
  if (!user_command->output_file)
    free(user_command->output_file);
  user_command->output_file = NULL;

  user_command->is_background = false;
}

/*
 * Function: expand_variable
 * -----------------------------------------------------------------------------
 * Get a pointer to a string as parameter,
 *   replace all instances of "$$" with the process ID of the program.
 * Reallocate enough memory for the new string as necessary.
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
 * Take a pointer to user_command and status variable and 
 *   execute status, cd, and exit commands in the foreground.
 *   Create a new process and execute for other commands.
 */
void execute_command(struct command *user_command, int *status, bool *exit_program) {
  
  if (strcmp(user_command->arguments, "status") == 0) {
    printf("exit value %d\n", *status);
    fflush(stdout);

  } else if (strcmp(user_command->arguments, "cd") == 0) {
    printf("executing cd...\n");

  } else if (strcmp(user_command->arguments, "exit") == 0) {
    *exit_program = true;

  } else {
    printf("creating new process for execution ...\n");
  }
}
