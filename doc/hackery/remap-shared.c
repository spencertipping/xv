#define _GNU_SOURCE

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main() {
  // Create some initialized memory...
  void *const p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                                   MAP_ANONYMOUS | MAP_SHARED, -1, 0);
  if (errno) {
    printf("got %d from mmap\n", errno);
    return 1;
  }
  memset(p, 7, 4096);

  // ... then fork the process. The fork will retain the mapping.
  pid_t child;
  if (child = fork()) {
    // Reset memory to all zeroes, but only after the remap has happened.
    printf("sleeping before resetting...\n");
    sleep(2);
    printf("resetting...\n");
    memset(p, 0, 4096);

    int status;
    waitpid(child, &status, 0);
    if (status) printf("child exited with %d\n", status);
  } else {
    // Remap the region to a different address.
    printf("child is remapping now...\n");
    void *const q = mremap(p, 4096, 4096,
                           MREMAP_MAYMOVE | MREMAP_FIXED, p + 4096);
    if (errno) {
      printf("mremap failed with %d\n", errno);
      return 1;
    }

    printf("child has remapped\n");

    // Now wait for the parent process to change its contents.
    while (*((int*) q)) {
      printf("child waiting for change...\n");
      sleep(1);
    }
    return 0;
  }
}
