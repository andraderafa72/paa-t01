#include "instance.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace warehouse {
namespace {

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

std::vector<std::string> split_fields(const std::string& line) {
  const char separator = line.find(';') != std::string::npos ? ';' : ',';
  std::vector<std::string> fields;
  std::string field;
  for (char ch : line) {
    if (ch == separator) {
      fields.push_back(trim(field));
      field.clear();
    } else {
      field.push_back(ch);
    }
  }
  fields.push_back(trim(field));
  return fields;
}

bool is_header(const std::string& token) {
  if (token.empty()) {
    return true;
  }
  const unsigned char first = static_cast<unsigned char>(token[0]);
  return std::isdigit(first) == 0 && token[0] != '-' && token[0] != '+';
}

std::filesystem::path resolve_directory(const std::string& raw_path) {
  const std::filesystem::path requested{raw_path};
  if (std::filesystem::exists(requested)) {
    return requested;
  }

  const std::filesystem::path prefixed = std::filesystem::path("PAA-T01") / requested;
  if (std::filesystem::exists(prefixed)) {
    return prefixed;
  }

  return requested;
}

std::vector<std::vector<std::string>> read_table(const std::filesystem::path& file) {
  std::ifstream input(file);
  if (!input) {
    throw std::runtime_error("Não foi possível abrir " + file.string());
  }

  std::vector<std::vector<std::string>> rows;
  std::string line;
  bool skipped_header = false;
  while (std::getline(input, line)) {
    const auto comment = line.find('#');
    if (comment != std::string::npos) {
      line = line.substr(0, comment);
    }
    line = trim(line);
    if (line.empty()) {
      continue;
    }

    const auto fields = split_fields(line);
    if (!skipped_header && is_header(fields.front())) {
      skipped_header = true;
      continue;
    }
    rows.push_back(fields);
  }

  return rows;
}

void require_columns(const std::vector<std::string>& row, std::size_t expected, const std::string& file) {
  if (row.size() != expected) {
    throw std::runtime_error(
      file + ": linha com " + std::to_string(row.size()) + " colunas; esperado " + std::to_string(expected));
  }
}

double parse_number(const std::string& text, const std::string& file) {
  try {
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size()) {
      throw std::invalid_argument("sobra");
    }
    return value;
  } catch (const std::exception&) {
    throw std::runtime_error(file + ": número inválido \"" + text + "\"");
  }
}

int parse_id(const std::string& text, const std::string& file) {
  const double value = parse_number(text, file);
  if (value != static_cast<int>(value)) {
    throw std::runtime_error(file + ": identificador inválido \"" + text + "\"");
  }
  return static_cast<int>(value);
}

}

