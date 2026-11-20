#include "program.h"

#include "branch_and_bound/branch_and_bound.h"
#include "common.h"
#include "greedy/greedy.h"
#include "improved/improved.h"
#include "local_search/local_search.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void report_search_progress(const warehouse::SearchProgress& progress) {
  std::cout << YELLOW
    << ">>> Passagem " << progress.pass
    << " | trocas " << progress.accepted_swaps
    << " | movimentos " << progress.moves_evaluated
    << " | " << common::format_elapsed(progress.elapsed_ms)
    << "\n" << RESET << std::flush;
}

void report_bound_progress(const warehouse::BoundProgress& progress) {
  std::cout << YELLOW
    << ">>> Nós " << progress.nodes
    << " | podas " << progress.pruned
    << " | melhor ";
  if (progress.has_best) {
    std::cout << progress.best_cost;
  } else {
    std::cout << "nenhum";
  }
  std::cout << " | " << common::format_elapsed(progress.elapsed_ms)
    << "\n" << RESET << std::flush;
}

struct SuiteMarks {
  std::ofstream* output = nullptr;
  std::string label;
  int next = 0;
};

SuiteMarks g_suite_marks;
constexpr int kSuiteMarkMinutes[] = {5, 10, 15, 20};

void note_suite_marks(const std::string& label, double elapsed_ms, double cost, long long nodes, long long pruned) {
  if (g_suite_marks.output == nullptr) {
    return;
  }
  while (g_suite_marks.next < 4 && elapsed_ms >= kSuiteMarkMinutes[g_suite_marks.next] * 60000.0) {
    *g_suite_marks.output << "marco " << label << " " << kSuiteMarkMinutes[g_suite_marks.next]
      << " " << cost << " " << nodes << " " << pruned << "\n";
    g_suite_marks.output->flush();
    std::cout << "marco " << label << " " << kSuiteMarkMinutes[g_suite_marks.next]
      << " min | custo " << cost << " | nós " << nodes << " | podas " << pruned << "\n" << std::flush;
    ++g_suite_marks.next;
  }
}

void report_suite_progress(const warehouse::BoundProgress& progress) {
  report_bound_progress(progress);
  note_suite_marks(
    g_suite_marks.label,
    progress.elapsed_ms,
    progress.has_best ? progress.best_cost : -1,
    progress.nodes,
    progress.pruned);
}

std::filesystem::path data_root() {
  if (std::filesystem::is_directory("data")) {
    return "data";
  }
  if (std::filesystem::is_directory("PAA-T01/data")) {
    return "PAA-T01/data";
  }
  return "data";
}

std::filesystem::path results_root() {
  if (std::filesystem::is_directory("PAA-T01")) {
    return "PAA-T01/resultados";
  }
  return "resultados";
}

std::filesystem::path log_path() {
  return results_root() / "log.txt";
}

std::string report_field(const std::string& text, const std::string& key) {
  const std::string prefix = key + ": ";
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t end = text.find('\n', start);
    const std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (line.compare(0, prefix.size(), prefix) == 0) {
      return line.substr(prefix.size());
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return "";
}

std::string log_line_from_report(const std::filesystem::path& report) {
  std::ifstream input(report);
  if (!input) {
    return "";
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const std::string text = buffer.str();
  const std::string instance = report_field(text, "Instancia");
  const std::string algorithm = report_field(text, "Algoritmo");
  const std::string cost = report_field(text, "Custo total");
  const std::string elapsed = report_field(text, "Tempo (ms)");
  const std::string moves = report_field(text, "Movimentos avaliados");
  const std::string validity = report_field(text, "Validade");
  if (instance.empty() || algorithm.empty()) {
    return "";
  }
  return instance + " | " + algorithm + " | custo " + cost + " | " + elapsed + " ms | " + moves
    + " movimentos | " + validity;
}

void write_log_from_reports(const std::filesystem::path& path) {
  std::vector<std::filesystem::path> reports;
  const std::filesystem::path root = results_root();
  if (std::filesystem::is_directory(root)) {
    for (const std::filesystem::directory_entry& directory : std::filesystem::directory_iterator(root)) {
      if (!directory.is_directory()) {
        continue;
      }
      for (const std::filesystem::directory_entry& file : std::filesystem::directory_iterator(directory.path())) {
        const std::string name = file.path().filename().string();
        if (name.rfind("relatorio-", 0) == 0 && name.size() > 4 && name.substr(name.size() - 4) == ".txt") {
          reports.push_back(file.path());
        }
      }
    }
  }
  std::sort(reports.begin(), reports.end());

  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("Não foi possível gravar " + path.string());
  }
  output << "# instancia | algoritmo | custo | tempo | movimentos | validade\n";
  for (const std::filesystem::path& report : reports) {
    const std::string line = log_line_from_report(report);
    if (!line.empty()) {
      output << line << "\n";
    }
  }
}

std::string algorithm_slug(const std::string& name) {
  if (name.find("Guloso") != std::string::npos) {
    return "guloso";
  }
  if (name.find("Busca") != std::string::npos) {
    return "busca-local";
  }
  if (name.find("Heur") != std::string::npos) {
    return "heuristica";
  }
  if (name.find("Branch") != std::string::npos) {
    return "branch-and-bound";
  }
  return "algoritmo";
}

bool compare_instance(const std::string& label, std::size_t products) {
  if (products > 20) {
    return false;
  }
  if (label == "armazem") {
    return true;
  }
  if (label.size() < 2 || label[0] != 'n') {
    return false;
  }
  for (std::size_t index = 1; index < label.size(); ++index) {
    if (std::isdigit(static_cast<unsigned char>(label[index])) == 0) {
      return false;
    }
  }
  return true;
}

std::string trim(std::string text) {
  const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!text.empty() && is_space(static_cast<unsigned char>(text.front()))) {
    text.erase(text.begin());
  }
  while (!text.empty() && is_space(static_cast<unsigned char>(text.back()))) {
    text.pop_back();
  }
  return text;
}

}

