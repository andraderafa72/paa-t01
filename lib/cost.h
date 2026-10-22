#ifndef COST_H
#define COST_H

#include "instance.h"

#include <string>
#include <vector>

namespace warehouse {

struct Cost {
  double access = 0;
  double separation = 0;
};

struct Placement {
  int slot_id = -1;
  int product_id = 0;
  std::string product_name;
  double volume = 0;
  double capacity = 0;
  double frequency = 0;
};

struct AllocationResult {
  std::string algorithm;
  bool valid = false;
  std::string validity = "não avaliada";
  Cost cost{};
  double total_cost = 0;
  double elapsed_ms = 0;
  int moves_evaluated = 0;
  double occupancy = 0;
  std::string summary;
  std::vector<Placement> placements;
  std::vector<int> slot_of_product;
};

enum class SwapEvaluation {
  Full,
  Incremental,
};

struct SwapSearch {
  std::vector<int> slot_of_product;
  int moves_evaluated = 0;
  int accepted_swaps = 0;
};

struct SearchProgress {
  int pass = 0;
  int accepted_swaps = 0;
  int moves_evaluated = 0;
  double elapsed_ms = 0;
  double best_delta = 0;
};

using SearchProgressFn = void (*)(const SearchProgress& progress);

double total_cost(const Cost& cost, double alpha, double beta);

Cost compute_cost(const Instance& instance, const std::vector<int>& slot_of_product);

AllocationResult build_result(
  const Instance& instance,
  std::vector<int> slot_of_product,
  const std::string& algorithm,
  int moves_evaluated,
  double elapsed_ms);

bool swap_is_feasible(const Instance& instance, const std::vector<int>& slot_of_product, int first, int second);

double full_swap_delta(
  const Instance& instance,
  std::vector<int>& slot_of_product,
  int first,
  int second,
  double current_total);

double incremental_swap_delta(
  const Instance& instance,
  const std::vector<int>& slot_of_product,
  int first,
  int second);

SwapSearch improve_by_swaps(
  const Instance& instance,
  std::vector<int> slot_of_product,
  int moves_evaluated,
  SwapEvaluation evaluation,
  SearchProgressFn on_progress = nullptr);

}

#endif
