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
 *                  true for background process
 *                  false for foreground process
 */
struct command {
  char *arguments[MAX_ARGS];
  char *input_file;
  char *output_file;
  bool background;
};

/* Struct: process
 * -----------------------------------------------------------------------------
 * Single node of a linked list that represent a single process.
 *   process_id - the id of the process
 *   next - point to the next node of the process
 */
struct process {
  pid_t process_id;
  struct process *next;
};

/* Struct: status
 * -----------------------------------------------------------------------------
 * Represents the current status of the program
 *   exit_program - whether or not the program should be exited
 *   exit_status - the exit status of the most recent foreground process,
 *                 0 by default or the process was interruped by a signal
 *   kill_signal - the signal that interruped the most recent foreground process,
 *                 0 by default or the process was exited without interruption.
 *   foreground - the process id of the most recent foreground process,
 *                0 by default.
 *   background - A linked list of background processes that has not
 *                finished or reaped yet.
 */
struct status {
  bool exit_program;
  int exit_status;
  int kill_signal;
  pid_t foreground;
  struct process *background;
};

/* Global Variable */
struct status program_status = {false, SUCCESS, 0, 0, NULL};

/* Function Prototypes */
int get_command(struct command *user_command);
void reset_command(struct command *user_command, bool reset_command);
char *expand_variable(char *unexpanded_string);
void execute_command(struct command *user_command);
void report_status(void);
void change_directory(struct command *user_command);
void exit_and_cleanup(struct command *user_command);
void fork_and_execute(struct command *user_command);
void handle_sigchld(void);

/* Main */
int main(void) {

  struct command user_command;
  reset_command(&user_command, true);
  
  while (!program_status.exit_program) {
    
    if (get_command(&user_command) == SUCCESS)
      execute_command(&user_command);
  }

  exit_and_cleanup(&user_command);
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
      user_command->background = true;

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
 *   and background to false.
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

  user_command->background = false;
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
 * Take a pointer to user_command as parameter.
 * Execute status, cd, and exit commands in the foreground and
 *   create a new process and execute for other commands.
 */
void execute_command(struct command *user_command) {

  char *first_argument = user_command->arguments[0];
  
  if (strcmp(first_argument, "status") == 0) {
    report_status();

  } else if (strncmp(first_argument, "cd", 2) == 0) {
    change_directory(user_command);

  } else if (strcmp(first_argument, "exit") == 0) {
    program_status.exit_program = true;

  } else {
    fork_and_execute(user_command);
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
void report_status(void) {

  if (program_status.kill_signal) {
    printf("terminated by signal %d\n", program_status.kill_signal);

  } else {
    printf("exit value %d\n", program_status.exit_status);
  }
  
  fflush(stdout);
}

/*
 * Function: exit_and_cleanup
 * -----------------------------------------------------------------------------
 * Take a pointer to user_command as parameter, 
 *   free all allocated memory and kill all child processes.
 * Also take a pointer to exit_program flag as parameter and change it to true.
 */
void exit_and_cleanup(struct command *user_command) {

  reset_command(user_command, false);
  // TO DO: free all background processes in program_status
  // TO DO: kill all child processes
  // TO DO: close all file pointers
}

/*
 * Function: fork_and_execute
 * -----------------------------------------------------------------------------
 * Take a pointer to user_command as parameter, 
 *   create a child process to execute the arguments
 *   in the background or the foreground based on user input.
 * Also update the last foreground process exit status appropriately.
 */
void fork_and_execute(struct command *user_command) {

  int child_exit_method;
  pid_t spwan_pid = fork();

  switch(spwan_pid) {

    // Forking error
    case -1:

      perror("fork() failed\n");
      exit(FAILURE);

    // Child process
    case 0:

      execvp(user_command->arguments[0], user_command->arguments);
      if (user_command->background)
        printf("\n");
        fflush(stdout);
      perror(user_command->arguments[0]);
      exit(FAILURE);

    // Parent Process
    default:

      // Creating background process
      if (user_command->background) {

        // Adding background pid to program_status
        struct process *new_background_process = (struct process *)malloc(sizeof(struct process));
        new_background_process->process_id = spwan_pid;
        new_background_process->next = program_status.background;
        program_status.background = new_background_process;

        printf("background pid is %d\n", spwan_pid);
        fflush(stdout);

        waitpid(spwan_pid, &child_exit_method, WNOHANG);

        // Listen for SIGCHLD signal
        struct sigaction sa_sigchld;
        sa_sigchld.sa_handler = (void *)handle_sigchld;
        sigemptyset(&sa_sigchld.sa_mask);
        sa_sigchld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
        sigaction(SIGCHLD, &sa_sigchld, NULL);
        
      // Creating foreground process
      } else {

        // Adding foreground pid to program_status
        program_status.foreground = spwan_pid;
        waitpid(spwan_pid, &child_exit_method, 0);

        // Normal exit
        if (WIFEXITED(child_exit_method)) {
          program_status.exit_status = WEXITSTATUS(child_exit_method);
          program_status.kill_signal = 0;

        // Exited by interruption
        } else {
          program_status.kill_signal = WTERMSIG(child_exit_method);
          program_status.exit_status = 0;
          report_status();
        }
      }
  }
}

/*
 * Function: handle_sigchld
 * -----------------------------------------------------------------------------
 * Listen and handle SIGCHLD signals returned by the background processes.
 * Also report exit value or termination signals.
 */
void handle_sigchld(void) {

  pid_t pid;
  int exit_method;
  while ((pid = waitpid(-1, &exit_method, WNOHANG)) > 0) {

    // Background process
    if (pid != program_status.foreground) {
      printf("\nbackground pid %d is done: ", pid);

      // Normal exit
      if (WEXITSTATUS(exit_method)) {
        printf("exit value %d\n", WEXITSTATUS(exit_method));

      // Exited by interruption
      } else {
        printf("terminated by signal %d\n", WTERMSIG(exit_method));
      }
    }
  }
}