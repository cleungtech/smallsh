/* Libraries */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Constants */
#define MAX_COMMAND_LENGTH 2048
#define MAX_ARGS 512

/* Structs */

/* Struct: command
 * ----------------------------------------------------------------------------
 * Struct that represent user's command
 *   command_args - points to the array of command line arguments 
 *   input_file - filename of input file for input redirection
 *   output_file - filename of output file for output redirection
 *   is_background - True if the process should be ran in the background. False otherwise.
 */
struct command {
  char *command_args[MAX_ARGS];
  char *input_file;
  char *output_file;
  bool is_background;
};

/* Function Prototypes */
void get_command(struct command user_command);

/* Main */
int main(void) {

  bool exit_program = false;
  struct command user_command;
  while (!exit_program) {
    get_command(user_command);
  }
  
  return 0;
}

/*
 * Function: get_command
 * ----------------------------------------------------------------------------
 * Prompt user for a command input, parse it, and 
 * Store the information in the given parameter struct command user_command
 */
void get_command(struct command user_command) {

  char buffer[MAX_COMMAND_LENGTH];
  printf(": ");
  scanf("%s", buffer);
}