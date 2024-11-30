#include "kernel/types.h"

#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N 512

void
find(const char *path, const char *name) {
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

  if (strcmp(path, name) == 0) {
    fprintf(1, "%s\n", path);
  }

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
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;

      find(buffer, name);
    }

    }
  }
}

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    const char *usage_str = "usage: find <dir> <some-name>";
    fprintf(2, usage_str);
    exit(1);
  }

  find(argv[1], argv[2]);

  exit(0);
}
