#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* 
   Function to read a line of input from the user.
   Clears the buffer, displays a prompt, and reads user input until newline.
*/
int getcmd(char *buf, int nbuf) {
  // Clear the buffer to avoid leftovers from previous commands
  for (int k = 0; k < nbuf; k++) buf[k] = 0;

  // Display the prompt
  fprintf(1, ">>> ");

  // Read input character by character
  int i = 0;
  while (i < nbuf - 1) {
    int n = read(0, &buf[i], 1);
    if (n < 1) return -1;
    if (buf[i] == '\n') break;
    i++;
  }

  // Null-terminate the string
  buf[i] = '\0';
  return i;
}

/* 
   Function to parse and execute a single command.
   Handles commands other than `cd`.
*/
void run_command(char *buf) {
  char *arguments[10] = {0}; // Array to store command arguments
  int numargs = 0;

  // Parse the command into arguments
  arguments[numargs++] = buf;
  for (int i = 0; buf[i] != '\0'; i++) {
    if (buf[i] == ' ' || buf[i] == '\t') {
      buf[i] = '\0';
      while (buf[i + 1] == ' ' || buf[i + 1] == '\t') i++;
      arguments[numargs++] = &buf[i + 1];
    }
  }
  arguments[numargs] = 0; // Null-terminate the arguments array

  // Execute the command
  exec(arguments[0], arguments);
  // If exec fails
  fprintf(2, "exec %s failed\n", arguments[0]);
  exit(1);
}

/* 
   Main function to run the shell, repeatedly calling getcmd and handling `cd`.
   Forks a child process to execute each command.
*/
int main(void) {
  static char buf[100];

  while (getcmd(buf, sizeof(buf)) >= 0) {
    char *cmd = buf;

    do {
      // Skip leading spaces
      while (*cmd == ' ' || *cmd == '\t') cmd++;

      // Find next command end or separator
      char *next_cmd = strchr(cmd, ';');
      if (next_cmd) *next_cmd++ = '\0'; // Split command by ';'

      // Handle `cd` in the parent process
      if (strcmp(cmd, "cd") == 0 || (cmd[0] == 'c' && cmd[1] == 'd' && (cmd[2] == ' ' || cmd[2] == '\0'))) {
        char *dir = cmd + 2;
        while (*dir == ' ') dir++;
        if (*dir == '\0') dir = "/";
        if (chdir(dir) < 0) {
          fprintf(2, "cd: cannot change directory %s\n", dir);
        }
      } else {
        if (fork() == 0) {
          run_command(cmd); // Run other commands in child
          exit(0);
        }
        wait(0); // Wait for child process
      }

      // Move to the next command
      cmd = next_cmd;
    } while (cmd && *cmd);
  }

  exit(0);
}

