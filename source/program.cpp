#include "program.h"

#include "common.h"
#include "greedy.h"
#include "improved.h"
#include "local_search.h"

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
  return "algoritmo";
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
    << "  -h, --help  Imprime este texto e encerra.\n";
  std::exit(0);
}

void Program::initialize(int argc, char* argv[]) {
  m_state = State::Starting;
  m_error.clear();
  m_msg.clear();

  for (int i = 1; i < argc; ++i) {
    const std::string arg{argv[i]};
    if (arg == "-h" || arg == "--help") {
      usage();
    }
  }
}

bool Program::has_finished() const {
  return m_state == State::Quitting;
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

  if (m_state == State::RunningGreedy) {
    m_result = warehouse::run_greedy(m_instance);
  } else if (m_state == State::RunningLocalSearch) {
    m_result = warehouse::run_local_search(m_instance, report_search_progress);
  } else if (m_state == State::RunningImproved) {
    m_result = warehouse::run_improved(m_instance, report_search_progress);
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
      m_result = {};
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
      || m_state == State::RunningImproved) {
    execute_algorithm();
    return;
  }

  if (m_state == State::Welcoming || m_state == State::Helping
      || m_state == State::ShowingResult || m_state == State::ShowingLog) {
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
            || m_selected == MenuOption::Improved)) {
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
      || m_state == State::RunningImproved) {
    m_state = m_has_result ? State::ShowingResult : State::ReadingMenu;
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
             || m_state == State::RunningImproved) {
    print_running();
  } else if (m_state == State::ShowingResult) {
    print_result();
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
  print_row("5. Mostrar resultado");
  print_row("6. Ver log de resultados");
  print_row("7. Ajuda");
  print_row("8. Sair");
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
    << CYAN << "\nTempo de execução: " << common::format_elapsed(m_result.elapsed_ms) << "\n" << RESET
    << GREEN << "\n>>> Pressione Enter para voltar ao menu...\n" << RESET;
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
  print_row("Pastas: armazem, n20, n50, n100, n200.");
  print_row("");
  print_row("1. Lista as pastas de data/ e carrega pelo número.");
  print_row("2. Guloso por frequência.");
  print_row("3. Busca local por trocas, recálculo completo.");
  print_row("4. Heurística com custo incremental.");
  print_row("5. Mostra o último posicionamento e o tempo.");
  print_row("6. Mostra o log gravado em resultados/log.txt.");
  print_row("7. Esta ajuda.");
  print_row("8. Encerra.");
  print_rule("");
  std::cout << GREEN << ">>> Pressione Enter para voltar ao menu...\n" << RESET;
}

void Program::print_exit() const {
  std::cout << BLUE << "\nPrograma encerrado.\n" << RESET;
}
