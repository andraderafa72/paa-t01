#include "cost.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace warehouse {

namespace {

constexpr double kImprovementEpsilon = 1e-9;

}

double total_cost(const Cost& cost, double alpha, double beta) {
  return alpha * cost.access + beta * cost.separation;
}

Cost compute_cost(const Instance& instance, const std::vector<int>& slot_of_product) {
  Cost cost;
  const int product_count = static_cast<int>(instance.products.size());

  for (int product = 0; product < product_count; ++product) {
    const int slot = slot_of_product[product];
    if (slot < 0) {
      continue;
    }
    cost.access += instance.products[product].frequency * instance.slots[slot].access_cost;
  }

  for (int first = 0; first < product_count; ++first) {
    if (slot_of_product[first] < 0) {
      continue;
    }
    for (int second = first + 1; second < product_count; ++second) {
      if (slot_of_product[second] < 0) {
        continue;
      }
      const double together = instance.cooccurrence[first][second];
      const double apart = instance.distance[slot_of_product[first]][slot_of_product[second]];
      cost.separation += together * apart;
    }
  }

  return cost;
}

AllocationResult build_result(
    const Instance& instance,
    std::vector<int> slot_of_product,
    const std::string& algorithm,
    int moves_evaluated,
    double elapsed_ms) {
  AllocationResult result;
  result.algorithm = algorithm;
  result.moves_evaluated = moves_evaluated;
  result.elapsed_ms = elapsed_ms;
  result.slot_of_product = std::move(slot_of_product);
  result.cost = compute_cost(instance, result.slot_of_product);
  result.total_cost = total_cost(result.cost, instance.alpha, instance.beta);

  int placed = 0;
  double used_capacity = 0;
  double used_volume = 0;
  const int product_count = static_cast<int>(instance.products.size());

  for (int product_index = 0; product_index < product_count; ++product_index) {
    const Product& product = instance.products[product_index];
    Placement placement;
    placement.product_id = product.id;
    placement.product_name = product.name;
    placement.volume = product.volume;
    placement.frequency = product.frequency;

    const int slot_index = result.slot_of_product[product_index];
    if (slot_index >= 0) {
      const Slot& slot = instance.slots[slot_index];
      placement.slot_id = slot.id;
      placement.capacity = slot.capacity;
      used_capacity += slot.capacity;
      used_volume += product.volume;
      ++placed;
    }
    result.placements.push_back(placement);
  }

  std::sort(result.placements.begin(), result.placements.end(), [](const Placement& left, const Placement& right) {
    if ((left.slot_id < 0) != (right.slot_id < 0)) {
      return left.slot_id >= 0;
    }
    if (left.slot_id != right.slot_id) {
      return left.slot_id < right.slot_id;
    }
    return left.product_id < right.product_id;
  });

  result.valid = placed == product_count;
  result.occupancy = used_capacity > 0 ? (used_volume / used_capacity) * 100.0 : 0;
  result.validity = result.valid ? "válida" : "há produto sem posição com capacidade livre";
  result.summary = std::to_string(placed) + " de " + std::to_string(product_count) + " produtos posicionados.";
  return result;
}

bool swap_is_feasible(const Instance& instance, const std::vector<int>& slot_of_product, int first, int second) {
  const int first_slot = slot_of_product[first];
  const int second_slot = slot_of_product[second];
  if (first_slot < 0 || second_slot < 0) {
    return false;
  }
  return instance.products[first].volume <= instance.slots[second_slot].capacity
    && instance.products[second].volume <= instance.slots[first_slot].capacity;
}

double full_swap_delta(
    const Instance& instance,
    std::vector<int>& slot_of_product,
    int first,
    int second,
    double current_total) {
  std::swap(slot_of_product[first], slot_of_product[second]);
  const double after = total_cost(compute_cost(instance, slot_of_product), instance.alpha, instance.beta);
  std::swap(slot_of_product[first], slot_of_product[second]);
  return after - current_total;
}

double incremental_swap_delta(
    const Instance& instance,
    const std::vector<int>& slot_of_product,
    int first,
    int second) {
  const int slot_first = slot_of_product[first];
  const int slot_second = slot_of_product[second];
  const double access = (instance.products[first].frequency - instance.products[second].frequency)
    * (instance.slots[slot_second].access_cost - instance.slots[slot_first].access_cost);

  double separation = 0;
  const int product_count = static_cast<int>(instance.products.size());
  for (int other = 0; other < product_count; ++other) {
    if (other == first || other == second || slot_of_product[other] < 0) {
      continue;
    }
    const int slot_other = slot_of_product[other];
    separation += instance.cooccurrence[first][other]
      * (instance.distance[slot_second][slot_other] - instance.distance[slot_first][slot_other]);
    separation += instance.cooccurrence[second][other]
      * (instance.distance[slot_first][slot_other] - instance.distance[slot_second][slot_other]);
  }

  return instance.alpha * access + instance.beta * separation;
}

SwapSearch improve_by_swaps(
    const Instance& instance,
    std::vector<int> slot_of_product,
    int moves_evaluated,
    SwapEvaluation evaluation,
    SearchProgressFn on_progress) {
  SwapSearch search;
  search.slot_of_product = std::move(slot_of_product);
  search.moves_evaluated = moves_evaluated;
  const int product_count = static_cast<int>(instance.products.size());
  const auto started = std::chrono::steady_clock::now();

  bool improved = true;
  int pass = 0;
  while (improved) {
    improved = false;
    double best_delta = 0;
    int best_first = -1;
    int best_second = -1;
    const double current_total = total_cost(compute_cost(instance, search.slot_of_product), instance.alpha, instance.beta);

    for (int first = 0; first < product_count; ++first) {
      if (search.slot_of_product[first] < 0) {
        continue;
      }
      for (int second = first + 1; second < product_count; ++second) {
        if (search.slot_of_product[second] < 0) {
          continue;
        }
        ++search.moves_evaluated;
        if (!swap_is_feasible(instance, search.slot_of_product, first, second)) {
          continue;
        }

        const double delta = evaluation == SwapEvaluation::Full
          ? full_swap_delta(instance, search.slot_of_product, first, second, current_total)
          : incremental_swap_delta(instance, search.slot_of_product, first, second);
        if (delta < -kImprovementEpsilon && delta < best_delta) {
          best_delta = delta;
          best_first = first;
          best_second = second;
        }
      }
    }

    if (best_first >= 0) {
      std::swap(search.slot_of_product[best_first], search.slot_of_product[best_second]);
      ++search.accepted_swaps;
      improved = true;
    }

    ++pass;
    if (on_progress != nullptr) {
      const auto now = std::chrono::steady_clock::now();
      SearchProgress progress;
      progress.pass = pass;
      progress.accepted_swaps = search.accepted_swaps;
      progress.moves_evaluated = search.moves_evaluated;
      progress.elapsed_ms = std::chrono::duration<double, std::milli>(now - started).count();
      progress.best_delta = best_delta;
      on_progress(progress);
    }
  }

  return search;
}

}