void Program::usage() const {
  std::cerr
    << "Uso: paa_t01 [-h, --help]\n"
    << "     paa_t01 --exato <pasta> <segundos>\n"
    << "  -h, --help  Imprime este texto e encerra.\n"
    << "  --exato     Roda o Branch and Bound e acrescenta uma linha em resultados/suites.txt.\n"
    << "              <segundos> é o teto. 0 segue até provar o ótimo. 1200 são 20 minutos.\n";
  std::exit(0);
}

void Program::run_exact_cli(const std::string& path, int seconds) {
  const warehouse::Instance instance = warehouse::load_instance(path);
  const std::string label = std::filesystem::path(path).filename().string();
  std::filesystem::create_directories(results_root());
  const std::filesystem::path out_path = results_root() / "suites.txt";
  const bool fresh = !std::filesystem::exists(out_path);
  std::ofstream output(out_path, std::ios::app);
  if (!output) {
    throw std::runtime_error("Não foi possível gravar " + out_path.string());
  }
  if (fresh) {
    output << "# tipo instancia n guloso heuristica exato gap_h nos podas tempo_ms status\n";
    output << "checagem toy 3 95 95 77 0 - - - otimo\n";
  }

  g_suite_marks.output = &output;
  g_suite_marks.label = label;
  g_suite_marks.next = 0;
  std::cout << ">>> " << label << (seconds > 0 ? " teto " + std::to_string(seconds) + " s" : " sem limite")
    << "\n" << std::flush;
  const warehouse::ExactResult result = warehouse::run_branch_and_bound(instance, report_suite_progress, seconds);
  note_suite_marks(
    label,
    result.solution.elapsed_ms,
    result.solution.total_cost,
    result.nodes,
    result.pruned);
  g_suite_marks.output = nullptr;

  const char* status = "sem solucao";
  if (result.optimal) {
    status = "otimo";
  } else if (result.solution.valid) {
    status = "tempo esgotado";
  }
  output << std::fixed << std::setprecision(3)
    << "exato " << label << " " << instance.products.size() << " "
    << result.greedy_cost << " " << result.heuristic_cost << " " << result.solution.total_cost << " "
    << std::setprecision(2) << result.gap_heuristic << (result.certified ? "" : "*") << " "
    << result.nodes << " " << result.pruned << " "
    << std::setprecision(1) << result.solution.elapsed_ms << " " << status << "\n";
  output.flush();
  std::cout << "exato " << label << " | heuristica " << result.heuristic_cost
    << " | exato " << result.solution.total_cost
    << " | gap " << result.gap_heuristic << (result.certified ? "" : "*")
    << " | " << status << " | " << common::format_elapsed(result.solution.elapsed_ms) << "\n";
}

