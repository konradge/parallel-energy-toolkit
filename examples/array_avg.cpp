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

size_t array_size = 1000000000;
int* array;

long long core_function(size_t thread_number, size_t total_threads) {
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

// function to combine results
void reduce(size_t thread_count, long long partial_result, double& total) {
  total += (double) partial_result / array_size;
}

int* create_random_int_vector(int n, int min_val, int max_val) {
    // 1. Create a vector of size n
    int* vec = new int[n];

    // 2. Setup the random number generation
    // Use std::random_device to generate a non-deterministic seed (best practice)
    std::random_device rd;

    // Use a Mersenne Twister engine, seeded with the random_device
    std::mt19937 gen(rd());

    // Define a uniform distribution for integers in the desired range [min_val, max_val]
    std::uniform_int_distribution<> distrib(min_val, max_val);

    // 3. Fill the vector
    // Use std::generate to fill the vector with random numbers from the distribution
    std::generate(vec, vec + n, [&]() {
        return distrib(gen);
    });

    return vec;
}

int main(int argc, char* argv[]) {
  array = create_random_int_vector(array_size, 1, 100);
  std::cout << "Start benchmarking..." << std::endl;
  benchmark(core_function, reduce, 20, 16);
  auto res = run(core_function, reduce, 8);
  std::cout << "Average is: " << std::setprecision(10) << res << std::endl;
  return 0;
}