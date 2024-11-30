#include "kernel/types.h"
#include "user/user.h"

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
main(int argc, char *argv[])
{
  // pipes[0]
  //     parent write; child read
  // pipes[1]
  //     parent read; child write
  int pipes[2][2];
  if (pipe(pipes[0]) < 0) {
    panic("first pipe()");
  }
  if (pipe(pipes[1]) < 0) {
    panic("second pipe()");
  }

  int pid;
  pid = fork();
  if (pid < 0) {
    panic("fork()");
  }

  // const int N = 4;
  // NOTE const int in C is not constant expression.
  //      char buffer[N] would be VLA.
#define N 4

  char buffer[N];
  int n;
  int my_pid = getpid();
  if (pid == 0) {
    // child
    // read pipes[0]
    // write pipes[1]
    close(pipes[0][1]);
    close(pipes[1][0]);
    n = read(pipes[0][0], buffer, sizeof(buffer));
    // assert(n == 1);
    close(pipes[0][0]);
    fprintf(1, "%d: received ping\n", my_pid);

    write(pipes[1][1], buffer, n);
    close(pipes[1][1]);
  } else {
    // parent
    // write pipes[0]
    // read pipes[1]
    close(pipes[0][0]);
    close(pipes[1][1]);
    int msg_len = 1;
    buffer[0] = 'O';
    write(pipes[0][1], buffer, msg_len);
    close(pipes[0][1]);

    read(pipes[1][0], buffer, msg_len);
    close(pipes[1][0]);
    fprintf(1, "%d: received pong\n", my_pid);
  }

  exit(0);
}
