#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define NPROCS 3
#define MIXED_ITERS 30
#define CPU_BURST_ITERS 600000
#define BLOCK_SIZE 256

struct bench_res {
  int id;
  int tickets;
  int tt; // Turnaround time in ticks
};

static int
calc_jain_pct(int vals[], int n)
{
  long long sum = 0;
  long long sum_sq = 0;
  for (int i = 0; i < n; i++) {
    sum += vals[i];
    sum_sq += (long long)vals[i] * vals[i];
  }
  if (sum_sq == 0)
    return 0;
  long long num = sum * sum * 100;
  long long den = (long long)n * sum_sq;
  return (int)(num / den);
}

int
main(int argc, char *argv[])
{
  int tickets[NPROCS] = {10, 20, 30};
  int pipes[NPROCS][2];
  int sync_pipe[2];

  printf("=========================================================\n");
  printf(" [BENCHMARK] Mixed CPU/IO Workload (Tickets: 10, 20, 30) \n");
  printf(" Workload: %d interleaved compute (%d iters) & disk I/O  \n",
         MIXED_ITERS, CPU_BURST_ITERS);
  printf("=========================================================\n");

  if (pipe(sync_pipe) < 0) {
    printf("pipe failed\n");
    exit(1);
  }

  for (int i = 0; i < NPROCS; i++) {
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
      close(sync_pipe[1]);
      close(pipes[i][0]);

      settickets(tickets[i]);

      char fname[16] = "bmix_x.dat";
      fname[5] = '0' + i;

      char buffer[BLOCK_SIZE];
      memset(buffer, 'M' + i, sizeof(buffer));

      // Wait for synchronized start
      char go;
      read(sync_pipe[0], &go, 1);
      close(sync_pipe[0]);

      uint t_start = uptime();

      volatile int acc = 7;
      for (int it = 0; it < MIXED_ITERS; it++) {
        // Phase 1: CPU burst
        for (int c = 0; c < CPU_BURST_ITERS; c++) {
          acc = (acc * 31 + 17) ^ c;
        }

        // Phase 2: Disk I/O burst
        int fd = open(fname, O_CREATE | O_RDWR);
        if (fd >= 0) {
          write(fd, buffer, BLOCK_SIZE);
          close(fd);
        }
        fd = open(fname, O_RDONLY);
        if (fd >= 0) {
          read(fd, buffer, BLOCK_SIZE);
          close(fd);
        }
      }
      unlink(fname);

      uint t_end = uptime();
      int tt = (int)(t_end - t_start);
      if (tt <= 0)
        tt = 1;

      struct bench_res res;
      res.id = i + 1;
      res.tickets = tickets[i];
      res.tt = tt;

      write(pipes[i][1], &res, sizeof(res));
      close(pipes[i][1]);
      exit(0);
    } else {
      // Parent
      close(pipes[i][1]);
    }
  }

  close(sync_pipe[0]);
  char signal = 'G';
  for (int i = 0; i < NPROCS; i++) {
    write(sync_pipe[1], &signal, 1);
  }
  close(sync_pipe[1]);

  struct bench_res results[NPROCS];
  for (int i = 0; i < NPROCS; i++) {
    read(pipes[i][0], &results[i], sizeof(struct bench_res));
    close(pipes[i][0]);
    wait(0);
  }

  printf("\nWorker Results:\n");
  printf("---------------------------------------------------------\n");

  int raw_rates[NPROCS];
  int weighted_rates[NPROCS];
  for (int i = 0; i < NPROCS; i++) {
    raw_rates[i] = 1000 / results[i].tt;
    weighted_rates[i] = 10000 / (results[i].tickets * results[i].tt);

    printf("Worker %d: tickets=%d, turnaround=%d ticks, norm_rate=%d\n",
           results[i].id, results[i].tickets, results[i].tt, weighted_rates[i]);
  }

  int jain_raw = calc_jain_pct(raw_rates, NPROCS);
  int jain_weighted = calc_jain_pct(weighted_rates, NPROCS);

  printf("---------------------------------------------------------\n");
  printf("Turnaround Fairness (Equal Share J):         %d%%\n", jain_raw);
  printf("Proportional Fairness (Ticket-Weighted J):   %d%%\n", jain_weighted);
  printf("=========================================================\n\n");

  exit(0);
}
