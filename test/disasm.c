#include "../build/xv.h"
#include "../build/xv-x64.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Reads instructions from the given flat file, printing each opcode */
int main(int const argc, char const *const *const argv) {
  int const fd = open(argv[1], O_RDONLY);
  if (fd == -1) return 1;

  struct stat s;
  if (fstat(fd, &s)) return 2;

  xv_x64_const_ibuffer buf;
  buf.current = buf.start
              = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

  xv_x64_ibuffer rewritten = { .start = 0 };
  xv_x64_reallocate_ibuffer(&rewritten, s.st_size);

  buf.logical_start = 0;
  buf.capacity = s.st_size;
  if (buf.start == (void *const) -1) return 3;

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

    printf("instruction length: %d\n", xv_x64_write_insn(NULL, &insn));
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

  munmap((void*) buf.start, buf.capacity);
  munmap(rewritten.start, rewritten.capacity);
  close(fd);
  return 0;
}
