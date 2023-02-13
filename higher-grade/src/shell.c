#include "parser.h" // cmd_t, position_t, parse_commands()

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h> //fcntl(), F_GETFL

#define READ 0
#define WRITE 1

/**
 * For simplicitiy we use a global array to store data of each command in a
 * command pipeline .
 */
cmd_t commands[MAX_COMMANDS];

/**
 *  Debug printout of the commands array.
 */
void print_commands(int n)
{
  for (int i = 0; i < n; i++)
  {
    printf("==> commands[%d]\n", i);
    printf("  pos = %s\n", position_to_string(commands[i].pos));
    printf("  in  = %d\n", commands[i].in);
    printf("  out = %d\n", commands[i].out);

    print_argv(commands[i].argv);
  }
}

/**
 * Returns true if file descriptor fd is open. Otherwise returns false.
 */
int is_open(int fd)
{
  return fcntl(fd, F_GETFL) != -1 || errno != EBADF;
}

void fork_error()
{
  perror("fork() failed)");
  exit(EXIT_FAILURE);
}

void pipe_error()
{
  perror("pipe() failed)");
  exit(EXIT_FAILURE);
}

void dup2_error()
{
  perror("dup2() failed)");
  exit(EXIT_FAILURE);
}

void redirect_file_descriptors(int *fd, unsigned int command_index)
{
  cmd_t *cmd = &commands[command_index];

  switch (cmd->pos)
  {
  case first:
    // Close the pipe read descriptor.
    close(fd[READ]);

    if (dup2(fd[WRITE], STDOUT_FILENO) == -1)
    {
      dup2_error();
    }
    cmd->out = fd[WRITE];

    // Close the dangling pipe write descriptor.
    close(fd[WRITE]);
    break;

  case last:

    close(commands[command_index - 1].fd[WRITE]);

    if (dup2(commands[command_index - 1].fd[READ], STDIN_FILENO) == -1)
    {
      dup2_error();
    }
    cmd->in = commands[command_index - 1].fd[READ];

    // Close the dangling pipe read descriptor.
    close(commands[command_index - 1].fd[READ]);
    break;

  case middle:
    //puts("Middle child");
    //printf("fd WRITE %d\n", fd[WRITE]);

    // Close the pipe read descriptor.
    close(fd[READ]);

    if (dup2(commands[command_index - 1].out, cmd->in) == -1)
    {
      //puts("Middle child 1");

      dup2_error();
    }
    if (dup2(fd[WRITE], cmd->out) == -1)
    {
      //puts("Middle child2 ");

      dup2_error();
    }
    cmd->in = commands[command_index - 1].out;
    cmd->out = fd[WRITE];

    // Close both the dangling pipe read and write descriptors.
    close(fd[WRITE]);
    //printf("middle child closed!\n");

    break;
  default:
    // unknown
    // single
    break;
  }
}

/**
 *  Fork a proccess for command with index i in the command pipeline. If needed,
 *  create a new pipe and update the in and out members for the command..
 */
void fork_cmd(int i)
{
  pid_t pid;
  int fd[2];

  // theres no need for a pipe if we have a single command

  bool create_pipes = (commands[i].pos == first) || (commands[i].pos == middle);
  if (create_pipes)
  {
    puts("Pipe created");
    if (pipe(fd) == -1)
    {
      pipe_error();
    }
    commands[i].fd[READ] = fd[READ];
    commands[i].fd[WRITE] = fd[WRITE];
  }

  switch (pid = fork())
  {
  case -1:
    fork_error();
    break;
  case 0:
    // Child process after a successful fork().
    redirect_file_descriptors(fd, i);
    // Execute the command in the contex of the child process.
    execvp(commands[i].argv[0], commands[i].argv);

    // If execvp() succeeds, this code should never be reached.
    fprintf(stderr, "shell: command not found: %s\n", commands[i].argv[0]);
    exit(EXIT_FAILURE);
    break;
  default:
    // Parent process after a successful fork().
    break;
  }
}

void close_all_pipes(int number_of_commands) {
  for (int i=0; i < number_of_commands; i++) {
    if ((commands[i].pos == first) || (commands[i].pos == middle)) {
      close(commands[i].fd[READ]);
      close(commands[i].fd[WRITE]);
    }
  }
}

/**
 *  Fork one child process for each command in the command pipeline.
 */
void fork_commands(int n)
{
  for (int i = 0; i < n; i++)
  {
    fork_cmd(i);
  }

  close_all_pipes(n);
}

/**
 *  Reads a command line from the user and stores the string in the provided
 *  buffer.
 */
void get_line(char *buffer, size_t size)
{
  getline(&buffer, &size, stdin);
  buffer[strlen(buffer) - 1] = '\0';
}

void wait_for_all_cmds(int n)
{
  for (int i = 0; i < n; i++)
  {
    wait(NULL);
  }
}

int main()
{
  int n;             // Number of commands in a command pipeline.
  size_t size = 128; // Max size of a command line string.
  char line[size];   // Buffer for a command line string.

  while (true)
  {
    printf(" >>> ");

    get_line(line, size);

    n = parse_commands(line, commands);

    fork_commands(n);

    wait_for_all_cmds(n);
  }

  exit(EXIT_SUCCESS);
}
