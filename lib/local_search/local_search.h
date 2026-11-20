#ifndef LOCAL_SEARCH_H
#define LOCAL_SEARCH_H

#include "cost.h"

namespace warehouse {

AllocationResult run_local_search(const Instance& instance, SearchProgressFn on_progress = nullptr);

}

#endif
