#include "kernel/types.h"

#include "user/user.h"

#define N_PROCESSES 128

int prev_pipe[2];
int next_pipe[2];

void print_help();
void primes(int p);
// if a function call does not return 0, report error.
#define CHECK_CALL(call)            \
  {                                 \
    if ((call) != 0) {              \
      fprintf(2, #call "failed\n"); \
      exit(1);                      \
    }                               \
  }

void
print_help()
{
  const char *usage_str = "usage: primes\n";
  fprintf(1, usage_str);
}

void
primes(int p)
{
  /* NOTE
     two choices:
     1. get the "p" variable from the function parameter
     2. get the "p" variable when reading the first number from the pipe
  */
  int n_read;
  int n_write;
  int number;
  int flag_has_created_next_pipe = 0;

  printf("%d\n", p);
  while ((n_read = read(prev_pipe[0], &number, sizeof(number))) != 0) {
    // if n_read != 0, then the pipe is not closed.
    // 1. there is data
    // 2. there is error
    if (n_read < 0 || n_read != sizeof(number)) {
      fprintf(2, "primes: error in read()\n");
      fprintf(2, "p = %d, n_read = %d\n", p, n_read);
      exit(1);
    }
    if (number % p == 0) {
      // number is not a prime
      continue;
    }
    // number may be a prime
    if (flag_has_created_next_pipe == 0) {
      flag_has_created_next_pipe = 1;
      // create the next pipe
      CHECK_CALL(pipe(next_pipe));
      int pid;
      pid = fork();
      if (pid < 0) {
        fprintf(2, "primes: error in fork()\n");
        fprintf(2, "p = %d\n", p);
        exit(1);
      }

      if (pid == 0) {
        // child
        close(prev_pipe[0]);
        prev_pipe[0] = next_pipe[0];
        close(next_pipe[1]);
        primes(number);
      } else {
        // parent
        close(next_pipe[0]);
      }
    }
    // write it to the next process
    n_write = write(next_pipe[1], &number, sizeof(number));
    // assert(n_write == sizeof(number));
    (void) n_write;
  }
  close(prev_pipe[0]);
  if (flag_has_created_next_pipe == 1) {
    close(next_pipe[1]);
  }
  // NOTE maybe call exit(0)?
}

int
main(int argc, char *argv[])
{
  if (argc == 2 && (argv[1][0] == '-' && argv[1][1] == 'h')) {
    print_help();
    exit(0);
  }

  /* implementation
     1. the first process would fork and feed numbers to the second process.
     2. the second process is the process that keeps 2 as the "p" variable.
  */
  CHECK_CALL(pipe(next_pipe));
  int pid;
  pid = fork();
  if (pid < 0) {
    fprintf(2, "primes: error in fork()\n");
    exit(1);
  }

  int p = 2;
  if (pid == 0) {
    // child
    // read from prev_pipe
    prev_pipe[0] = next_pipe[0];
    close(next_pipe[1]);
    // the child would use p = 2
    primes(p);
  } else {
    // parent
    // write numbers to the pipe
    // wait for the child
    close(next_pipe[0]);
    int N = 280;
    int n_write;
    for (int i = 2; i <= N; ++i) {
      n_write = write(next_pipe[1], &i, sizeof(i));
      // assert(n_write == sizeof(i));
      (void) n_write;
    }
    wait(0);
  }

  exit(0);
}
