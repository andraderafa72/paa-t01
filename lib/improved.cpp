#include "improved.h"

#include "greedy.h"

#include <chrono>
#include <string>

namespace warehouse {

AllocationResult run_improved(const Instance& instance, SearchProgressFn on_progress) {
  const auto started = std::chrono::steady_clock::now();
  const AllocationResult seed = run_greedy(instance);
  const SwapSearch search = improve_by_swaps(
    instance, seed.slot_of_product, seed.moves_evaluated, SwapEvaluation::Incremental, on_progress);
  const auto finished = std::chrono::steady_clock::now();
  const double elapsed_ms = std::chrono::duration<double, std::milli>(finished - started).count();

  AllocationResult result = build_result(
    instance, search.slot_of_product, "Heurística aprimorada", search.moves_evaluated, elapsed_ms);
  if (search.accepted_swaps > 0) {
    result.summary += " Trocas aceitas: " + std::to_string(search.accepted_swaps) + ".";
  } else {
    result.summary += " Nenhuma troca melhorou o custo.";
  }
  return result;
}

}
