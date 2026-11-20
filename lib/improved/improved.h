#ifndef IMPROVED_H
#define IMPROVED_H

#include "cost.h"

namespace warehouse {

AllocationResult run_improved(const Instance& instance, SearchProgressFn on_progress = nullptr);

}

#endif
