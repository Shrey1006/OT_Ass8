#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPROCS 3
#define CPU_ITERS 25000000

struct bench_res {
  int id;
  int tickets;
  int tt; // Turnaround time in ticks
};

// Integer Jain's Fairness Index calculation in percentage (0 to 100%)
// J = (sum(x)^2) * 100 / (n * sum(x^2))
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
  printf(" [BENCHMARK] CPU-Bound Workload (Tickets: 10, 20, 30)    \n");
  printf(" Workload: %d arithmetic iterations per worker        \n", CPU_ITERS);
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
      close(sync_pipe[1]); // Close write end
      close(pipes[i][0]);  // Close read end of result pipe

      settickets(tickets[i]);

      // Wait for synchronized start signal
      char go;
      read(sync_pipe[0], &go, 1);
      close(sync_pipe[0]);

      uint t_start = uptime();

      // CPU-intensive computation (LCG hash sequence)
      volatile int val = 1;
      for (int it = 0; it < CPU_ITERS; it++) {
        val = (val * 1103515245 + 12345) & 0x7fffffff;
      }

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

  // Close read end of sync pipe in parent, then broadcast start by writing
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
    // Throughput rate R_i = 1000 / TT_i
    raw_rates[i] = 1000 / results[i].tt;
    // Normalized rate x_i = 10000 / (tickets * TT_i)
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
