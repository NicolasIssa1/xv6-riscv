#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* 
   This function is to read a line of input from the user.
   It Clears the buffer, displays a prompt, and reads user input until newline.
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

    /* Useful data structures and flags. */
    char *arguments[10];
    int numargs = 0;

    int redirection_left = 0;
    int redirection_right = 0;
    char *file_name_l = 0;
    char *file_name_r = 0;

    int p[2];
    int pipe_cmd = 0;
    int sequence_cmd = 0;

    int i = 0;

    /* Parse the command character by character. */
    for (; i < nbuf; i++) {
        /*This loop is to parse the current charactew and set-up
        vrious flags: sequence_cmd, redirection, pipe_cmd and similar.*/

        // Skip leading spaces and tabs
        while (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t') {
            i++;
        }

        //now we check for specific symbols and set flags
        if (buf[i] == '>') {
            buf[i] = '\0';
            redirection_right = 1;
            i++;
            break;
        } else if (buf[i] == '<') {
            buf[i] = '\0';
            redirection_left = 1;
            i++;
            break;
        } else if (buf[i] == '|') {
            buf[i] = '\0';
            pipe_cmd = 1;
            i++;
            break;
        } else if (buf[i] == ';') {
            buf[i] = '\0';
            sequence_cmd = 1;
            i++;
            break;
        }

        // Continue parsing and adding arguments if there was
        // no redirection or pipe that were found
        if (!(redirection_left || redirection_right)) {
            if (buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\t' && buf[i] != 0) {
                if (numargs < 10) {
                    arguments[numargs++] = &buf[i];
                }
                while (i < nbuf && buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\t' && buf[i] != 0) {
                    i++;
                }
                buf[i] = '\0';
            }
        }
    }

    arguments[numargs] = 0;


    // This check is to notiy if noo commands are given
    if (numargs == 0) {
        fprintf(2, "Error: No command provided. Please enter a valid command.\n");
        exit(1);
    }
    /* Redirection command. Capture the file names. */
    if (redirection_right || redirection_left) {
        while (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t') {
            i++;
        }

        if (redirection_right) {
            file_name_r = &buf[i];
            while (i < nbuf && buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\t' && buf[i] != 0) {
                i++;
            }
            buf[i] = '\0';
        } else if (redirection_left) {
            file_name_l = &buf[i];
            while (i < nbuf && buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\t' && buf[i] != 0) {
                i++;
            }
            buf[i] = '\0';
        }
    }
    /*
      Sequence command. Continue this command in a new process.
      Wait for it to complete and execute the command following ';'.
    */
    if (sequence_cmd) {
        sequence_cmd = 0;
        if (fork() != 0) {
            wait(0);
            run_command(buf + i + 1, nbuf - (i + 1), pcp);
        }
    }

    /*
        If this is a redirection command,
        tie the specified files to std in/out.
    */
    if (redirection_left) {
        // Close stdin
        close(0);
        // Open file for reading
        if (open(file_name_l, O_RDONLY) < 0) {
            fprintf(2, "Failed to open %s for reading\n", file_name_l);
            // Exit if failed to open file
            exit(1);
        }
    }

    if (redirection_right) {
        close(1); // Close stdout
        if (open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC) < 0) { // Open file for writing
            fprintf(2, "Failed to open %s for writing\n", file_name_r);
            exit(1); // Exit if failed to open file
        }
    }

    /* Parsing done. Execute the command. */

/*
   If this command is a CD command, write the arguments to the pcp pipe
   and exit with '2' to tell the parent process about this.
*/
    if (strcmp(arguments[0], "cd") == 0) {
        if (numargs < 2 || chdir(arguments[1]) < 0) {
            fprintf(2, "cd: cannot change directory to %s\n", numargs < 2 ? "" : arguments[1]);
        }
        exit(2);
    } else {
        /*
          Pipe command: fork twice. Execute the left-hand side directly.
          Call run_command recursively for the right side of the pipe.
        */
        if (pipe_cmd) {
            // Create a pipe
            pipe(p);
            
            // Start with the left side of the pipe
            if (fork() == 0) {
                // Redirect stdout to write end of the pipe
                close(1);
                dup(p[1]);
                // Close read end in the child
                close(p[0]);
                // Close write end after duplication
                close(p[1]);
                //Execute the commands for the left side
                exec(arguments[0], arguments);
                fprintf(2, "exec %s failed\n", arguments[0]);
                exit(1);
            }

            // Right side of the pipwe
            if (fork() == 0) {
                close(0); 
                dup(p[0]);
                close(p[0]); 
                close(p[1]);
                // Execute the commads for the right side recursively
                run_command(buf + i + 1, nbuf - (i + 1), pcp);
                exit(0);
            }

            // Parent process closes both ends and waits for children
            close(p[0]);
            close(p[1]);
            wait(0);
            wait(0);
        } else {
            // Normal execution for non-piped commands
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
