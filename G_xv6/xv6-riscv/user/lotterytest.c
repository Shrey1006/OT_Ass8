#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int tickets[3] = {10, 20, 30};
  int pipes[3][2];

  printf("lotterytest: testing lottery scheduler with tickets [10, 20, 30]\n");

  for (int i = 0; i < 3; i++) {
    if (pipe(pipes[i]) < 0) {
      printf("pipe failed\n");
      exit(1);
    }
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      // Child process
      close(pipes[i][0]);
      settickets(tickets[i]);

      // Align to tick start
      uint start = uptime();
      while (uptime() == start)
        ;
      start = uptime();

      int count = 0;
      // Spin for 30 ticks
      while (uptime() - start < 30) {
        count++;
      }

      write(pipes[i][1], &count, sizeof(count));
      close(pipes[i][1]);
      exit(0);
    } else {
      close(pipes[i][1]);
    }
  }

  int counts[3] = {0, 0, 0};
  int total = 0;
  for (int i = 0; i < 3; i++) {
    read(pipes[i][0], &counts[i], sizeof(int));
    close(pipes[i][0]);
    wait(0);
    total += counts[i];
  }

  printf("\n=== Lottery Scheduling Benchmark Results ===\n");
  for (int i = 0; i < 3; i++) {
    int pct = (total > 0) ? (counts[i] * 100) / total : 0;
    printf("Process %d (Tickets: %d): %d loops (%d%% of total)\n",
           i + 1, tickets[i], counts[i], pct);
  }
  printf("Theoretical Expected Ratios: 10/60 (16.7%%), 20/60 (33.3%%), 30/60 (50.0%%)\n");
  exit(0);
}
