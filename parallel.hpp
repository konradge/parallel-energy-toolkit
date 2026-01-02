#include <assert.h>
#include <fcntl.h>
#include <malloc.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <execution>
#include <fstream>
#include <future>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "benchmark.hpp"
#include "msr_reader.h"
#include "parallel_utils.hpp"
#include "thread_result.hpp"

template <typename ArgumentT, typename PartialResultT,
          typename ResultT = PartialResultT>
class ParallelProgram {
 public:
  void register_function(PartialResultT (*function)(ArgumentT),
                         ArgumentT argument) {
    this->partial_funcs.push_back({function, argument});
  }
  void benchmark(void (*reduce)(ResultT&, PartialResultT), size_t iterations);

 private:
  std::vector<std::pair<PartialResultT (*)(ArgumentT), ArgumentT>>
      partial_funcs;
};

template <typename ArgumentT, typename PartialResultT, typename ResultT>
void ParallelProgram<ArgumentT, PartialResultT, ResultT>::benchmark(
    void (*reduce)(ResultT&, PartialResultT), size_t iterations) {
  std::ofstream output_file("benchmark_results.csv");
  output_file << "iteration, runtime, energy, min_core_runtime, max_core_runtime, min_core_energy, max_core_energy\n";

  for (size_t i = 0; i < iterations; i++) {
    std::cout << "Benchmark iteration " << (i + 1) << " / " << iterations
              << std::endl;
    double energy = 0.0;
    double runtime = 0.0;
    Benchmark<ArgumentT, PartialResultT, ResultT> benchmark;
    benchmark.run(this->partial_funcs, reduce);

    output_file << (i + 1) << ", " << benchmark.csv();
  }

  output_file.close();
}