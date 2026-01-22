#include <chrono>
#include <format>
#include <future>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

#include "../src/parallel.hpp"
#include "utils.hpp"

int precision = 20000000;

double core_function(size_t thread_number, size_t total_threads) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<double> dis(0.0, 1.0);

  long long hits = 0;

  long long iterations = precision / total_threads;
  for (long long i = 0; i < iterations; ++i) {
    double x = dis(gen);
    double y = dis(gen);

    if (x * x + y * y <= 1.0) {
      hits++;
    }
  }
  return 4.0 * hits / iterations;
}


int main(int argc, char* argv[]) {
  benchmark(core_function, average, 20, 16, argv[1]);
    auto res = run(core_function, average, 8);
    std::cout << "Estimated Pi: " << std::setprecision(10) << res <<
    std::endl;
  return 0;
}