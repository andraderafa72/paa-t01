#ifndef BRANCH_AND_BOUND_H
#define BRANCH_AND_BOUND_H

#include "cost.h"

namespace warehouse {

struct BoundProgress {
  long long nodes = 0;
  long long pruned = 0;
  double best_cost = 0;
  double elapsed_ms = 0;
  bool has_best = false;
};

using BoundProgressFn = void (*)(const BoundProgress& progress);

struct ExactResult {
  AllocationResult solution;
  double greedy_cost = 0;
  double local_cost = 0;
  double heuristic_cost = 0;
  double gap_greedy = 0;
  double gap_local = 0;
  double gap_heuristic = 0;
  long long nodes = 0;
  long long pruned = 0;
  bool optimal = false;
  bool certified = false;
};

ExactResult run_branch_and_bound(
  const Instance& instance,
  BoundProgressFn on_progress = nullptr,
  int time_limit_seconds = 0);

bool branch_and_bound_self_check();

}

#endif