void Program::initialize(int argc, char* argv[]) {
  m_state = State::Starting;
  m_error.clear();
  m_msg.clear();

  std::string exact_path;
  int exact_seconds = 0;
  bool exact_mode = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg{argv[i]};
    if (arg == "-h" || arg == "--help") {
      usage();
    }
    if (arg == "--exato") {
      if (i + 2 >= argc) {
        usage();
      }
      exact_path = argv[++i];
      try {
        exact_seconds = std::stoi(argv[++i]);
      } catch (const std::exception&) {
        usage();
      }
      exact_mode = true;
    }
  }

  if (!warehouse::branch_and_bound_self_check()) {
    std::cerr << "Falha na checagem do Branch and Bound.\n";
    std::exit(1);
  }

  if (exact_mode) {
    try {
      run_exact_cli(exact_path, exact_seconds);
    } catch (const std::exception& error) {
      std::cerr << error.what() << "\n";
      std::exit(1);
    }
    m_state = State::Quitting;
  }
}

bool Program::has_finished() const {
  return m_state == State::Quitting;
}

int Program::read_limit_seconds() {
  std::cout << "Limite em segundos (Enter = sem limite, 1200 = 20 min): " << std::flush;
  std::string line;
  if (!read_line(line)) {
    return -1;
  }
  line = trim(line);
  if (line.empty()) {
    return 0;
  }
  try {
    const int seconds = std::stoi(line);
    if (seconds < 0) {
      throw std::runtime_error("negativo");
    }
    return seconds;
  } catch (const std::exception&) {
    m_error = "Informe o limite em segundos, ou Enter para não cortar.";
    return -1;
  }
}

bool Program::read_line(std::string& line) {
  if (!std::getline(std::cin, line)) {
    m_state = State::Quitting;
    return false;
  }
  return true;
}

void Program::execute_algorithm() {
  if (!m_instance.loaded) {
    m_error = "Carregue uma instância antes de executar.";
    return;
  }

  m_has_exact = false;
  if (m_state == State::RunningGreedy) {
    m_result = warehouse::run_greedy(m_instance);
  } else if (m_state == State::RunningLocalSearch) {
    m_result = warehouse::run_local_search(m_instance, report_search_progress);
  } else if (m_state == State::RunningImproved) {
    m_result = warehouse::run_improved(m_instance, report_search_progress);
  } else if (m_state == State::RunningExact) {
    const int seconds = read_limit_seconds();
    if (seconds < 0) {
      return;
    }
    m_exact = warehouse::run_branch_and_bound(m_instance, report_bound_progress, seconds);
    m_result = m_exact.solution;
    m_has_exact = true;
  }

  m_result.total_cost = warehouse::total_cost(m_result.cost, m_instance.alpha, m_instance.beta);
  m_has_result = true;
  record_time(m_result.algorithm, m_result.elapsed_ms);
  m_msg = m_result.summary + " Tempo: " + common::format_elapsed(m_result.elapsed_ms);
  try {
    save_outputs();
  } catch (const std::exception& error) {
    m_error = error.what();
  }
}

void Program::record_time(const std::string& label, double elapsed_ms) {
  m_times.push_back(TimeEntry{label, elapsed_ms});
}

void Program::refresh_catalog() {
  m_catalog.clear();
  const std::filesystem::path root = data_root();
  if (!std::filesystem::is_directory(root)) {
    return;
  }

  for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root)) {
    if (!entry.is_directory()) {
      continue;
    }
    if (std::filesystem::is_regular_file(entry.path() / "produtos.txt")) {
      m_catalog.push_back(entry.path().filename().string());
    }
  }
  std::sort(m_catalog.begin(), m_catalog.end());
}

