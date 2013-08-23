#define _GNU_SOURCE

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

unsigned long long contains_illegal(void) {
  // This will be rewritten by main()
  return 0x7777777777777777ull;
}

unsigned long long contains_normal_syscall(void) {
  int fd = open("/dev/null", O_RDONLY);
  if (fd != -1) close(fd);
  return 3;
}

void sigill_action(int sig, siginfo_t *info, void *ucontext) {
  // Fix up the context by bumping the instruction pointer. This signal handler
  // is not re-entrant, but it doesn't need to be because we won't encounter
  // illegal instructions here.
  ucontext_t *context = (ucontext_t*) ucontext;
  context->uc_mcontext.gregs[REG_RIP] += 2;

  contains_normal_syscall();    // for benchmarking
}

int main(void) {
  // Looking for REX.W prefix followed by b8 instruction (move 64-bit immediate
  // into %rax)
  unsigned char *code_address = (unsigned char*) &contains_illegal;
  int count = 0;
  for (; !(code_address[0] == 0x48 && code_address[1] == 0xb8) && count < 20;
         ++code_address, ++count);

  if (count >= 20) {
    printf("hit loop limit searching for 48 b8\n");
    return 1;
  }

  // Re-protect the page so we can modify it.
  if (mprotect((unsigned long long) code_address & ~4095, 4096, 7)) {
    printf("mprotect returned %d; code_address = %llx\n", errno, code_address);
    return 1;
  }

  // Now rewrite that instruction into the illegal "lea %eax, %eax" and provide
  // an alternative return path.
  code_address[0] = 0x8d;       // lea
  code_address[1] = 0xc0;       // %eax, %eax
  code_address[2] = 0x48;       // rex.w
  code_address[3] = 0x31;       // xor
  code_address[4] = 0xc0;       // %rax, %rax
  code_address[5] = 0xc9;       // leave
  code_address[6] = 0xc3;       // ret

  // At this point contains_illegal should contain an illegal instruction that
  // will trigger a SIGILL. If we ignore the signal and jump to the point after
  // the illegal instruction, contains_illegal should return 0.
  struct sigaction new_signal = {
    .sa_handler   = NULL,
    .sa_sigaction = &sigill_action,
    .sa_mask      = 0,
    .sa_flags     = SA_SIGINFO,
    .sa_restorer  = NULL
  };

  if (sigaction(SIGILL, &new_signal, NULL)) {
    printf("sigaction returned %d\n", errno);
    return 1;
  }

  unsigned long long result;
  result = contains_normal_syscall();
  printf("contains_normal_syscall() = %lld\n", result);

  result = contains_illegal();
  printf("contains_illegal() = %lld\n", result);

  // Now time each one to figure out how much overhead is associated with
  // catching illegal signals.
  struct timeval start_normal_syscall;
  struct timeval end_normal_syscall;

  struct timeval start_illegal;
  struct timeval end_illegal;

  const int iterations = 1000000;

  gettimeofday(&start_normal_syscall, NULL);
  for (int i = 0; i < iterations; ++i) contains_normal_syscall();
  gettimeofday(&end_normal_syscall, NULL);

  printf("normal syscall took %ld microseconds\n",
         (end_normal_syscall.tv_sec - start_normal_syscall.tv_sec) * 1000000 +
         end_normal_syscall.tv_usec - start_normal_syscall.tv_usec);

  gettimeofday(&start_illegal, NULL);
  for (int i = 0; i < iterations; ++i) contains_illegal();
  gettimeofday(&end_illegal, NULL);

  printf("illegal took %ld microseconds\n",
         (end_illegal.tv_sec - start_illegal.tv_sec) * 1000000 +
         end_illegal.tv_usec - start_illegal.tv_usec);

  return 0;
}
