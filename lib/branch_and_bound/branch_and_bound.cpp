#include "branch_and_bound.h"

#include "assignment.h"
#include "greedy/greedy.h"
#include "improved/improved.h"
#include "local_search/local_search.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace warehouse {
namespace {

constexpr double kEps = 1e-9;
constexpr double kInf = 1e100;

double gap_percent(double cost, double opt) {
  if (std::abs(opt) <= kEps) {
    return 0;
  }
  return (cost - opt) / opt * 100.0;
}

struct Search {
  const Instance* instance = nullptr;
  BoundProgressFn on_progress = nullptr;
  std::vector<int> order;
  std::vector<int> slot_order;
  std::vector<int> slot_of;
  std::vector<char> used;
  double partial_access = 0;
  double partial_separation = 0;
  double best_cost = kInf;
  std::vector<int> best;
  long long nodes = 0;
  long long pruned = 0;
  bool found = false;
  bool timed_out = false;
  std::chrono::steady_clock::time_point started;
  std::chrono::steady_clock::time_point deadline;
  std::chrono::steady_clock::time_point last_report;

  double min_access(int depth) {
    const int product_count = static_cast<int>(order.size());
    if (depth >= product_count) {
      return 0;
    }
    std::vector<int> products;
    products.reserve(product_count - depth);
    for (int index = depth; index < product_count; ++index) {
      products.push_back(order[index]);
    }
    std::vector<int> free_slots;
    const int slot_count = static_cast<int>(instance->slots.size());
    for (int slot = 0; slot < slot_count; ++slot) {
      if (!used[slot]) {
        free_slots.push_back(slot);
      }
    }
    return min_remaining_access(*instance, products, free_slots);
  }

  void report(double elapsed_ms) {
    if (on_progress == nullptr) {
      return;
    }
    BoundProgress progress;
    progress.nodes = nodes;
    progress.pruned = pruned;
    progress.best_cost = best_cost;
    progress.elapsed_ms = elapsed_ms;
    progress.has_best = found;
    on_progress(progress);
  }

  void dfs(int depth) {
    if (timed_out) {
      return;
    }
    ++nodes;
    if ((nodes & 255) == 0) {
      const auto now = std::chrono::steady_clock::now();
      if (now >= deadline) {
        timed_out = true;
        return;
      }
      if (now - last_report >= std::chrono::seconds(2)) {
        last_report = now;
        report(std::chrono::duration<double, std::milli>(now - started).count());
      }
    }

    const int product_count = static_cast<int>(order.size());
    if (depth == product_count) {
      const double total = total_cost(compute_cost(*instance, slot_of), instance->alpha, instance->beta);
      if (total < best_cost - kEps) {
        best_cost = total;
        best = slot_of;
        found = true;
      }
      return;
    }

    const double lb = instance->alpha * (partial_access + min_access(depth))
      + instance->beta * partial_separation;
    if (lb >= best_cost - kEps) {
      ++pruned;
      return;
    }

    const int product = order[depth];
    const double volume = instance->products[product].volume;
    const double frequency = instance->products[product].frequency;
    for (int slot : slot_order) {
      if (used[slot] || instance->slots[slot].capacity < volume) {
        continue;
      }
      double add_separation = 0;
      for (int placed = 0; placed < depth; ++placed) {
        const int other = order[placed];
        add_separation += instance->cooccurrence[product][other]
          * instance->distance[slot][slot_of[other]];
      }
      const double add_access = frequency * instance->slots[slot].access_cost;
      used[slot] = 1;
      slot_of[product] = slot;
      partial_access += add_access;
      partial_separation += add_separation;
      dfs(depth + 1);
      partial_separation -= add_separation;
      partial_access -= add_access;
      slot_of[product] = -1;
      used[slot] = 0;
      if (timed_out) {
        return;
      }
    }
  }
};

double enumerate_opt(const Instance& instance) {
  const int product_count = static_cast<int>(instance.products.size());
  const int slot_count = static_cast<int>(instance.slots.size());
  std::vector<int> slot_of(product_count, -1);
  std::vector<char> used(slot_count, 0);
  double best = kInf;

  const auto visit = [&](auto&& self, int product) -> void {
    if (product == product_count) {
      const double total = total_cost(compute_cost(instance, slot_of), instance.alpha, instance.beta);
      best = std::min(best, total);
      return;
    }
    for (int slot = 0; slot < slot_count; ++slot) {
      if (used[slot] || instance.products[product].volume > instance.slots[slot].capacity) {
        continue;
      }
      used[slot] = 1;
      slot_of[product] = slot;
      self(self, product + 1);
      slot_of[product] = -1;
      used[slot] = 0;
    }
  };
  visit(visit, 0);
  return best;
}

bool fail_check(const std::string& reason) {
  std::cerr << "Checagem do Branch and Bound: " << reason << "\n";
  return false;
}

}

