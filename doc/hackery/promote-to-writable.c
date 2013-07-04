#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

int main() {
  int   fd = open("/bin/ls", O_RDONLY);
  void *m1 = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
  void *m2 = mmap(NULL, 4096, PROT_READ, MAP_SHARED,  fd, 0);

  if (mprotect(m1, 4096, PROT_READ | PROT_WRITE))
    printf("mprotect of MAP_PRIVATE failed: %s\n",
           errno == EACCES ? "EACCES" :
           errno == EINVAL ? "EINVAL" :
           errno == ENOMEM ? "ENOMEM" : "unknown");

  if (mprotect(m2, 4096, PROT_READ | PROT_WRITE))
    printf("mprotect of MAP_SHARED failed: %s\n",
           errno == EACCES ? "EACCES" :
           errno == EINVAL ? "EINVAL" :
           errno == ENOMEM ? "ENOMEM" : "unknown");
  return 0;
}
