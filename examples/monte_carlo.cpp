#include <chrono>
#include <format>
#include <future>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

#include "../parallel.hpp"

int precision = 10000000;

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

// function to combine results
void reduce(size_t thread_count, double partial_result, double& total) {
  total += partial_result / thread_count;
}

int main(int argc, char* argv[]) {
  benchmark(core_function, reduce, 20, 16);
    auto res = run(core_function, reduce, 8);
    std::cout << "Estimated Pi: " << std::setprecision(10) << res <<
    std::endl;
  return 0;
}