void Program::save_outputs() {
  const std::string label = m_instance_label.empty() ? "instancia" : m_instance_label;
  const std::filesystem::path directory = results_root() / label;
  std::filesystem::create_directories(directory);

  const std::string slug = algorithm_slug(m_result.algorithm);
  const std::filesystem::path report_path = directory / "relatorio.txt";
  const std::filesystem::path solution_path = directory / "melhor-solucao.txt";
  const std::filesystem::path report_copy = directory / ("relatorio-" + slug + ".txt");
  const std::filesystem::path solution_copy = directory / ("melhor-solucao-" + slug + ".txt");

  std::ostringstream report;
  report << "Instancia: " << label << "\n"
         << "Produtos: " << m_instance.products.size() << "\n"
         << "Posicoes: " << m_instance.slots.size() << "\n"
         << "Algoritmo: " << m_result.algorithm << "\n"
         << "Custo de acesso: " << m_result.cost.access << "\n"
         << "Custo de separacao: " << m_result.cost.separation << "\n"
         << "Custo total: " << m_result.total_cost << "\n"
         << "Tempo (ms): " << m_result.elapsed_ms << "\n"
         << "Movimentos avaliados: " << m_result.moves_evaluated << "\n"
         << "Ocupacao (%): " << m_result.occupancy << "\n"
         << "Validade: " << m_result.validity << "\n"
         << "Resumo: " << m_result.summary << "\n";
  if (m_has_exact) {
    report << "Nos explorados: " << m_exact.nodes << "\n"
           << "Ramos podados: " << m_exact.pruned << "\n"
           << "Otimo provado: " << (m_exact.optimal ? "sim" : "nao") << "\n"
           << "Custo guloso: " << m_exact.greedy_cost << "\n"
           << "Custo busca local: " << m_exact.local_cost << "\n"
           << "Custo heuristica: " << m_exact.heuristic_cost << "\n"
           << std::fixed << std::setprecision(2)
           << "Gap guloso (%): " << m_exact.gap_greedy << "\n"
           << "Gap busca local (%): " << m_exact.gap_local << "\n"
           << "Gap heuristica (%): " << m_exact.gap_heuristic << "\n"
           << "Gap certificado: " << (m_exact.certified ? "sim" : "nao, contra o incumbente") << "\n";
  }

  std::ostringstream solution;
  solution << "Instancia: " << label << "\n"
           << "Algoritmo: " << m_result.algorithm << "\n"
           << "Custo total: " << m_result.total_cost << "\n"
           << "Posicao;ProdutoId;Produto;Volume;Capacidade;Frequencia\n";
  for (const warehouse::Placement& placement : m_result.placements) {
    solution << placement.slot_id << ";"
             << placement.product_id << ";"
             << placement.product_name << ";"
             << placement.volume << ";"
             << placement.capacity << ";"
             << placement.frequency << "\n";
  }

  const auto write_file = [](const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path);
    if (!output) {
      throw std::runtime_error("Não foi possível gravar " + path.string());
    }
    output << text;
  };

  const std::string report_text = report.str();
  const std::string solution_text = solution.str();
  write_file(report_path, report_text);
  write_file(solution_path, solution_text);
  write_file(report_copy, report_text);
  write_file(solution_copy, solution_text);
  append_log();
  m_msg += " Arquivos: " + report_path.generic_string() + ", " + solution_path.generic_string()
    + " e " + log_path().generic_string() + ".";
}

void Program::append_log() {
  std::filesystem::create_directories(results_root());
  const std::filesystem::path path = log_path();
  if (!std::filesystem::exists(path)) {
    write_log_from_reports(path);
    return;
  }

  std::ostringstream line;
  line << m_instance_label << " | " << m_result.algorithm
       << " | custo " << m_result.total_cost
       << " | " << m_result.elapsed_ms << " ms | "
       << m_result.moves_evaluated << " movimentos | " << m_result.validity;

  std::ofstream output(path, std::ios::app);
  if (!output) {
    throw std::runtime_error("Não foi possível gravar " + path.string());
  }
  output << line.str() << "\n";
}

void Program::process_events() {
  m_error.clear();
  m_msg.clear();

  if (m_state == State::ReadingMenu) {
    std::string line;
    if (!read_line(line)) {
      return;
    }

    try {
      const int option = std::stoi(line);
      if (option < static_cast<int>(MenuOption::Load) || option > static_cast<int>(MenuOption::Quit)) {
        throw std::runtime_error("opção fora do intervalo");
      }
      m_selected = static_cast<MenuOption>(option);
    } catch (const std::exception&) {
      m_error = "Opção inválida.";
      m_selected = MenuOption::Invalid;
    }
    return;
  }

  if (m_state == State::LoadingInstance) {
    std::string line;
    if (!read_line(line)) {
      return;
    }
    line = trim(line);
    int index = 0;
    try {
      index = std::stoi(line);
    } catch (const std::exception&) {
      m_error = "Informe o número da instância.";
      return;
    }
    if (index < 1 || index > static_cast<int>(m_catalog.size())) {
      m_error = "Instância fora da lista.";
      return;
    }

    m_instance_label = m_catalog[static_cast<std::size_t>(index - 1)];
    const std::string path = (data_root() / m_instance_label).generic_string();

    try {
      warehouse::Instance loaded;
      const double elapsed_ms = common::measure_execution_ms([&]() {
        loaded = warehouse::load_instance(path);
      });
      m_instance = std::move(loaded);
      m_has_result = false;
      m_has_exact = false;
      m_result = {};
      m_exact = {};
      m_times.clear();
      record_time("Carregar instância", elapsed_ms);
      m_msg = "Instância carregada: " + std::to_string(m_instance.products.size()) + " produtos, "
        + std::to_string(m_instance.slots.size()) + " posições. Tempo: " + common::format_elapsed(elapsed_ms);
    } catch (const std::exception& error) {
      m_error = error.what();
    }
    return;
  }

  if (m_state == State::RunningGreedy
      || m_state == State::RunningLocalSearch
      || m_state == State::RunningImproved
      || m_state == State::RunningExact) {
    execute_algorithm();
    return;
  }

  if (m_state == State::Comparing) {
    run_comparison();
    return;
  }

  if (m_state == State::Welcoming || m_state == State::Helping
      || m_state == State::ShowingResult || m_state == State::ShowingCompare
      || m_state == State::ShowingLog) {
    std::string line;
    read_line(line);
  }
}