Instance load_instance(const std::string& path) {
  const std::filesystem::path directory = resolve_directory(path);
  if (!std::filesystem::is_directory(directory)) {
    throw std::runtime_error(
      "Pasta não encontrada: " + path
      + ". Esperado um diretório com produtos.txt, posicoes.txt, distancias.txt, coocorrencia.txt e pesos.txt.");
  }

  const auto products_file = directory / "produtos.txt";
  const auto slots_file = directory / "posicoes.txt";
  const auto distances_file = directory / "distancias.txt";
  const auto cooccurrence_file = directory / "coocorrencia.txt";
  const auto weights_file = directory / "pesos.txt";

  const auto product_rows = read_table(products_file);
  const auto slot_rows = read_table(slots_file);
  const auto distance_rows = read_table(distances_file);
  const auto cooccurrence_rows = read_table(cooccurrence_file);
  const auto weight_rows = read_table(weights_file);

  if (product_rows.empty()) {
    throw std::runtime_error("produtos.txt sem registros.");
  }
  if (slot_rows.empty()) {
    throw std::runtime_error("posicoes.txt sem registros.");
  }
  if (weight_rows.size() != 1) {
    throw std::runtime_error("pesos.txt deve ter uma linha com alfa e beta.");
  }

  Instance instance;
  instance.name = directory.generic_string();
  std::map<int, int> product_index;
  std::map<int, int> slot_index;

  for (const auto& row : product_rows) {
    require_columns(row, 4, "produtos.txt");
    Product product;
    product.id = parse_id(row[0], "produtos.txt");
    product.name = row[1];
    product.volume = parse_number(row[2], "produtos.txt");
    product.frequency = parse_number(row[3], "produtos.txt");
    if (product.volume < 0 || product.frequency < 0) {
      throw std::runtime_error("produtos.txt: volume e frequência não podem ser negativos.");
    }
    if (!product_index.emplace(product.id, static_cast<int>(instance.products.size())).second) {
      throw std::runtime_error("produtos.txt: id repetido " + std::to_string(product.id));
    }
    instance.products.push_back(product);
  }

  for (const auto& row : slot_rows) {
    require_columns(row, 3, "posicoes.txt");
    Slot slot;
    slot.id = parse_id(row[0], "posicoes.txt");
    slot.capacity = parse_number(row[1], "posicoes.txt");
    slot.access_cost = parse_number(row[2], "posicoes.txt");
    if (slot.capacity < 0 || slot.access_cost < 0) {
      throw std::runtime_error("posicoes.txt: capacidade e custo não podem ser negativos.");
    }
    if (!slot_index.emplace(slot.id, static_cast<int>(instance.slots.size())).second) {
      throw std::runtime_error("posicoes.txt: id repetido " + std::to_string(slot.id));
    }
    instance.slots.push_back(slot);
  }

  const int product_count = static_cast<int>(instance.products.size());
  const int slot_count = static_cast<int>(instance.slots.size());
  instance.distance.assign(slot_count, std::vector<double>(slot_count, 0));
  instance.cooccurrence.assign(product_count, std::vector<double>(product_count, 0));
  std::vector<std::vector<bool>> distance_filled(slot_count, std::vector<bool>(slot_count, false));

  for (const auto& row : distance_rows) {
    require_columns(row, 3, "distancias.txt");
    const int from_id = parse_id(row[0], "distancias.txt");
    const int to_id = parse_id(row[1], "distancias.txt");
    const double distance = parse_number(row[2], "distancias.txt");
    if (distance < 0) {
      throw std::runtime_error("distancias.txt: distância negativa.");
    }
    if (from_id == to_id) {
      throw std::runtime_error("distancias.txt: distância de uma posição para ela mesma.");
    }

    const auto from = slot_index.find(from_id);
    const auto to = slot_index.find(to_id);
    if (from == slot_index.end() || to == slot_index.end()) {
      throw std::runtime_error("distancias.txt: posição desconhecida " + row[0] + " ou " + row[1]);
    }

    const int i = from->second;
    const int j = to->second;
    if (distance_filled[i][j] && instance.distance[i][j] != distance) {
      throw std::runtime_error("distancias.txt: par de posições repetido com distância diferente.");
    }
    instance.distance[i][j] = distance;
    instance.distance[j][i] = distance;
    distance_filled[i][j] = true;
    distance_filled[j][i] = true;
  }

  for (int i = 0; i < slot_count; ++i) {
    for (int j = i + 1; j < slot_count; ++j) {
      if (!distance_filled[i][j]) {
        throw std::runtime_error(
          "distancias.txt: falta o par "
          + std::to_string(instance.slots[i].id) + " e " + std::to_string(instance.slots[j].id));
      }
    }
  }

  for (const auto& row : cooccurrence_rows) {
    require_columns(row, 3, "coocorrencia.txt");
    const int first_id = parse_id(row[0], "coocorrencia.txt");
    const int second_id = parse_id(row[1], "coocorrencia.txt");
    const double times = parse_number(row[2], "coocorrencia.txt");
    if (times < 0) {
      throw std::runtime_error("coocorrencia.txt: contagem negativa.");
    }
    if (first_id == second_id) {
      throw std::runtime_error("coocorrencia.txt: produto pareado com ele mesmo.");
    }

    const auto first = product_index.find(first_id);
    const auto second = product_index.find(second_id);
    if (first == product_index.end() || second == product_index.end()) {
      throw std::runtime_error("coocorrencia.txt: produto desconhecido " + row[0] + " ou " + row[1]);
    }

    const int i = first->second;
    const int j = second->second;
    if (instance.cooccurrence[i][j] != 0 && instance.cooccurrence[i][j] != times) {
      throw std::runtime_error("coocorrencia.txt: par de produtos repetido com valor diferente.");
    }
    instance.cooccurrence[i][j] = times;
    instance.cooccurrence[j][i] = times;
  }

  require_columns(weight_rows.front(), 2, "pesos.txt");
  instance.alpha = parse_number(weight_rows.front()[0], "pesos.txt");
  instance.beta = parse_number(weight_rows.front()[1], "pesos.txt");
  if (instance.alpha < 0 || instance.beta < 0) {
    throw std::runtime_error("pesos.txt: alfa e beta não podem ser negativos.");
  }

  instance.loaded = true;
  return instance;
}

}
