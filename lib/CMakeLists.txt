add_library(paa_lib
  instance.cpp
  cost.cpp
  greedy.cpp
  local_search.cpp
  improved.cpp
)

target_include_directories(paa_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(paa_lib PUBLIC cxx_std_17)
