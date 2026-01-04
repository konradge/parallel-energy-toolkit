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

#include "msr_reader.h"
#include "parallel_utils.hpp"
#include "single_benchmark.hpp"
#include "thread_result.hpp"

template <typename PartialResultT>
using ThreadFunctionT = PartialResultT (*)(size_t thread_id,
                                           size_t total_threads);
template <typename PartialResultT, typename ResultT = PartialResultT>
using CombineFunctionT = void (*)(size_t thread_count,
                                  PartialResultT partial_result,
                                  ResultT& result);

/**
 * Runs a benchmark with the given function
 *
 * @param distribution_function Function to distribute work among cores
 * @param core_function Function to be executed on each core
 * @param reduce_function Function to combine results from each core
 * @param sample_size Number of benchmark iterations
 * @param max_threads Maximum number of threads to use
 */
template <typename PartialResultT, typename ResultT = PartialResultT>
void benchmark(ThreadFunctionT<PartialResultT> thread_function,
               CombineFunctionT<PartialResultT, ResultT> combine_function,
               size_t sample_size, size_t max_threads);

template <typename PartialResultT, typename ResultT = PartialResultT>
ResultT run(ThreadFunctionT<PartialResultT> thread_function,
            CombineFunctionT<PartialResultT, ResultT> combine_function,
            size_t thread_count);

struct BenchmarkResult {
  double runtime;
  double energy;
};
template <typename PartialResultT, typename ResultT = PartialResultT>
BenchmarkResult _benchmark(
    ThreadFunctionT<PartialResultT> core_function,
    CombineFunctionT<PartialResultT, ResultT> combine_function,
    size_t core_count);

/**
 * --------------- Implementations ---------------
 */

template <typename PartialResultT, typename ResultT>
void benchmark(ThreadFunctionT<PartialResultT> core_function,
               CombineFunctionT<PartialResultT, ResultT> combine_function,
               size_t sample_size, size_t max_cores) {
  std::cout << "Warm up..." << std::endl;
  // Warm up on (hopefully) all cores
  for (size_t i = 0; i < 10; i++) {
    run(core_function, combine_function, max_cores);
  }
  std::ofstream output_file("benchmark_results.csv");
  output_file << "core_count,runtime,energy\n";

  for (size_t core_count = 2; core_count <= max_cores; core_count++) {
    for (size_t iteration = 0; iteration < sample_size; iteration++) {
      std::cout << "Benchmark iteration " << (iteration + 1) << " / "
                << sample_size << " on " << core_count << " / " << max_cores
                << " cores" << std::endl;
      auto res = _benchmark(core_function, combine_function, core_count);

      output_file << core_count << "," << res.runtime << "," << res.energy
                  << "\n";

      // output_file << (i + 1) << ", " << benchmark.csv();
    }
  }

  output_file.close();
}

template <typename PartialResultT, typename ResultT>
ResultT run(ThreadFunctionT<PartialResultT> thread_function,
            CombineFunctionT<PartialResultT, ResultT> combine_function,
            size_t thread_count) {
  std::vector<std::future<PartialResultT>> threads;
  threads.reserve(thread_count);

  ResultT res;
  for (size_t thread = 0; thread < thread_count; thread++) {
    threads.push_back(std::async(thread_function, thread, thread_count));
  }

  for (size_t i = 0; i < threads.size(); i++) {
    combine_function(thread_count, threads[i].get(), res);
  }
  return res;
}

template <typename PartialResultT, typename ResultT>
BenchmarkResult _benchmark(
    ThreadFunctionT<PartialResultT> thread_function,
    CombineFunctionT<PartialResultT, ResultT> combine_function,
    size_t thread_count) {
  // start threads
  std::vector<std::future<ThreadResult<PartialResultT>>> threads;
  threads.reserve(thread_count);
  auto start_time = std::chrono::high_resolution_clock::now();

  for (size_t thread = 0; thread < thread_count; thread++) {
    threads.push_back(std::async(calculate<PartialResultT>, thread_function,
                                 thread, thread_count));
  }

  // read results (synchronously wait for threads)
  std::vector<ThreadResult<PartialResultT>> results(threads.size());
  ResultT res;
  for (size_t i = 0; i < threads.size(); i++) {
    results[i] = threads[i].get();

    // reduce to final result (dicard for benchmark)
    combine_function(thread_count, results[i].result, res);
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  duration elapsed = end_time - start_time;

  BenchmarkResult benchmark_result;
  // Since the parent thread takes the longest time, we use its time as the
  // total runtime
  benchmark_result.runtime = elapsed.count();

  // Group thread results by core they were calculated on
  std::unordered_map<int, std::vector<ThreadResult<PartialResultT>>>
      result_by_core;
  for (const auto& result : results) {
    if (result_by_core.find(result.core_id) == result_by_core.end()) {
      result_by_core[result.core_id] =
          std::vector<ThreadResult<PartialResultT>>();
    }
    result_by_core[result.core_id].push_back(result);
  }

  for (const auto& [core_id, core_results] : result_by_core) {
    // sort core_results by start_time
    std::vector<ThreadResult<PartialResultT>> sorted_core_results =
        core_results;
    std::sort(sorted_core_results.begin(), sorted_core_results.end(),
              [](const ThreadResult<PartialResultT>& a,
                 const ThreadResult<PartialResultT>& b) {
                return a.start_time < b.start_time;
              });
    duration core_time = duration::zero();
    double core_energy = 0.0;
    ThreadResult<PartialResultT> last_result = sorted_core_results[0];
    double start_energy = last_result.start_energy;
    double end_energy = last_result.end_energy;
    auto start_time = last_result.start_time;
    auto end_time = last_result.end_time;
    bool last_added = false;
    for (size_t i = 1; i < core_results.size(); i++) {
      auto current_result = sorted_core_results[i];
      if (last_result.end_time > current_result.start_time) {
        /**
         * No overlap, take the energy at the end of the last result:
         *
         * <-----i-1----->
         *                  <-----i----->
         */
        core_energy += last_result.end_energy - start_energy;
        start_energy = current_result.start_energy;
        core_time.operator+=(last_result.end_time - start_time);
        start_time = current_result.end_time;
        last_added = true;
      } else {
        /**
         * Overlap, one of the following cases:
         *
         * <------i-1------>
         *            <------i------->
         * or:
         * <------i-1----------->
         *     <---i--->
         */
        end_time = std::max(end_time, current_result.end_time);
        end_energy = std::max(end_energy, current_result.end_energy);
      }
    }
    if (!last_added) {
      core_energy += end_energy - start_energy;
      core_time += (end_time - start_time);
    }

    benchmark_result.energy += core_energy;
  }

  return benchmark_result;
}