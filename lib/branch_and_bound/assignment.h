#ifndef ASSIGNMENT_H
#define ASSIGNMENT_H

#include "instance.h"

#include <vector>

namespace warehouse {

constexpr double kAssignmentInf = 1e100;
constexpr double kAssignmentForbidden = 1e12;

double hungarian(const std::vector<std::vector<double>>& cost);

double min_remaining_access(
  const Instance& instance,
  const std::vector<int>& products,
  const std::vector<int>& free_slots);

}

#endif
