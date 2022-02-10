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
#include <fcntl.h>

/* Constants */
#define MAX_COMMAND_LENGTH 2048
#define MAX_ARGS 512
#define SUCCESS 0
#define FAILURE 1
#define INPUT 0
#define OUTPUT 1

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
 * Single node of a linked list that represents a single process.
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
 *   foreground_only - if processes should be run in the foreground only.
 *                     (fg-only mode)
 */
struct status {
  bool exit_program;
  int exit_status;
  int kill_signal;
  pid_t foreground;
  struct process *background;
  bool foreground_only;
};

/* Global Variable */
struct status program_status = {false, SUCCESS, 0, 0, NULL, false};
struct sigaction sa_sigint = {0};
struct sigaction sa_sigtstp = {0};
struct sigaction sa_sigchld = {0};

/* Function Prototypes */
int get_command(struct command *user_command);
void reset_command(struct command *user_command, bool reset_command);
char *expand_variable(char *unexpanded_string);
void execute_command(struct command *user_command);
void report_status(void);
void change_directory(struct command *user_command);
void exit_and_cleanup(struct command *user_command);
void fork_and_execute(struct command *user_command);
void handle_sigchld(int signal);
void handle_sigtstp(int signal);
void push_background_process(int process_id);
bool pop_background_process(int process_id);
void write_integer(int num);
bool redirect(struct command *user_command, int mode);

