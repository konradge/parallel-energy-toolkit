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

#include "measurement.hpp"
#include "msr_reader.h"

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
               size_t sample_size, size_t max_threads, std::string output_file_name);

template <typename PartialResultT, typename ResultT = PartialResultT>
ResultT run(ThreadFunctionT<PartialResultT> thread_function,
            CombineFunctionT<PartialResultT, ResultT> combine_function,
            size_t thread_count);

struct BenchmarkResult {
  double runtime;
  double parallel_energy;
  double combine_energy;
  size_t distinct_used_cores;
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
               size_t sample_size, size_t max_cores, std::string output_file_name) {
  // Warm up on (hopefully) all cores
  std::cout << "Warming up...\n";
  for (size_t i = 0; i < 10; i++) {
    // std::cout << i << std::endl;
    std::cout << "(1) Warmup: Iteration " << (i + 1) << " / " << 10 << "\r" << std::flush;
    run(core_function, combine_function, max_cores);
  }
  std::cout << "\n";
  std::ofstream output_file(output_file_name);
  output_file
      << "core_count,distinct_cores,runtime,energy\n";

  for (size_t core_count = 2; core_count <= max_cores; core_count++) {
    for (size_t iteration = 0; iteration < sample_size; iteration++) {
      std::cout << "(2) Benchmark: Iteration " << (iteration + 1) << " / "
                << sample_size << " on " << core_count << " / " << max_cores
                << " threads\r" << std::flush;
      auto res = _benchmark(core_function, combine_function, core_count);

      output_file << core_count << "," << res.distinct_used_cores << ","
                  << res.runtime << "," << res.parallel_energy + res.combine_energy << "\n" << std::flush;
    }
  }
  std::cout << "\n";

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

template <typename PartialResultT>
struct ThreadMeasurementWithResult : public TimeEnergyMeasurement {
  PartialResultT result;
};

template <typename PartialResultT>
ThreadMeasurementWithResult<PartialResultT> measure_and_calculate(
    ThreadFunctionT<PartialResultT> thread_function, size_t thread_id,
    size_t thread_count) {
  ThreadMeasurementWithResult<PartialResultT> measurement;
  measurement.start(thread_id);
  measurement.result = thread_function(thread_id, thread_count);
  measurement.stop();
  return measurement;
}

template <typename PartialResultT, typename ResultT>
BenchmarkResult _benchmark(
    ThreadFunctionT<PartialResultT> thread_function,
    CombineFunctionT<PartialResultT, ResultT> combine_function,
    size_t thread_count) {
  // start threads
  std::vector<std::future<ThreadMeasurementWithResult<PartialResultT>>> threads;
  threads.reserve(thread_count);
  TimeMeasurement parent_time;
  parent_time.start();

  for (size_t thread = 0; thread < thread_count; thread++) {
    threads.push_back(std::async(measure_and_calculate<PartialResultT>,
                                 thread_function, thread, thread_count));
  }

  // wait for results
  std::vector<ThreadMeasurementWithResult<PartialResultT>> results(
      threads.size());
  ResultT res;
  for (size_t i = 0; i < threads.size(); i++) {
    results[i] = threads[i].get();
  }

  TimeEnergyMeasurement combine_energy;
  combine_energy.start(0);
  for (size_t i = 0; i < results.size(); i++) {
    // reduce to final result (dicard for benchmark)
    combine_function(thread_count, results[i].result, res);
  }
  combine_energy.stop();
  parent_time.stop();

  BenchmarkResult benchmark_result;
  // Since the parent thread takes the longest time, we use its time as the
  // total runtime
  benchmark_result.runtime = parent_time.time();
  // The energy for dispatching the threads is not measured to to unreliability
  benchmark_result.parallel_energy = 0;
  benchmark_result.combine_energy = combine_energy.energy();

  // Group thread results by core they were calculated on
  std::unordered_map<int,
                     std::vector<ThreadMeasurementWithResult<PartialResultT>>>
      result_by_core;
  for (const auto& result : results) {
    if (result_by_core.find(result.core_id) == result_by_core.end()) {
      result_by_core[result.core_id] =
          std::vector<ThreadMeasurementWithResult<PartialResultT>>();
    }
    result_by_core[result.core_id].push_back(result);
  }
  benchmark_result.distinct_used_cores = result_by_core.size();

  for (const auto& [core_id, core_results] : result_by_core) {
    // sort core_results by start_time
    std::vector<ThreadMeasurementWithResult<PartialResultT>>
        sorted_core_results = core_results;
    std::sort(sorted_core_results.begin(), sorted_core_results.end(),
              [](const ThreadMeasurementWithResult<PartialResultT>& a,
                 const ThreadMeasurementWithResult<PartialResultT>& b) {
                return a.start_time < b.start_time;
              });
    duration core_time = duration::zero();
    double core_energy = 0.0;
    ThreadMeasurementWithResult<PartialResultT> last_result =
        sorted_core_results[0];
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

    benchmark_result.parallel_energy += core_energy;
  }

  return benchmark_result;
}