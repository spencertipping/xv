#include "../build/xv-x64.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Reads instructions from the given flat file, printing each opcode */
int main(int const argc, char const *const *const argv) {
  if (argc != 3) {
    printf("usage: %s infile outfile\n", argv[0]);
    return 1;
  }

  int const fd  = open(argv[1], O_RDONLY);
  int const wfd = open(argv[2], O_RDWR);

  if (fd == -1 || wfd == -1) {
    printf("failed to open either input or output file (do both exist?)\n");
    return 1;
  }

  struct stat s;
  if (fstat(fd, &s)) return 2;

  unsigned const size = s.st_size + 4095 & ~4095;

  xv_x64_const_ibuffer buf;
  buf.current = buf.start
              = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

  buf.logical_start = 0;
  buf.capacity = s.st_size;
  if (buf.start == (void *const) -1) return 3;

  xv_x64_ibuffer rewritten;
  rewritten.current = rewritten.start
                    = mmap(NULL, size, PROT_READ | PROT_WRITE,
                           MAP_SHARED, wfd, 0);

  rewritten.logical_start = 0;
  rewritten.capacity = s.st_size;
  if (rewritten.start == (void *const) -1) return 4;

  printf("initialized buffer; printing bytes...\n");
  for (int i = 0; i < buf.capacity; ++i)
    i & 0x0f || printf("\n%04x: ", i),
    printf("%02x ", buf.start[i]);

  printf("\n");

  xv_x64_insn insn;
  int         status;
  int         write_status;
  char        buffer[128];
  while (!(status = xv_x64_read_insn(&buf, &insn))) {
    if (xv_x64_print_insn(buffer, sizeof(buffer), &insn))
      printf("%s\n", buffer);

    if (write_status = xv_x64_write_insn(&rewritten, &insn)) {
      printf("got %d\n", write_status);
      break;
    }
  }

  printf("after loop\n");
  if (xv_x64_print_insn(buffer, sizeof(buffer), &insn))
    printf("%s\n", buffer);

  printf("last status code: %d\n", status);
  printf("last write status code: %d\n", write_status);

  printf("printing bytes...\n");
  for (int i = 0; i < buf.capacity; ++i)
    i & 0x07 || printf("\n%04x: ", i),
    printf("%02x\033[%s%02x\033[0;0m ",
           buf.start[i],
           buf.start[i] == rewritten.start[i] ? "0;32m" : "1;31m",
           rewritten.start[i]);

  munmap((void*) buf.start, size);
  munmap(rewritten.start, size);
  close(fd);
  return 0;
}
