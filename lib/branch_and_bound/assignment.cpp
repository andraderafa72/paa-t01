#include "assignment.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace warehouse {

double hungarian(const std::vector<std::vector<double>>& cost) {
  const int rows = static_cast<int>(cost.size());
  if (rows == 0) {
    return 0;
  }
  const int cols = static_cast<int>(cost[0].size());
  if (cols < rows) {
    return kAssignmentInf;
  }

  const double huge = 1e300;
  std::vector<double> u(rows + 1, 0);
  std::vector<double> v(cols + 1, 0);
  std::vector<int> p(cols + 1, 0);
  std::vector<int> way(cols + 1, 0);

  for (int row = 1; row <= rows; ++row) {
    p[0] = row;
    int column0 = 0;
    std::vector<double> min_value(cols + 1, huge);
    std::vector<char> used(cols + 1, 0);
    do {
      used[column0] = 1;
      const int row0 = p[column0];
      double delta = huge;
      int column1 = -1;
      for (int column = 1; column <= cols; ++column) {
        if (used[column]) {
          continue;
        }
        const double current = cost[row0 - 1][column - 1] - u[row0] - v[column];
        if (current < min_value[column]) {
          min_value[column] = current;
          way[column] = column0;
        }
        if (min_value[column] < delta) {
          delta = min_value[column];
          column1 = column;
        }
      }
      if (column1 < 0) {
        return kAssignmentInf;
      }
      for (int column = 0; column <= cols; ++column) {
        if (used[column]) {
          u[p[column]] += delta;
          v[column] -= delta;
        } else {
          min_value[column] -= delta;
        }
      }
      column0 = column1;
    } while (p[column0] != 0);

    do {
      const int column1 = way[column0];
      p[column0] = p[column1];
      column0 = column1;
    } while (column0 != 0);
  }

  double sum = 0;
  for (int column = 1; column <= cols; ++column) {
    if (p[column] == 0) {
      continue;
    }
    const double chosen = cost[p[column] - 1][column - 1];
    if (chosen >= kAssignmentForbidden * 0.5) {
      return kAssignmentInf;
    }
    sum += chosen;
  }
  return sum;
}

double min_remaining_access(
    const Instance& instance,
    const std::vector<int>& products,
    const std::vector<int>& free_slots) {
  const int remaining = static_cast<int>(products.size());
  if (remaining == 0) {
    return 0;
  }
  if (static_cast<int>(free_slots.size()) < remaining) {
    return kAssignmentInf;
  }

  double max_volume = 0;
  double min_capacity = kAssignmentInf;
  std::vector<double> frequencies;
  frequencies.reserve(products.size());
  for (int product : products) {
    frequencies.push_back(instance.products[product].frequency);
    max_volume = std::max(max_volume, instance.products[product].volume);
  }

  std::vector<double> access_costs;
  access_costs.reserve(free_slots.size());
  for (int slot : free_slots) {
    access_costs.push_back(instance.slots[slot].access_cost);
    min_capacity = std::min(min_capacity, instance.slots[slot].capacity);
  }

  if (max_volume <= min_capacity) {
    std::sort(frequencies.begin(), frequencies.end(), std::greater<double>());
    std::sort(access_costs.begin(), access_costs.end());
    double sum = 0;
    for (int index = 0; index < remaining; ++index) {
      sum += frequencies[index] * access_costs[index];
    }
    return sum;
  }

  std::vector<std::vector<double>> matrix(remaining, std::vector<double>(free_slots.size(), 0));
  for (int row = 0; row < remaining; ++row) {
    const Product& product = instance.products[products[row]];
    for (int column = 0; column < static_cast<int>(free_slots.size()); ++column) {
      const Slot& slot = instance.slots[free_slots[column]];
      if (product.volume > slot.capacity) {
        matrix[row][column] = kAssignmentForbidden;
      } else {
        matrix[row][column] = product.frequency * slot.access_cost;
      }
    }
  }
  return hungarian(matrix);
}

}
