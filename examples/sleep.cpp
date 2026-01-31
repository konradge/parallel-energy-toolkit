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

int millis;

void* calculate(size_t thread_number, size_t total_threads) {
  std::this_thread::sleep_for(std::chrono::milliseconds(millis));
  return nullptr;
}

// function to combine results
void reduce(size_t thread_count, void* partial_result,
                      double& result) {}

int main(int argc, char* argv[]) {
  millis = (argc > 1) ? std::stoul(argv[1]) : 1000;
  ParallelToolkit::benchmark(calculate, reduce, 10, 16, "results/sleep_" + std::to_string(millis) + ".csv");
  auto res = ParallelToolkit::run(calculate, reduce, 8);
  std::cout << "Average is: " << std::setprecision(10) << res << std::endl;
  return 0;
}