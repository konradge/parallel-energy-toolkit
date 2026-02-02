#include <chrono>
#include <format>
#include <future>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

#include "utils.hpp"
#include "../src/parallel.hpp"

int max;

bool is_prime(int n) {
  if(n <= 1) return false;
  for(int i = 2; i * i <= n; i++) {
    if(n % i == 0) return false;
  }
  return true;
}

// Each worker is responsible for all numbers i = j * worker_number
int worker_function(size_t worker_number, size_t total_workers) {
  int prime_count = 0;
  for(int i = worker_number; i < max; i += total_workers) {
    if(is_prime(i)) {
      prime_count++;
    }
  }
  return prime_count;
}

int main(int argc, char* argv[]) {
  max = (argc > 1) ? std::stoul(argv[1]) : 200000;
  std::cout << "Calculating number of primes less than " << max << std::endl;
  ParallelToolkit::benchmark(worker_function, sum, 20, 16, "results/prime_count_" + std::to_string(max) + ".csv");
  auto res = ParallelToolkit::run(worker_function, sum, 8);
  printf("\nNumber of primes less than %d is %d\n", max, res);
  return 0;
}