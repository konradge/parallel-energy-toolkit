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

double worker_function(size_t worker_number, size_t total_workers) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<double> dis(0.0, 1.0);

  long long hits = 0;

  long long iterations = precision / total_workers;
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
  precision = (argc > 1) ? std::stoul(argv[1]) : 200000;
  std::cout << "Estimating Pi with 'precision'" << precision << std::endl;
  ParallelToolkit::benchmark(worker_function, average, 20, 16, "results/monte_carlo_" + std::to_string(precision) + ".csv");
    auto res = ParallelToolkit::run(worker_function, average, 8);
    std::cout << "Estimated Pi: " << std::setprecision(10) << res <<
    std::endl;
  return 0;
}