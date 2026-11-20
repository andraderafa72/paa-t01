add_library(paa_lib
  instance.cpp
  cost.cpp
  greedy/greedy.cpp
  local_search/local_search.cpp
  improved/improved.cpp
  branch_and_bound/assignment.cpp
  branch_and_bound/branch_and_bound.cpp
)

target_include_directories(paa_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(paa_lib PUBLIC cxx_std_17)
