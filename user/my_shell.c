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
   Recursive function to parse and execute a command.
   Handles command execution and redirection based on the pseudocode.
*/
__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *pcp) {
  char *arguments[10] = {0};
  int numargs = 0;
  int ws = 1;

  int redirection_left = 0;
  int redirection_right = 0;
  char *file_name_l = 0;
  char *file_name_r = 0;

  int pipe_cmd = 0;
  int sequence_cmd = 0;
  int i = 0;

  // Parse the command character by character
  for (; i < nbuf; i++) {
    if (buf[i] == ' ' || buf[i] == '\t') {
      buf[i] = '\0';
      ws = 1;
    } else if (ws) {
      arguments[numargs++] = &buf[i];
      ws = 0;
    }

    if (buf[i] == '|') {
      buf[i] = '\0';
      pipe_cmd = 1;
      break;
    }

    if (buf[i] == ';') {
      buf[i] = '\0';
      sequence_cmd = 1;
      break;
    }

    if (buf[i] == '<') {
      buf[i] = '\0';
      redirection_left = 1;
      while (buf[++i] == ' '); // Skip whitespace after `<`
      file_name_l = &buf[i];
    }

    if (buf[i] == '>') {
      buf[i] = '\0';
      redirection_right = 1;
      while (buf[++i] == ' '); // Skip whitespace after `>`
      file_name_r = &buf[i];
    }
  }

  arguments[numargs] = 0;

  // Handle sequence command with `;`
  if (sequence_cmd) {
    if (fork() == 0) {
      run_command(buf + i + 1, nbuf - (i + 1), pcp);
      exit(0);
    } else {
      wait(0);
    }
  }

  // Handle input redirection if found
  if (redirection_left) {
    close(0); // Close stdin
    if (open(file_name_l, 0) < 0) { // Using 0 to open for reading
      fprintf(2, "Failed to open %s for reading\n", file_name_l);
      exit(1);
    }
  }

  // Handle output redirection if found
  if (redirection_right) {
    close(1); // Close stdout
    if (open(file_name_r, 1 | 0x200 | 0x400) < 0) { 
      fprintf(2, "Failed to open %s for writing\n", file_name_r);
      exit(1);
    }
  }

  // Handle piping
  if (pipe_cmd) {
    int p[2];
    pipe(p);

    if (fork() == 0) { // Left side of the pipe
      close(p[0]); // Close read end
      close(1); // Close stdout
      dup(p[1]); // Redirect stdout to write end of pipe
      close(p[1]);
      exec(arguments[0], arguments); // Execute left side command
      fprintf(2, "exec %s failed\n", arguments[0]);
      exit(1);
    } else {
      if (fork() == 0) { // Right side of the pipe
        close(p[1]); // Close write end
        close(0); // Close stdin
        dup(p[0]); // Redirect stdin to read end of pipe
        close(p[0]);
        run_command(buf + i + 1, nbuf - (i + 1), pcp); // Execute right side command
      } else {
        close(p[0]);
        close(p[1]);
        wait(0);
        wait(0);
      }
    }
  } else {
    // Normal execution for non-piped commands
    if (strcmp(arguments[0], "cd") == 0) {
      if (numargs < 2 || chdir(arguments[1]) < 0) {
        fprintf(2, "cd: cannot change directory to %s\n", numargs < 2 ? "" : arguments[1]);
      }
      exit(2);
    } else {
      if (fork() == 0) {
        exec(arguments[0], arguments);
        fprintf(2, "exec %s failed\n", arguments[0]);
        exit(1);
      } else {
        wait(0);
      }
    }
  }
  exit(0);
}



/* 
   Main function to run the shell, repeatedly calling getcmd and handling `cd`.
*/
int main(void) {
  static char buf[100];
  int pcp[2];
  pipe(pcp);

  while (getcmd(buf, sizeof(buf)) >= 0) {
    char *cmd = buf;

    do {
      // Skip leading spaces
      while (*cmd == ' ' || *cmd == '\t') cmd++;

      // Find next command end or separator
      char *next_cmd = strchr(cmd, ';');
      if (next_cmd) {
        *next_cmd = '\0'; 
        next_cmd++;
      }


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
          run_command(cmd, 100, pcp);
          exit(0);
        }
        wait(0);
      }

      cmd = next_cmd;
    } while (cmd && *cmd != '\0');
  }

  exit(0);
}