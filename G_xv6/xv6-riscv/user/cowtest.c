#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static char cow_page1[4096] __attribute__((aligned(4096)));
static char cow_page2[4096] __attribute__((aligned(4096)));
static char cow_page3[4096] __attribute__((aligned(4096)));

void
test_simple_cow(void)
{
  printf("test simple_cow: ");
  cow_page1[0] = 'A';
  cow_page1[100] = 'X';

  int pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // Child: check shared value
    if (cow_page1[0] != 'A' || cow_page1[100] != 'X') {
      printf("FAIL: child initial value mismatch\n");
      exit(1);
    }
    // Child writes to page (triggers COW store fault)
    cow_page1[0] = 'B';
    cow_page1[100] = 'Y';
    if (cow_page1[0] != 'B' || cow_page1[100] != 'Y') {
      printf("FAIL: child write failed\n");
      exit(1);
    }
    exit(0);
  }

  wait(0);
  // Parent verifies its memory remains untouched
  if (cow_page1[0] != 'A' || cow_page1[100] != 'X') {
    printf("FAIL: parent memory corrupted by child write\n");
    exit(1);
  }
  printf("OK\n");
}

void
test_copyout_cow(void)
{
  printf("test copyout_cow (syscall write to COW page): ");
  cow_page2[0] = 'Z';

  int p[2];
  if (pipe(p) < 0) {
    printf("pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    close(p[1]);
    // Kernel copies into COW page via copyout during read()
    int n = read(p[0], cow_page2, 4);
    if (n != 4 || cow_page2[0] != 'H' || cow_page2[1] != 'E') {
      printf("FAIL: child read into COW page failed\n");
      exit(1);
    }
    close(p[0]);
    exit(0);
  }

  close(p[0]);
  write(p[1], "HELP", 4);
  close(p[1]);
  wait(0);

  if (cow_page2[0] != 'Z') {
    printf("FAIL: parent page modified by child copyout\n");
    exit(1);
  }
  printf("OK\n");
}

void
test_multi_fork(void)
{
  printf("test multi_fork (cascading COW sharing): ");
  cow_page3[0] = '0';

  for (int i = 1; i <= 4; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      // Child modifies its private copy
      cow_page3[0] = '0' + i;
      if (cow_page3[0] != '0' + i) {
        printf("FAIL: child %d value incorrect\n", i);
        exit(1);
      }
      exit(0);
    }
  }

  for (int i = 1; i <= 4; i++) {
    wait(0);
  }

  if (cow_page3[0] != '0') {
    printf("FAIL: parent value corrupted in multi-fork\n");
    exit(1);
  }
  printf("OK\n");
}

int
main(int argc, char *argv[])
{
  printf("=========================================================\n");
  printf(" Running Copy-on-Write (COW) Fork Diagnostics            \n");
  printf("=========================================================\n");

  test_simple_cow();
  test_copyout_cow();
  test_multi_fork();

  printf("=========================================================\n");
  printf(" ALL COW TESTS PASSED!\n");
  printf("=========================================================\n");
  exit(0);
}
