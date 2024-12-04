#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  const int number_of_pages = 32;
  const int offset = 32;
  char *end = sbrk(PGSIZE * number_of_pages);
  for (int i = 0; i < number_of_pages; ++i) {
    char *p = end + i * PGSIZE;
    p[offset] = '\0';
    fprintf(1, "at page %d (address %p), the string is \"%s\"\n", i, p, p);
  }
  exit(1);
}
