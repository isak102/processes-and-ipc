#include <stdio.h>    // puts(), printf(), perror(), getchar()
#include <stdlib.h>   // exit(), EXIT_SUCCESS, EXIT_FAILURE
#include <unistd.h>   // getpid(), getppid(),fork()
#include <sys/wait.h> // wait()

#define READ 0
#define WRITE 1

#define EXIT_ON_ERROR(status, error) \
  if (status < 0)                    \
  {                                  \
    perror(error);                   \
    exit(EXIT_FAILURE);              \
  }

static void child_a(int fd[])
{
  // Close the pipe read descriptor.
  close(fd[READ]);

  // Redirect STDOUT to write to the pipe.
  EXIT_ON_ERROR(dup2(fd[WRITE], STDOUT_FILENO), "dup2 in child a failed\n")

  // Close the dangling pipe write descriptor.
  close(fd[WRITE]);

  EXIT_ON_ERROR(execlp("ls", "ls", "-F", "-1", NULL), "execlp for ls failed\n")
}

static void child_b(int fd[])
{
  // Close the pipe write descriptor.
  close(fd[WRITE]);

  // Redirect STDIN to read from the pipe.
  EXIT_ON_ERROR(dup2(fd[READ], STDIN_FILENO), "dup2 in child b failed\n")

  // Close the dangling pipe read descriptor.
  close(fd[READ]);

  EXIT_ON_ERROR(execlp("nl", "nl", NULL), "execlp for nl failed\n")
}

static void create_fork(int fd[], void (*child_function)(int *))
{
  pid_t pid = fork();

  EXIT_ON_ERROR(pid, "Fork failed");

  if (pid == 0)
  {
    // On success fork() returns 0 in the child.
    child_function(fd);
  }
}

int main()
{
  // File descriptors to the pipe:
  int fd[2];

  // Create the pipe and check for errors.
  pipe(fd);

  // create two children a & b
  create_fork(fd, child_a);
  create_fork(fd, child_b);

  close(fd[READ]);
  close(fd[WRITE]);

  wait(NULL);
  wait(NULL);

  return EXIT_SUCCESS;
}
