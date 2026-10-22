#ifndef PROGRAM_H
#define PROGRAM_H

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
    ShowingResult,
    ShowingLog,
    Helping,
    Quitting,
  };

  enum class MenuOption {
    Load = 1,
    Greedy,
    LocalSearch,
    Improved,
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
  bool m_has_result = false;
  std::vector<TimeEntry> m_times;
  std::vector<std::string> m_catalog;
  std::string m_instance_label;

  void record_time(const std::string& label, double elapsed_ms);
  void refresh_catalog();
  void save_outputs();
  void append_log();

  void usage() const;
  bool read_line(std::string& line);
  void execute_algorithm();

  void print_welcome() const;
  void print_menu() const;
  void print_help() const;
  void print_exit() const;
  void print_loading() const;
  void print_running() const;
  void print_result() const;
  void print_log() const;

public:
  void initialize(int argc, char* argv[]);
  bool has_finished() const;

  void process_events();
  void update();
  void render() const;
};

#endif
