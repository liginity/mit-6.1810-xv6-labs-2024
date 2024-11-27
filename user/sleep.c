#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(2, "sleep: need to pass time duration as an argument\n");
    exit(1);
  }

  // NOTE if the string is not a valid decimal number, them atoi() returns 0.
  int dt = atoi(argv[1]);
  sleep(dt);
}
