add_executable(paa_t01
  main.cpp
  program.cpp
)

target_link_libraries(paa_t01 PRIVATE paa_lib)
target_compile_features(paa_t01 PRIVATE cxx_std_17)
