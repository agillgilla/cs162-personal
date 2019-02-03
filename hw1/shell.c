#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd, "pwd", "print the current working directory"},
  {cmd_cd, "cd", "change the current working directory"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/* Prints the current working directory */
int cmd_pwd(unused struct tokens *tokens) {
  /* Get the current working directory of the shell */
  char cwd[PATH_MAX];
  getcwd(cwd, sizeof(cwd));
  printf("%s\n", cwd);
  return 0;
}

/* Changes the directory to specified argument */
int cmd_cd(struct tokens *tokens) {
  if (tokens_get_length(tokens) > 1) {
    char *dirStr = tokens_get_token(tokens, 1);

    DIR* dir = opendir(dirStr);
    if (dir) {
      /* Directory exists. */
      closedir(dir);
      chdir(dirStr);
      return 0;
    }
    else if (ENOENT == errno) {
      /* Directory does not exist. */
      printf("cd: %s: No such file or directory\n", dirStr);
      return -1;
    } else {
      printf("Unknown error changing to directory: %s\n", dirStr);
      return -1;
    }
  } else {
    /* No args.  cd to home directory */
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    chdir(homedir);
    return 0;
  }
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGKILL, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGCONT, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
}

int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      
      char *filename = tokens_get_token(tokens, 0);

      bool redirect_out = false;
      bool redirect_in = false;
      char *out_filename;
      char *in_filename;
      int outfd = -1;
      int infd = -1;
      int out_char_index = -1;
      int in_char_index = -1;

      for (int i = 0; i < tokens_get_length(tokens); i++) {
        if (strcmp(">", tokens_get_token(tokens, i)) == 0) {
          redirect_out = true;
          out_char_index = i;
          if (i < tokens_get_length(tokens) - 1) {
            out_filename = tokens_get_token(tokens, i + 1);
            outfd = open(out_filename, O_CREAT|O_WRONLY|O_TRUNC, 0644);
          }
          break;
        } else if (strcmp("<", tokens_get_token(tokens, i)) == 0) {
          redirect_in = true;
          in_char_index = i;
          if (i < tokens_get_length(tokens) - 1) {
            in_filename = tokens_get_token(tokens, i + 1);
            infd = open(in_filename, O_RDONLY);
          }
          break;
        }
      }

    
      char *full_file_path = NULL;
      char *path = getenv("PATH");

      char *path_copy = malloc(strlen(path) + 1); 
      strcpy(path_copy, path);

      char *contains_slash = strchr(filename, '/');

      if (contains_slash == NULL) { /* Only check $PATH if no slash in filename */
        char *path_split = strtok(path_copy, ":");

        /* Iterate through all dirs in $PATH */
        while (path_split != NULL) {
          /* malloc +2 (1 for slash between path and filename and 1 for null terminator) */
          char *full_path = malloc(strlen(path_split) + strlen(filename) + 2); 
          if (full_path == NULL) {
            printf("Error on memory allocation for path resolutions\n");
          }
          strcpy(full_path, path_split);
          strcat(full_path, "/");
          strcat(full_path, filename);
          if (access(full_path, F_OK) != -1) {
            /* File exists, break from loop and execute */
            full_file_path = full_path;
            break;
          }
          path_split = strtok(NULL, ":");
        }
      }

      free(path_copy);

      if (full_file_path == NULL) {
        full_file_path = filename;
      }


      if (access(full_file_path, F_OK) == -1) {
        /* Executable doesn't exist and isn't in $PATH */
        printf("%s: No such file or directory\n", full_file_path);
      } else {
        /* Spawn a child to run the program */
        pid_t pid = fork();

        if (pid == 0) { /* Child process */

          /* Set pgid to child pid */
          setpgrp();
          /* Move this child process to the foreground */
          tcsetpgrp(STDIN_FILENO, getpid());
          
          printf("Child pgid: %ld\n", (long) getpgid(getpid()));

          size_t argc;
        
          if (redirect_out) {
            argc = out_char_index;
          } else if (redirect_in) {
            argc = in_char_index;
          } else {
            argc = tokens_get_length(tokens);
          }

          char *argv[argc + 1];

          for (int i = 0; i < argc; i++) {
            argv[i] = tokens_get_token(tokens, i);
          }
          
          argv[argc] = (char *) NULL;

          /* Shell only supports one > or < at a time */
          if (redirect_out) {
            /* Replace stdout */
            if (!outfd) {
              printf("Error opening output file: %s\n", out_filename);
            } else {
              dup2(outfd, STDOUT_FILENO); 
              close(outfd);
            }
          } else if (redirect_in) {
            /* Replace stdin */
            if (!infd) {
              printf("Error opening input file: %s\n", in_filename);
            } else {
              dup2(infd, STDIN_FILENO); 
              close(infd);
            }
          }

          signal(SIGINT, SIG_DFL);
          signal(SIGQUIT, SIG_DFL);
          signal(SIGKILL, SIG_DFL);
          signal(SIGTERM, SIG_DFL);
          signal(SIGTSTP, SIG_DFL);
          signal(SIGTSTP, SIG_DFL);
          signal(SIGCONT, SIG_DFL);
          signal(SIGTTIN, SIG_DFL);
          signal(SIGTTOU, SIG_DFL);

          execv(full_file_path, argv);
          /* If execv fails it will get here */

        } else if (pid < 0) {
          /* Error on fork() */
          printf("Error on creating new process: %s\n", full_file_path);
          if (outfd) {
            close(outfd);
          }
          if (infd) {
            close(infd);
          }
        } else { /* Parent process */
          printf("Parent pgid: %ld\n", (long) getpgid(getpid()));
          if (outfd) {
            close(outfd);
          }
          if (infd) {
            close(infd);
          }
          waitpid(pid, 0, 0); /* wait for child to exit */

          tcsetpgrp(shell_terminal, shell_pgid);
        }
      }      
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
