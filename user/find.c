#include "kernel/types.h"

#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N 512

void
find(const char *path, const char *name)
{
  char buffer[N];
  char *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    // exit(1);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  // debug
  // printf("line %d\n", __LINE__);
  // printf("path = \"%s\"\n", path);

  switch (st.type) {
  case T_DEVICE:
  case T_FILE: {
    break;
  }
  case T_DIR: {
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buffer)) {
      fprintf(2, "find: path too long\n");
      break;
    }

    strcpy(buffer, path);
    p = buffer + strlen(buffer);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0) {
        continue;
      }
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
        // skip current dir, or parent dir.
        continue;
      }

      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if (strcmp(de.name, name) == 0) {
        fprintf(1, "%s\n", buffer);
      }

      find(buffer, name);
    }
  }
  }
  // NOTE remember to close the fd.
  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    const char *usage_str = "usage: find directory some_name";
    fprintf(2, usage_str);
    exit(1);
  }

  // TODO-MORE should ensure "path" is a directory.
  const char *path = argv[1];
  const char *name = argv[2];
  if (strcmp(path, name) == 0) {
    fprintf(1, "%s\n", path);
  }

  find(path, name);

  exit(0);
}
