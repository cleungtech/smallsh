/* Libraries */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

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

/* Main */
int main(void) {

  bool exit_program = false;
  struct command user_command;
  reset_command(&user_command);
  
  while (!exit_program) {

    if (get_command(&user_command) == SUCCESS) {

      printf("user_command.arguments: %s\n", user_command.arguments);
      printf("user_command.output_file: %s\n", user_command.output_file);
      printf("user_command.input_file: %s\n", user_command.input_file);
      printf("user_command.is_background: %d\n", user_command.is_background);
    }

    reset_command(&user_command);

  }
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
  char *save_ptr = input_buffer;
  token = strtok_r(input_buffer, " \n", &save_ptr);
  user_command->arguments = (char *)calloc(MAX_COMMAND_LENGTH, sizeof(char));

  while (token != NULL) {

    // Input Redirection
    if (strcmp(token, "<") == 0) {
      token = strtok_r(NULL, " \n", &save_ptr);
      user_command->input_file = strdup(token);

    // Output Redirection
    } else if (strcmp(token, ">") == 0) {
      token = strtok_r(NULL, " \n", &save_ptr);
      user_command->output_file = strdup(token);

    // Run in the background
    } else if (strcmp(token, "&") == 0) {
      user_command->is_background = true;

    // Command arguments
    } else {

      // Add space in between arguments except for the first one
      if (first_token) {
        strcpy(user_command->arguments, token);
        first_token = false;

      } else {
        sprintf(user_command->arguments, "%s %s", user_command->arguments, token);
      }
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