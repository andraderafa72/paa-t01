#include "greedy.h"

#include <algorithm>
#include <chrono>
#include <vector>

namespace warehouse {

AllocationResult run_greedy(const Instance& instance) {
  const auto started = std::chrono::steady_clock::now();
  const int product_count = static_cast<int>(instance.products.size());
  const int slot_count = static_cast<int>(instance.slots.size());

  std::vector<int> product_order(product_count);
  std::vector<int> slot_order(slot_count);
  for (int index = 0; index < product_count; ++index) {
    product_order[index] = index;
  }
  for (int index = 0; index < slot_count; ++index) {
    slot_order[index] = index;
  }

  std::sort(product_order.begin(), product_order.end(), [&](int left, int right) {
    const Product& a = instance.products[left];
    const Product& b = instance.products[right];
    if (a.frequency != b.frequency) {
      return a.frequency > b.frequency;
    }
    return a.id < b.id;
  });

  std::sort(slot_order.begin(), slot_order.end(), [&](int left, int right) {
    const Slot& a = instance.slots[left];
    const Slot& b = instance.slots[right];
    if (a.access_cost != b.access_cost) {
      return a.access_cost < b.access_cost;
    }
    return a.id < b.id;
  });

  std::vector<int> slot_of_product(product_count, -1);
  std::vector<bool> slot_used(slot_count, false);
  int moves = 0;

  for (int product_index : product_order) {
    const Product& product = instance.products[product_index];
    for (int slot_index : slot_order) {
      ++moves;
      if (slot_used[slot_index]) {
        continue;
      }
      if (instance.slots[slot_index].capacity < product.volume) {
        continue;
      }
      slot_used[slot_index] = true;
      slot_of_product[product_index] = slot_index;
      break;
    }
  }

  const auto finished = std::chrono::steady_clock::now();
  const double elapsed_ms = std::chrono::duration<double, std::milli>(finished - started).count();
  return build_result(instance, std::move(slot_of_product), "Guloso por frequência", moves, elapsed_ms);
}

}
