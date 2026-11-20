#ifndef PROGRAM_H
#define PROGRAM_H

#include "branch_and_bound/branch_and_bound.h"
#include "cost.h"
#include "instance.h"

#include <string>
#include <vector>

class Program {
private:
  enum class State {
    Starting,
    Welcoming,
    ReadingMenu,
    LoadingInstance,
    RunningGreedy,
    RunningLocalSearch,
    RunningImproved,
    RunningExact,
    Comparing,
    ShowingResult,
    ShowingCompare,
    ShowingLog,
    Helping,
    Quitting,
  };

  enum class MenuOption {
    Load = 1,
    Greedy,
    LocalSearch,
    Improved,
    Exact,
    Compare,
    Result,
    Log,
    Help,
    Quit,
    Invalid,
  };

  State m_state = State::Starting;
  MenuOption m_selected = MenuOption::Invalid;
  struct TimeEntry {
    std::string label;
    double elapsed_ms = 0;
  };

  std::string m_error;
  std::string m_msg;
  warehouse::Instance m_instance;
  warehouse::AllocationResult m_result;
  warehouse::ExactResult m_exact;
  bool m_has_result = false;
  bool m_has_exact = false;
  std::string m_compare;
  std::vector<TimeEntry> m_times;
  std::vector<std::string> m_catalog;
  std::string m_instance_label;

  void record_time(const std::string& label, double elapsed_ms);
  void run_comparison();
  void refresh_catalog();
  void save_outputs();
  void append_log();

  void usage() const;
  void run_exact_cli(const std::string& path, int seconds);
  int read_limit_seconds();
  bool read_line(std::string& line);
  void execute_algorithm();

  void print_welcome() const;
  void print_menu() const;
  void print_help() const;
  void print_exit() const;
  void print_loading() const;
  void print_running() const;
  void print_result() const;
  void print_compare() const;
  void print_log() const;

public:
  void initialize(int argc, char* argv[]);
  bool has_finished() const;

  void process_events();
  void update();
  void render() const;
};

#endif