/* Main */
int main(void) {

  // Listen and ignore SIGCHLD
  sa_sigint.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sa_sigint, NULL);

  // Handle SIGTSTP
  sa_sigtstp.sa_handler = handle_sigtstp;
  sigfillset(&sa_sigtstp.sa_mask);
  sigdelset(&sa_sigtstp.sa_mask, SIGCHLD);
  sa_sigtstp.sa_flags = 0;
  sigaction(SIGTSTP, &sa_sigtstp, NULL);

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
  int num_chars = getline(&input_buffer, &input_size, stdin);
  if (num_chars == -1)
    clearerr(stdin);

  // Parse user input and store information
  int arg_index = 0;
  char *token;
  char *save_ptr = input_buffer;
  token = strtok_r(input_buffer, " \n", &save_ptr);

  // Handle comment or blank input
  bool is_comment = strncmp(input_buffer, "#", 1) == 0;
  bool is_blank_line = (bool)!token;
  if (is_comment || is_blank_line) {
    free(input_buffer);
    return FAILURE;
  }

  while (token != NULL) {

    // Input Redirection
    if (strcmp(token, "<") == 0) {
      token = strtok_r(NULL, " \n", &save_ptr);
      user_command->input_file = expand_variable(token);

    // Output Redirection
    } else if (strcmp(token, ">") == 0) {
      token = strtok_r(NULL, " \n", &save_ptr);
      user_command->output_file = expand_variable(token);

    // Run in the background if the foreground-only mode is off
    } else if (strcmp(token, "&") == 0) {
      user_command->background = !program_status.foreground_only;

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
    write(STDOUT_FILENO, "terminated by signal ", 21);
    write_integer(program_status.kill_signal);

  } else {
    write(STDOUT_FILENO, "exit value ", 11);
    write_integer(program_status.exit_status);
  }

  write(STDOUT_FILENO, "\n", 1);
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
 * Also update/add the foreground and background process to program_status.
 */
void fork_and_execute(struct command *user_command) {
  
  // int child_exit_method;
  pid_t spwan_pid = fork();

  switch(spwan_pid) {

    // Forking error
    case -1:

      perror("fork() failed\n");
      exit(FAILURE);

    // Child process
    case 0:

      // Only foreground process should listen for SIGINT
      if (!user_command->background) {
        sa_sigint.sa_handler = SIG_DFL;
        sigaction(SIGINT, &sa_sigint, NULL);
      }

      // Both foreground and background processes should ignore SIGTSTP
      sa_sigtstp.sa_handler = SIG_IGN;
      sigaction(SIGTSTP, &sa_sigtstp, NULL);

      if (!redirect(user_command, INPUT) || !redirect(user_command, OUTPUT))
        exit(FAILURE);
      
      execvp(user_command->arguments[0], user_command->arguments);
      perror(user_command->arguments[0]);
      exit(FAILURE);

    // Parent Process
    default:

      // Listen for SIGCHLD in order to reap zombie processes in the background.
      sa_sigchld.sa_handler = (void *)handle_sigchld;
      sigfillset(&sa_sigchld.sa_mask);
      sa_sigchld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
      sigaction(SIGCHLD, &sa_sigchld, NULL);

      // Background process
      if (user_command->background) {

        // Adding background pid to program_status
        push_background_process(spwan_pid);
        printf("background pid is %d\n", spwan_pid);
        fflush(stdout);

      // Foreground process
      } else {

        // Adding foreground pid to program_status
        program_status.foreground = spwan_pid;

        // Pause program until the foreground process finishes
        while (program_status.foreground)
          pause();
      }
  }
}

/*
 * Function: handle_sigchld
 * -----------------------------------------------------------------------------
 * Listen and handle SIGCHLD signals returned by 
 *   both the background and foreground processes.
 * Also, report and/or update  exit value or termination signals as well as
 *   updating foreground/background processes in program_status.
 */
void handle_sigchld(int signal) {

  if (signal != SIGCHLD)
    return;

  pid_t pid;
  int exit_method;

  while ((pid = waitpid(-1, &exit_method, WNOHANG)) > 0) {

    // Background process
    if (pop_background_process(pid)) {

      // Normal exit
      if (WIFEXITED(exit_method)) {
        write(STDOUT_FILENO, "\nbackground pid ", 16);
        write_integer(pid);
        write(STDOUT_FILENO, " is done: ", 10);
        write(STDOUT_FILENO, "exit value ", 11);
        write_integer(WEXITSTATUS(exit_method));
        write(STDOUT_FILENO, "\n: ", 3);
      }

      // Exited by interruption
      if (WIFSIGNALED(exit_method)) {
        write(STDOUT_FILENO, "background pid ", 15);
        write_integer(pid);
        write(STDOUT_FILENO, " is done: ", 10);
        write(STDOUT_FILENO, "terminated by signal ", 21);
        write_integer(WTERMSIG(exit_method));
        write(STDOUT_FILENO, "\n", 1);
      }

    // Foreground process
    } else {

      // Normal exit
      if (WIFEXITED(exit_method)) {
        program_status.exit_status = WEXITSTATUS(exit_method);
        program_status.kill_signal = 0;
      }

      // Exited by interruption
      if (WIFSIGNALED(exit_method)) {
        program_status.kill_signal = WTERMSIG(exit_method);
        program_status.exit_status = 0;
        report_status();
      }

      program_status.foreground = 0;
    }
  }
}

/*
 * Function: handle_sigtstp
 * -----------------------------------------------------------------------------
 * Listen and handle SIGTSTP signals returned by the terminal.
 * Toggle foreground-only mode upon receive and display the status
 *   when there is no foreground process running.
 */
void handle_sigtstp(int signal) {

  // Toggle foreground-only mode
  program_status.foreground_only = !program_status.foreground_only;

  // Display the status foreground-only mode when there is not foreground process.
  while (program_status.foreground) {};
  if (program_status.foreground_only) {
    write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n", 50);
  } else {
    write(STDOUT_FILENO, "\nExiting foreground-only mode\n", 30);
  }
}

/*
 * Function: push_background_process
 * -----------------------------------------------------------------------------
 * Takes a background process pid as parameter.
 * Create a process node with the provided pid and 
 *   add the node to the end of the background process linked list.
 */
void push_background_process(pid_t process_id) {

  // Create new node
  struct process *new_background_process = (struct process *)malloc(sizeof(struct process));
  new_background_process->process_id = process_id;
  new_background_process->next = NULL;

  // Add node to head if not exist
  if (!program_status.background) {
    program_status.background = new_background_process;
    return;
  }

  // Add node to the last location of the linked list
  struct process *current_background_process = program_status.background;
  while (current_background_process->next) {
    current_background_process = current_background_process->next;
  }

  current_background_process->next = new_background_process;
}

/*
 * Function: pop_background_process
 * -----------------------------------------------------------------------------
 * Takes a process pid as parameter.
 * Remove/free the node from the background process linked list if available.
 * Return true if the node has been removed from the background processes.
 * Return false if there is no background processes matching the provided pid.
 */
bool pop_background_process(pid_t process_id) {

  // No background processes currently
  if (!program_status.background) {
    return false;
  }

  // Search for matching background proces and remove it
  struct process *current_background_process = program_status.background;
  struct process *previous_background_process = NULL;
  while (current_background_process->process_id != process_id) {
    previous_background_process = current_background_process;
    current_background_process = current_background_process->next;

    // process id not found
    if (!current_background_process)
      return false;
  }

  // Deleting the background process
  if (program_status.background->process_id == process_id) {
    program_status.background = program_status.background->next;
  } else {
    previous_background_process->next = current_background_process->next;
  }
  
  free(current_background_process);
  return true;
}

/*
 * Function: write_integer
 * -----------------------------------------------------------------------------
 * A reentrant function that takes an integer as parameter and 
 *   print it to the terminal.
 */
void write_integer(int num) {

  // Handle zero
  if (num == 0) {
    write(STDOUT_FILENO, "0", 1);
    return;
  }

  // Get number of digits
  int num_digit = 0;
  int dividend = num;
  while (dividend > 0) {
    dividend = (int) (dividend / 10);
    num_digit += 1;
  }

  // Create a number string
  char num_string[num_digit];
  int index = num_digit - 1;
  dividend = num;
  while (index >= 0) {
    num_string[index] = dividend - (int)(dividend / 10) * 10 + 48;
    dividend = (int) (dividend / 10);
    index--;
  }

  // Write to STDOUT
  write(STDOUT_FILENO, num_string, num_digit);
}

/*
 * Function: redirect
 * -----------------------------------------------------------------------------
 * Takes the user command and redirection mode as parameters.
 *   Redirect the input or output to the provided filename (if available)
 *     based on the redirection mode.
 * If the process is a background process and no filename is given, 
 *   then the input or output will be redirected to /dev/null
 */
bool redirect(struct command *user_command, int mode) {

  // Get filename
  char *filename;
  if (mode == INPUT) {
    filename = user_command->input_file;
  } else {
    filename = user_command->output_file;
  }

  // Handle missing files
  if (!filename) {
    if (user_command->background)
      filename = "/dev/null";
    else
      return true;
  }

	// Get file name and open file
  int file_pointer;
  char *mode_string;
  if (mode == INPUT) {
    file_pointer = open(filename, O_RDONLY);
    mode_string = "input";

  } else {
    file_pointer = open(filename, O_WRONLY | O_CREAT | O_TRUNC , 0644);
    mode_string = "output";
  }

  // Handle file opening error
	if (file_pointer == -1) { 
		printf("cannot open %s for %s\n", filename, mode_string);
		return false;
	}

	// Redirection to designated file
  int result = dup2(file_pointer, mode);
	if (result == -1) { 
		printf("redirection to %s failed\n", filename); 
		return false;
	}

  return true;
}