#include "kernel/types.h"

#include "kernel/param.h"
#include "user/user.h"

#define MAX_LINE 256

void print_help();
// return line length.
// the '\n' char is not included in the buffer.
int read_line(int fd, void *buffer, int size);

void
xargs(char *command, char *initial_arguments[], int length)
{
  // debug
  printf("command = \"%s\"\n", command);
  printf("length = %d\n", length);

  // the delimiter char
  char delim = ' ';
  char buffer[MAX_LINE];
  char *args[MAXARG];
  args[0] = command;
  int arg_count = 1;
  for (int i = 0; i < length && arg_count < MAXARG; ++i) {
    args[arg_count] = initial_arguments[i];
    ++arg_count;
  }

  int line_length = 0;
  // debug
  printf("line %d\n", __LINE__);

  while ((line_length = read_line(0, buffer, sizeof(buffer) - 1)) > 0) {
    buffer[line_length + 1] = '\0';
    // delimit each argument
    char *slow = buffer;
    char *fast = buffer;
    for (; fast - buffer < line_length; ++fast) {
      if (*fast == delim) {
        // delimit an argument
        fast = '\0';
        args[arg_count] = slow;
        ++arg_count;
        if (arg_count >= MAXARG) {
          // too manf args, for xv6 user program arg list
          fprintf(2, "xargs: too many args in a line\n");
          // just exit for now
          exit(1);
        }
        slow = fast + 1;
      }
    }
    // the final argument
    args[arg_count] = slow;
    ++arg_count;
    if (arg_count >= MAXARG) {
      fprintf(2, "xargs: too many args in a line\n");
      exit(1);
    }

    // set null pointer at the end of arg list
    args[arg_count] = 0;
    // debug
    for (int i = 0; i < arg_count; ++i) {
      printf("args[%d] = \"%s\"\n", i, args[i]);
    }

    int pid;
    pid = fork();
    if (pid < 0) {
      fprintf(2, "xargs: error in fork()\n");
      exit(1);
    }
    if (pid == 0) {
      // child
      exec(command, args);
    } else {
      // parent
      wait(0);
    }
  }
}

void
print_help()
{
  const char *usage_str = "usage: xargs command [initial-argument ...]";
  fprintf(1, usage_str);
}

int
read_line(int fd, void *buffer, int size)
{
  int i = 0;
  int n_read;
  char *p = buffer;
  for (; i < size;) {
    // read one char at a time.
    n_read = read(fd, p, 1);
    if (n_read == 0) {
      // end of file
      break;
    }
    if (n_read < 0) {
      // error
      fprintf(2, "error in read()\n");
      exit(1);
    }

    if (*p == '\n') {
      // get a full line
      *p = '\0';
      break;
    }
    i += n_read;
    ++p;
  }
  // debug
  printf("buffer (length = %d): \"%s\"\n", i, (char *) buffer);
  return i;
}

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    print_help();
    exit(1);
  }

  char *command = argv[1];
  char **initial_arguments = &argv[2];
  int length = argc - 2;
  xargs(command, initial_arguments, length);

  exit(0);
}