void Program::update() {
  if (m_state == State::Quitting) {
    return;
  }

  if (m_state == State::Starting) {
    m_state = State::Welcoming;
    return;
  }

  if (m_state == State::Welcoming) {
    m_state = State::ReadingMenu;
    return;
  }

  if (m_state == State::ReadingMenu) {
    if (!m_instance.loaded
        && (m_selected == MenuOption::Greedy
            || m_selected == MenuOption::LocalSearch
            || m_selected == MenuOption::Improved
            || m_selected == MenuOption::Exact)) {
      m_error = "Carregue uma instância antes de executar.";
      return;
    }

    if (m_selected == MenuOption::Result && !m_has_result) {
      m_msg = "Nenhum algoritmo foi executado.";
      return;
    }

    switch (m_selected) {
    case MenuOption::Load:
      refresh_catalog();
      m_state = State::LoadingInstance;
      break;
    case MenuOption::Greedy:
      m_state = State::RunningGreedy;
      break;
    case MenuOption::LocalSearch:
      m_state = State::RunningLocalSearch;
      break;
    case MenuOption::Improved:
      m_state = State::RunningImproved;
      break;
    case MenuOption::Exact:
      m_state = State::RunningExact;
      break;
    case MenuOption::Compare:
      m_state = State::Comparing;
      break;
    case MenuOption::Result:
      m_state = State::ShowingResult;
      break;
    case MenuOption::Log:
      m_state = State::ShowingLog;
      break;
    case MenuOption::Help:
      m_state = State::Helping;
      break;
    case MenuOption::Quit:
      m_state = State::Quitting;
      break;
    case MenuOption::Invalid:
      break;
    }
    return;
  }

  if (m_state == State::RunningGreedy
      || m_state == State::RunningLocalSearch
      || m_state == State::RunningImproved
      || m_state == State::RunningExact) {
    m_state = m_has_result ? State::ShowingResult : State::ReadingMenu;
    return;
  }

  if (m_state == State::Comparing) {
    m_state = m_error.empty() ? State::ShowingCompare : State::ReadingMenu;
    return;
  }

  m_state = State::ReadingMenu;
}

void Program::render() const {
  if (m_state == State::Welcoming) {
    print_welcome();
  } else if (m_state == State::ReadingMenu) {
    print_menu();
  } else if (m_state == State::LoadingInstance) {
    print_loading();
  } else if (m_state == State::RunningGreedy
             || m_state == State::RunningLocalSearch
             || m_state == State::RunningImproved
             || m_state == State::RunningExact
             || m_state == State::Comparing) {
    print_running();
  } else if (m_state == State::ShowingResult) {
    print_result();
  } else if (m_state == State::ShowingCompare) {
    print_compare();
  } else if (m_state == State::ShowingLog) {
    print_log();
  } else if (m_state == State::Helping) {
    print_help();
  } else if (m_state == State::Quitting) {
    print_exit();
  }
}

