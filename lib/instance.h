#ifndef INSTANCE_H
#define INSTANCE_H

#include <string>
#include <vector>

namespace warehouse {

struct Product {
  int id = 0;
  std::string name;
  double volume = 0;
  double frequency = 0;
};

struct Slot {
  int id = 0;
  double capacity = 0;
  double access_cost = 0;
};

struct Instance {
  std::string name;
  std::vector<Product> products;
  std::vector<Slot> slots;
  std::vector<std::vector<double>> distance;
  std::vector<std::vector<double>> cooccurrence;
  double alpha = 1;
  double beta = 1;
  bool loaded = false;
};

Instance load_instance(const std::string& path);

}

#endif
