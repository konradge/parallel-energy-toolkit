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

size_t thread_function(size_t thread_number, size_t total_threads) {
  return thread_number;
}

// function to combine results
void combine_function(size_t thread_count, size_t partial_result,
                      double& result) {
  result += (double)partial_result;
}

int main(int argc, char* argv[]) {
  benchmark(thread_function, combine_function, 20, 16);
  auto res = run(thread_function, combine_function, 8);
  std::cout << "Average is: " << std::setprecision(10) << res << std::endl;
  return 0;
}