namespace {

constexpr int kBoxWidth = 76;
constexpr int kBoxInner = 72;

std::size_t display_width(const std::string& text) {
  std::size_t width = 0;
  for (std::size_t index = 0; index < text.size();) {
    const unsigned char ch = static_cast<unsigned char>(text[index]);
    std::size_t bytes = 1;
    if ((ch & 0x80) == 0) {
      bytes = 1;
    } else if ((ch & 0xE0) == 0xC0) {
      bytes = 2;
    } else if ((ch & 0xF0) == 0xE0) {
      bytes = 3;
    } else if ((ch & 0xF8) == 0xF0) {
      bytes = 4;
    }
    if (index + bytes > text.size()) {
      bytes = 1;
    }
    ++width;
    index += bytes;
  }
  return width;
}

std::string pad_display(const std::string& text, std::size_t width) {
  const std::size_t current = display_width(text);
  if (current >= width) {
    return text;
  }
  return text + std::string(width - current, ' ');
}

std::string fit_inner(const std::string& text) {
  if (display_width(text) <= static_cast<std::size_t>(kBoxInner)) {
    return text + std::string(static_cast<std::size_t>(kBoxInner) - display_width(text), ' ');
  }

  std::string cut;
  std::size_t width = 0;
  for (std::size_t index = 0; index < text.size() && width < static_cast<std::size_t>(kBoxInner);) {
    const unsigned char ch = static_cast<unsigned char>(text[index]);
    std::size_t bytes = 1;
    if ((ch & 0xE0) == 0xC0) {
      bytes = 2;
    } else if ((ch & 0xF0) == 0xE0) {
      bytes = 3;
    } else if ((ch & 0xF8) == 0xF0) {
      bytes = 4;
    }
    if (index + bytes > text.size()) {
      bytes = 1;
    }
    cut.append(text, index, bytes);
    ++width;
    index += bytes;
  }
  return cut;
}

void print_rule(const std::string& title) {
  if (title.empty()) {
    std::cout << std::string(kBoxWidth, '-') << "\n";
    return;
  }
  const std::string label = " " + title + " ";
  const int remaining = kBoxWidth - static_cast<int>(display_width(label));
  const int left = remaining > 0 ? remaining / 2 : 0;
  const int right = remaining > 0 ? remaining - left : 0;
  std::cout << std::string(left, '-') << label << std::string(right, '-') << "\n";
}

void print_row(const std::string& text) {
  std::cout << "| " << fit_inner(text) << " |\n";
}

}

void Program::print_welcome() const {
  std::cout
    << BLUE << BOLD << "Alocação de produtos no armazém\n\n" << RESET
    << "Produtos frequentes ficam em posições fáceis de alcançar.\n"
    << "Itens que saem no mesmo pedido ficam perto, se a posição couber o volume.\n\n"
    << GREEN << ">>> Pressione Enter para iniciar..." << RESET;
}

void Program::print_menu() const {
  const std::string products = m_instance.loaded
    ? common::pad_start(std::to_string(m_instance.products.size()), 4, '0')
    : "0000";
  const std::string slots = m_instance.loaded
    ? common::pad_start(std::to_string(m_instance.slots.size()), 4, '0')
    : "0000";
  const std::string instance_name = m_instance.loaded ? m_instance.name : "nenhuma";
  const std::string algorithm_name = m_has_result ? m_result.algorithm : "nenhum";

  std::cout << "\n\n" << BLUE;
  print_rule("Alocação de produtos no armazém");
  print_row(products + " produtos   " + slots + " posições");
  print_row("Instância: " + instance_name);
  print_row("Algoritmo: " + algorithm_name);
  print_row("Selecione uma opção:");
  print_row("");
  print_row("1. Carregar instância");
  print_row("2. Guloso por frequência");
  print_row("3. Busca local por trocas");
  print_row("4. Heurística aprimorada");
  print_row("5. Branch and Bound");
  print_row("6. Comparar heurística e ótimo");
  print_row("7. Mostrar resultado");
  print_row("8. Ver log de resultados");
  print_row("9. Ajuda");
  print_row("10. Sair");
  print_row("");
  print_row("Tempos de execução");
  if (m_times.empty()) {
    print_row("Nenhuma execução ainda.");
  } else {
    for (const TimeEntry& entry : m_times) {
      print_row(pad_display(entry.label, 26) + common::format_elapsed(entry.elapsed_ms));
    }
  }
  print_rule("");
  std::cout
    << RED << "ERROR: [" << m_error << "]\n" << RESET
    << YELLOW << "MSG: [" << m_msg << "]\n\n" << RESET
    << ">>> ";
}

void Program::print_loading() const {
  std::cout << "\n" << BLUE;
  print_rule("Instâncias");
  if (m_catalog.empty()) {
    print_row("Nenhuma pasta com produtos.txt em data/.");
  } else {
    for (std::size_t index = 0; index < m_catalog.size(); ++index) {
      print_row(std::to_string(index + 1) + ". " + m_catalog[index]);
    }
  }
  print_rule("");
  std::cout << YELLOW << "Número da instância\n" << RESET << ">>> ";
}

void Program::print_running() const {
  std::string name = "algoritmo";
  if (m_state == State::RunningGreedy) {
    name = "Guloso por frequência";
  } else if (m_state == State::RunningLocalSearch) {
    name = "Busca local por trocas";
  } else if (m_state == State::RunningImproved) {
    name = "Heurística aprimorada";
  } else if (m_state == State::RunningExact) {
    name = "Branch and Bound";
  } else if (m_state == State::Comparing) {
    name = "comparação nas instâncias pequenas";
  }

  std::cout << YELLOW << "\n>>> Executando " << name << "...\n" << RESET;
}

