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
  const int secret_size = 8;

  char *end = sbrk(PGSIZE * number_of_pages);
  // for (int i = -1; i < number_of_pages; ++i) {
  //   char *p = end + i * PGSIZE;
  //   // p[offset] = '\0';
  //   fprintf(1, "at page %d (address %p), the string is \"%s\"\n", i, p, p);
  //   fprintf(1, "           (address %p), \"%s\"\n", p + offset, p + offset);

  //   fprintf(1, "           content: \"");
  //   for (int i = 0; i < offset; ++i) {
  //     // printf("%c", page_start[i]);
  //     // putc(page_start[i]);
  //     write(1, p + i, 1);
  //   }
  //   printf("\"\n");
  // }

  const char *page_start = end + 16 * PGSIZE;
  printf("content: \"");
  for (int i = 0; i < offset; ++i) {
    // printf("%c", page_start[i]);
    // putc(page_start[i]);
    write(1, page_start + i, 1);
  }
  printf("\"\n");

  // from my test, the secret sits on the 16th page.
  const char *secret = end + 16 * PGSIZE + offset;
  // fprintf(2, "%s", secret);
  write(2, secret, secret_size);
  exit(1);
}
