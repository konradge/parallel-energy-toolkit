#include <iostream>
#include <iomanip>

#include "utils.hpp"
#include "../src/parallel.hpp"

size_t array_size = 10000;
int* array;

long long thread_function(size_t thread_number, size_t total_threads) {
  size_t chunk_start = (array_size / total_threads) * thread_number;
  size_t chunk_end = (thread_number == total_threads - 1)
                         ? array_size
                         : (array_size / total_threads) * (thread_number + 1);
  long long sum = 0;
  for (size_t i = chunk_start; i < chunk_end; i++) {
    sum += array[i];
  }

  return sum;
}

int main(int argc, char* argv[]) {
  array = create_random_int_vector(array_size, 1, 100);
  std::cout << "Start benchmarking..." << std::endl;
  benchmark(thread_function, average<long long>, 20, 50, argv[1]);
  auto res = run(thread_function, average<long long>, 8);
  std::cout << "Average is: " << std::setprecision(10) << res << std::endl;
  return 0;
}