void Program::print_result() const {
  std::cout << "\n" << BLUE << BOLD << "Posicionamento: " << m_result.algorithm << "\n"
    << "------------------------------------------------------------------------------------\n"
    << "| Pos | Produto          | Volume | Capacidade | Frequencia |\n"
    << "------------------------------------------------------------------------------------\n"
    << RESET;

  int placed = 0;
  const auto flags = std::cout.flags();
  for (const warehouse::Placement& placement : m_result.placements) {
    if (placement.slot_id < 0) {
      continue;
    }
    ++placed;
    std::cout << "| "
      << std::setw(3) << placement.slot_id << " | "
      << std::left << std::setw(16) << placement.product_name << std::right << " | "
      << std::setw(6) << placement.volume << " | "
      << std::setw(10) << placement.capacity << " | "
      << std::setw(10) << placement.frequency << " |\n";
  }
  std::cout.flags(flags);

  if (placed == 0) {
    std::cout << "| Nenhum produto posicionado.                                                |\n";
  }

  std::cout << BLUE << BOLD
    << "------------------------------------------------------------------------------------\n"
    << RESET;

  for (const warehouse::Placement& placement : m_result.placements) {
    if (placement.slot_id >= 0) {
      continue;
    }
    std::cout << YELLOW << "Sem posição: " << placement.product_name
      << " (volume " << placement.volume << ")\n" << RESET;
  }

  std::cout
    << "\n" << BLUE << BOLD << "Resultado: " << m_result.algorithm << "\n" << RESET
    << "Custo de acesso: " << m_result.cost.access << "\n"
    << "Custo de separação: " << m_result.cost.separation << "\n"
    << "Custo total: " << m_result.total_cost << "\n"
    << "Movimentos avaliados: " << m_result.moves_evaluated << "\n"
    << "Ocupação (%): " << m_result.occupancy << "\n"
    << "Validade: " << m_result.validity << "\n"
    << m_result.summary << "\n"
    << CYAN << "\nTempo de execução: " << common::format_elapsed(m_result.elapsed_ms) << "\n" << RESET;

  if (m_has_exact) {
    const auto flags = std::cout.flags();
    std::cout << std::fixed << std::setprecision(3)
      << "\nCusto guloso: " << m_exact.greedy_cost << "\n"
      << "Custo busca local: " << m_exact.local_cost << "\n"
      << "Custo heurística: " << m_exact.heuristic_cost << "\n"
      << "Custo exato: " << m_exact.solution.total_cost << "\n"
      << std::setprecision(2)
      << "Gap guloso (%): " << m_exact.gap_greedy << "\n"
      << "Gap busca local (%): " << m_exact.gap_local << "\n"
      << "Gap heurística (%): " << m_exact.gap_heuristic << "\n";
    std::cout.flags(flags);
    if (m_exact.certified) {
      std::cout << "Gap certificado pelo ótimo.\n";
    } else {
      std::cout << "Gap contra o incumbente, sem certificado.\n";
    }
    std::cout << "Nós explorados: " << m_exact.nodes << "\n"
      << "Ramos podados: " << m_exact.pruned << "\n";
  }

  std::cout << GREEN << "\n>>> Pressione Enter para voltar ao menu...\n" << RESET;
}

