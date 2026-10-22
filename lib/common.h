#ifndef COMMON_H
#define COMMON_H

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BOLD    "\033[1m"

namespace common {

inline std::string pad_end(const std::string& input, std::size_t target_length, char pad_char = ' ') {
  if (input.size() >= target_length) {
    return input.substr(0, target_length);
  }
  return input + std::string(target_length - input.size(), pad_char);
}

inline std::string pad_start(const std::string& input, std::size_t target_length, char pad_char = ' ') {
  if (input.size() >= target_length) {
    return input;
  }
  return std::string(target_length - input.size(), pad_char) + input;
}

inline std::string format_elapsed(double elapsed_ms) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(3) << elapsed_ms << " ms ("
      << std::setprecision(3) << (elapsed_ms / 1000.0) << " s)";
  return out.str();
}

template <typename Func>
double measure_execution_ms(Func func) {
  const auto started = std::chrono::steady_clock::now();
  func();
  const auto finished = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(finished - started).count();
}

}

#endif
