/* Libraries */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

/* Constants */
#define MAX_COMMAND_LENGTH 2048
#define MAX_ARGS 512
#define SUCCESS 0
#define FAILURE -1

/* Structs */
/* Struct: command
 * -----------------------------------------------------------------------------
 * Struct that represent user's command
 *   argc - number of command line arguments (stored in argv)
 *   argv - points to the array of command line arguments 
 *   input_file - filename of input file for input redirection
 *   output_file - filename of output file for output redirection
 *   is_background - True if the process should be ran in the background. False otherwise.
 */
struct command {
  unsigned int argc;
  char *argv[MAX_ARGS];
  char *input_file;
  char *output_file;
  bool is_background;
};

/* Function Prototypes */
int get_command(struct command *user_command);

/* Main */
int main(void) {

  bool exit_program = false;
  struct command user_command;
  
  while (!exit_program) {

    if (get_command(&user_command) == SUCCESS) {
      for (int i = 0; i < user_command.argc; i++) {
        printf("user_command.command_argv[%d]: %s\n", i, user_command.argv[i]);
      }
      printf("user_command.command_argv: %d\n", user_command.argc);
      printf("user_command.output_file: %s\n", user_command.output_file);
      printf("user_command.input_file: %s\n", user_command.input_file);
      printf("user_command.is_background: %d\n", user_command.is_background);
    };    
  }
  
  return 0;
}

/*
 * Function: get_command
 * -----------------------------------------------------------------------------
 * Prompt user for a command input, parse it, and 
 * Store the information in the given pointer to a struct command (user_command)
 * Returns SUCCESS (0) if the command has be parsed and saved in user_command
 * Returns FAILURE (-1) if the user input is blank or a command.
 */
int get_command(struct command *user_command) {

  // Prompt for command
  printf(": ");
  char *input_buffer = (char *)calloc(MAX_COMMAND_LENGTH, sizeof(char));
  size_t input_size = MAX_COMMAND_LENGTH;
  getline(&input_buffer, &input_size, stdin);

  // Clear previous command
  int i = 0;
  while (user_command->argv[i]) {
    user_command->argv[i] = NULL;
    i++;
  }
  user_command->argc = 0;
  user_command->input_file = NULL;
  user_command->output_file = NULL;
  user_command->is_background = false;

  // Parse user input and store information
  char *token;
  char *save_ptr = input_buffer;
  int arg_index = 0;
  token = strtok_r(input_buffer, " ", &save_ptr);
  while (token != NULL) {

    // Input Redirection
    if (strcmp(token, "<") == 0) {
      token = strtok_r(NULL, " ", &save_ptr);
      user_command->input_file = strdup(token);
      arg_index++;

    // Output Redirection
    } else if (strcmp(token, ">") == 0) {
      token = strtok_r(NULL, " ", &save_ptr);
      user_command->output_file = strdup(token);
      arg_index++;

    // Run in the background
    } else if (strcmp(token, "&\n") == 0) {
      user_command->is_background = true;

    // Command arguments
    } else if (strcmp(token, "\n") != 0){
      user_command->argv[arg_index] = strdup(token);
      user_command->argc++;
    }

    token = strtok_r(NULL, " ", &save_ptr);
    arg_index++;
  }
  user_command->argv[arg_index] = NULL;
  free(input_buffer);

  // Handle comment or blank input
  bool is_comment = strncmp(user_command->argv[0], "#", 1) == 0;
  bool is_blank_line = user_command->argc == 1 && 
                       strcmp(user_command->argv[0], "\n") == 0;
  if (is_comment || is_blank_line) {
    return FAILURE;
  }
  return SUCCESS;
}