void Program::run_comparison() {
  refresh_catalog();
  std::ostringstream table;
  table << std::fixed;
  table << "instancia | n | guloso | busca local | heuristica | exato | "
        << "gap guloso % | gap busca % | gap heuristica % | nos | podas | tempo ms | status\n";

  struct Pending {
    std::string label;
    warehouse::Instance instance;
  };
  std::vector<Pending> pending;
  for (const std::string& label : m_catalog) {
    warehouse::Instance instance;
    try {
      instance = warehouse::load_instance((data_root() / label).generic_string());
    } catch (const std::exception& error) {
      table << label << " | erro | " << error.what() << "\n";
      continue;
    }
    if (!compare_instance(label, instance.products.size())) {
      continue;
    }
    pending.push_back(Pending{label, std::move(instance)});
  }
  std::sort(pending.begin(), pending.end(), [](const Pending& left, const Pending& right) {
    if (left.instance.products.size() != right.instance.products.size()) {
      return left.instance.products.size() < right.instance.products.size();
    }
    return left.label < right.label;
  });

  bool any = !pending.empty();
  int seconds = 0;
  if (any) {
    seconds = read_limit_seconds();
    if (seconds < 0) {
      return;
    }
  }
  for (const Pending& item : pending) {
    const warehouse::Instance& instance = item.instance;
    std::cout << YELLOW << ">>> " << item.label << " (" << instance.products.size()
      << " produtos)\n" << RESET << std::flush;
    const warehouse::ExactResult result = warehouse::run_branch_and_bound(
      instance, report_bound_progress, seconds);
    const char* status = "sem solucao";
    if (result.optimal) {
      status = "otimo";
    } else if (result.solution.valid) {
      status = "tempo esgotado";
    }
    table << item.label << " | " << instance.products.size() << " | "
          << std::setprecision(3)
          << result.greedy_cost << " | "
          << result.local_cost << " | "
          << result.heuristic_cost << " | "
          << result.solution.total_cost << " | "
          << std::setprecision(2)
          << result.gap_greedy << (result.certified ? "" : "*") << " | "
          << result.gap_local << (result.certified ? "" : "*") << " | "
          << result.gap_heuristic << (result.certified ? "" : "*") << " | "
          << result.nodes << " | "
          << result.pruned << " | "
          << std::setprecision(1) << result.solution.elapsed_ms << " | "
          << status << "\n";
  }
  if (!any) {
    table << "Nenhuma instância entre armazem, n8, n10, n12, n15 e n20.\n";
  }
  table << "* gap contra o incumbente, sem certificado de otimalidade.\n";
  m_compare = table.str();

  try {
    std::filesystem::create_directories(results_root());
    const std::filesystem::path path = results_root() / "comparacao.txt";
    std::ofstream output(path);
    if (!output) {
      m_error = "Não foi possível gravar " + path.string();
      return;
    }
    output << m_compare;
    m_msg = "Arquivo: " + path.generic_string();
  } catch (const std::exception& error) {
    m_error = error.what();
  }
}

void Program::print_compare() const {
  std::cout << "\n" << BLUE << BOLD << "Comparação com o ótimo\n" << RESET
    << m_compare << "\n";
  if (!m_error.empty()) {
    std::cout << RED << m_error << "\n" << RESET;
  }
  if (!m_msg.empty()) {
    std::cout << YELLOW << m_msg << "\n" << RESET;
  }
  std::cout << GREEN << "\n>>> Pressione Enter para voltar ao menu...\n" << RESET;
}

void Program::print_log() const {
  const std::filesystem::path path = log_path();
  if (!std::filesystem::exists(path)) {
    try {
      std::filesystem::create_directories(results_root());
      write_log_from_reports(path);
    } catch (const std::exception&) {
    }
  }

  std::cout << "\n" << BLUE << BOLD << "Log de resultados\n" << RESET
    << path.generic_string() << "\n\n";

  std::ifstream input(path);
  bool has_entry = false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    has_entry = true;
    std::cout << line << "\n";
  }

  if (!has_entry) {
    std::cout << "Nenhum resultado gravado ainda.\n";
  }

  std::cout << GREEN << "\n>>> Pressione Enter para voltar ao menu...\n" << RESET;
}

void Program::print_help() const {
  std::cout << "\n" << BLUE;
  print_rule("Ajuda");
  print_row("Cada produto ocupa uma posição com capacidade suficiente.");
  print_row("O custo junta acesso e separação de itens do mesmo pedido.");
  print_row("");
  print_row("Pasta da instância, textos com ';' ou ',':");
  print_row("produtos, posicoes, distancias, coocorrencia, pesos.");
  print_row("Pastas: armazem, n8, n10, n12, n15, n20 e maiores.");
  print_row("");
  print_row("1. Lista as pastas de data/ e carrega pelo número.");
  print_row("2. Guloso por frequência.");
  print_row("3. Busca local por trocas, recálculo completo.");
  print_row("4. Heurística com custo incremental.");
  print_row("5. Branch and Bound. Pede o limite em segundos.");
  print_row("6. Compara nas pastas armazem, n8, n10, n12, n15 e n20.");
  print_row("7. Mostra o último posicionamento e o tempo.");
  print_row("8. Mostra o log gravado em resultados/log.txt.");
  print_row("9. Esta ajuda.");
  print_row("10. Encerra.");
  print_rule("");
  std::cout << GREEN << ">>> Pressione Enter para voltar ao menu...\n" << RESET;
}

void Program::print_exit() const {
  std::cout << BLUE << "\nPrograma encerrado.\n" << RESET;
}
