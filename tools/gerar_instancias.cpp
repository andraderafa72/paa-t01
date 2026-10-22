#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

class Random {
public:
  explicit Random(std::uint32_t seed) : m_state(seed == 0 ? 1 : seed) {}

  int next_int(int low, int high) {
    const std::uint32_t span = static_cast<std::uint32_t>(high - low + 1);
    return low + static_cast<int>(next() % span);
  }

private:
  std::uint32_t next() {
    m_state = m_state * 1664525u + 1013904223u;
    return m_state;
  }

  std::uint32_t m_state;
};

void write_instance(const std::filesystem::path& root, int product_count) {
  const int slot_count = product_count + product_count / 5;
  const std::filesystem::path directory = root / ("n" + std::to_string(product_count));
  std::filesystem::create_directories(directory);
  Random random(21);

  std::ofstream products(directory / "produtos.txt");
  products << "# id; nome; volume total; frequencia. Volume de 2 a 12.\n";
  products << "id;nome;volume;frequencia\n";
  for (int id = 1; id <= product_count; ++id) {
    const int volume = random.next_int(2, 12);
    const int frequency = random.next_int(1, 180);
    products << id << ";Produto " << id << ";" << volume << ";" << frequency << "\n";
  }

  std::ofstream slots(directory / "posicoes.txt");
  slots << "# id; capacidade; custo de acesso. Capacidade sempre comporta qualquer produto.\n";
  slots << "id;capacidade;custo_acesso\n";
  for (int id = 1; id <= slot_count; ++id) {
    const int capacity = 20 + (id % 5);
    const int access = 1 + (id - 1) / 2;
    slots << id << ";" << capacity << ";" << access << "\n";
  }

  std::ofstream distances(directory / "distancias.txt");
  distances << "# origem; destino; distancia |destino-origem|\n";
  distances << "origem;destino;distancia\n";
  for (int from = 1; from <= slot_count; ++from) {
    for (int to = from + 1; to <= slot_count; ++to) {
      distances << from << ";" << to << ";" << (to - from) << "\n";
    }
  }

  std::ofstream together(directory / "coocorrencia.txt");
  together << "# produto_a; produto_b; vezes. Pares i+1, i+3, i+7 e i+12.\n";
  together << "produto_a;produto_b;vezes\n";
  const int steps[] = {1, 3, 7, 12};
  for (int id = 1; id <= product_count; ++id) {
    for (int step : steps) {
      const int other = id + step;
      if (other > product_count) {
        continue;
      }
      together << id << ";" << other << ";" << random.next_int(2, 40) << "\n";
    }
  }

  std::ofstream weights(directory / "pesos.txt");
  weights << "# alfa; beta\n";
  weights << "alfa;beta\n";
  weights << "1;1\n";

  std::cout << directory.generic_string() << " produtos=" << product_count
            << " posicoes=" << slot_count << "\n";
}

}

int main() {
  std::filesystem::path root = "data";
  if (!std::filesystem::is_directory(root) && std::filesystem::is_directory("PAA-T01/data")) {
    root = "PAA-T01/data";
  }
  std::filesystem::create_directories(root);

  const int sizes[] = {20, 50, 100, 200};
  for (int size : sizes) {
    write_instance(root, size);
  }
  return 0;
}