ExactResult run_branch_and_bound(const Instance& instance, BoundProgressFn on_progress, int time_limit_seconds) {
  const AllocationResult greedy = run_greedy(instance);
  const AllocationResult local = run_local_search(instance);
  const AllocationResult heuristic = run_improved(instance);

  const int product_count = static_cast<int>(instance.products.size());
  const int slot_count = static_cast<int>(instance.slots.size());

  Search search;
  search.instance = &instance;
  search.on_progress = on_progress;
  search.order.resize(product_count);
  search.slot_order.resize(slot_count);
  search.slot_of.assign(product_count, -1);
  search.used.assign(slot_count, 0);

  std::vector<double> together(product_count, 0);
  for (int first = 0; first < product_count; ++first) {
    for (int second = 0; second < product_count; ++second) {
      together[first] += instance.cooccurrence[first][second];
    }
    search.order[first] = first;
  }
  for (int slot = 0; slot < slot_count; ++slot) {
    search.slot_order[slot] = slot;
  }

  std::sort(search.order.begin(), search.order.end(), [&](int left, int right) {
    const Product& a = instance.products[left];
    const Product& b = instance.products[right];
    if (a.frequency != b.frequency) {
      return a.frequency > b.frequency;
    }
    if (together[left] != together[right]) {
      return together[left] > together[right];
    }
    return a.id < b.id;
  });
  std::sort(search.slot_order.begin(), search.slot_order.end(), [&](int left, int right) {
    const Slot& a = instance.slots[left];
    const Slot& b = instance.slots[right];
    if (a.access_cost != b.access_cost) {
      return a.access_cost < b.access_cost;
    }
    return a.id < b.id;
  });

  if (heuristic.valid) {
    search.best_cost = heuristic.total_cost;
    search.best = heuristic.slot_of_product;
    search.found = true;
  }

  search.started = std::chrono::steady_clock::now();
  // ponytail: segundos <= 0 não corta; o menu e --exato é que passam o teto
  search.deadline = time_limit_seconds > 0
    ? search.started + std::chrono::seconds(time_limit_seconds)
    : std::chrono::steady_clock::time_point::max();
  search.last_report = search.started;
  search.dfs(0);
  const auto finished = std::chrono::steady_clock::now();
  const double elapsed_ms = std::chrono::duration<double, std::milli>(finished - search.started).count();

  std::vector<int> chosen = search.found ? search.best : std::vector<int>(product_count, -1);
  const int moves = search.nodes > 2147483647LL ? 2147483647 : static_cast<int>(search.nodes);
  AllocationResult solution = build_result(
    instance, std::move(chosen), "Branch and Bound", moves, elapsed_ms);
  if (search.timed_out) {
    solution.summary += " Tempo esgotado; melhor incumbente, sem certificado.";
  } else if (solution.valid) {
    solution.summary += " Ótimo provado.";
  } else {
    solution.summary += " Busca encerrada sem alocação viável.";
  }
  solution.summary += " Nós: " + std::to_string(search.nodes)
    + ". Podas: " + std::to_string(search.pruned) + ".";

  ExactResult result;
  result.solution = std::move(solution);
  result.greedy_cost = greedy.total_cost;
  result.local_cost = local.total_cost;
  result.heuristic_cost = heuristic.total_cost;
  result.nodes = search.nodes;
  result.pruned = search.pruned;
  result.optimal = !search.timed_out && result.solution.valid;
  result.certified = result.optimal;
  if (result.solution.valid) {
    result.gap_greedy = gap_percent(greedy.total_cost, result.solution.total_cost);
    result.gap_local = gap_percent(local.total_cost, result.solution.total_cost);
    result.gap_heuristic = gap_percent(heuristic.total_cost, result.solution.total_cost);
  }
  return result;
}

bool branch_and_bound_self_check() {
  const double pair = hungarian({{4, 1}, {2, 3}});
  if (std::abs(pair - 3) > kEps) {
    return fail_check("atribuição 2x2 esperava 3.");
  }
  const double wide = hungarian({{5, 1, 9}, {8, 2, 4}});
  if (std::abs(wide - 5) > kEps) {
    return fail_check("atribuição 2x3 esperava 5.");
  }
  const double blocked = hungarian({{kAssignmentForbidden, 4}, {kAssignmentForbidden, kAssignmentForbidden}});
  if (blocked < kAssignmentInf * 0.5) {
    return fail_check("atribuição inviável deveria ser infinita.");
  }

  Instance toy;
  toy.name = "checagem";
  toy.loaded = true;
  toy.alpha = 1;
  toy.beta = 1;
  toy.products = {{1, "A", 5, 10}, {2, "B", 3, 9}, {3, "C", 2, 8}};
  toy.slots = {{1, 10, 1}, {2, 4, 2}, {3, 10, 3}};
  toy.distance = {{0, 10, 8}, {10, 0, 1}, {8, 1, 0}};
  toy.cooccurrence = {{0, 1, 4}, {1, 0, 1}, {4, 1, 0}};

  const double opt = enumerate_opt(toy);
  if (std::abs(opt - 77) > kEps) {
    return fail_check("enumeração esperava 77 e obteve " + std::to_string(opt) + ".");
  }

  Search root;
  root.instance = &toy;
  root.order = {0, 1, 2};
  root.slot_of.assign(3, -1);
  root.used.assign(3, 0);
  const double lb = toy.alpha * root.min_access(0);
  if (lb > opt + kEps) {
    return fail_check("limite da raiz acima do ótimo.");
  }

  const ExactResult result = run_branch_and_bound(toy);
  if (!result.optimal || std::abs(result.solution.total_cost - opt) > kEps) {
    return fail_check(
      "exato obteve " + std::to_string(result.solution.total_cost) + " e o ótimo é " + std::to_string(opt) + ".");
  }
  if (std::abs(result.heuristic_cost - 95) > kEps) {
    return fail_check("a heurística da checagem deveria parar em 95, não em "
      + std::to_string(result.heuristic_cost) + ".");
  }
  return true;
}

}
