#pragma once

#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <vector>

#include "measurement.hpp"

namespace ParallelToolkit {

/**
 * --------------- Type Definitions ---------------
 */

template <typename PartialResultT>
using ThreadFunctionT = PartialResultT (*)(size_t thread_id, size_t total_threads);

template <typename PartialResultT, typename ResultT = PartialResultT>
using ReduceFunctionT = void (*)(size_t thread_count, 
                                 PartialResultT partial_result, 
                                 ResultT& result);

/**
 * --------------- Public Interface ---------------
 */

/**
 * Runs a benchmark with the given function, outputting the results to a CSV file.
 *
 * @param thread_function Function to be executed in parallel.
 * @param reduce_function Function to combine results from each core.
 * @param sample_size Number of benchmark iterations for each thread count.
 * @param max_thread_count Maximum number of threads to use (executes 2..max).
 * @param output_file_name CSV file path for results.
 */
template <typename PartialResultT, typename ResultT = PartialResultT>
void benchmark(ThreadFunctionT<PartialResultT> thread_function,
               ReduceFunctionT<PartialResultT, ResultT> reduce_function,
               size_t sample_size, 
               size_t max_thread_count,
               std::string output_file_name);

/**
 * Runs the given function in parallel using the specified number of threads.
 *
 * @return The combined result from all threads.
 */
template <typename PartialResultT, typename ResultT = PartialResultT>
ResultT run(ThreadFunctionT<PartialResultT> thread_function,
            ReduceFunctionT<PartialResultT, ResultT> reduce_function,
            size_t thread_count);

/**
 * --------------- Internal Logic ---------------
 */

namespace _Internal {

struct BenchmarkResult {
  double runtime;
  double parallel_energy;
  double combine_energy;
  size_t distinct_used_machine_threads;
};

template <typename PartialResultT, typename ResultT = PartialResultT>
BenchmarkResult _single_benchmark(ThreadFunctionT<PartialResultT> core_function,
                                  ReduceFunctionT<PartialResultT, ResultT> reduce_function,
                                  size_t thread_count);

double _calculate_energy_consumptions(std::vector<TimeEnergyMeasurement*> results,
                                      size_t& distinct_used_machine_threads);

}  // namespace _Internal

/**
 * --------------- Implementations ---------------
 */

template <typename PartialResultT, typename ResultT>
void benchmark(ThreadFunctionT<PartialResultT> core_function,
               ReduceFunctionT<PartialResultT, ResultT> reduce_function,
               size_t sample_size, 
               size_t max_thread_count,
               std::string output_file_name) {
  // Warm up on all cores
  std::cout << "Warming up...\n";
  for (size_t i = 0; i < 10; i++) {
    std::cout << "(1) Warmup: Iteration " << (i + 1) << " / 10\r" << std::flush;
    run(core_function, reduce_function, max_thread_count);
  }
  std::cout << "\n";

  std::ofstream output_file(output_file_name);
  output_file << "thread_count,distinct_threads,runtime,energy\n";

  for (size_t thread_count = 2; thread_count <= max_thread_count; thread_count++) {
    for (size_t iteration = 0; iteration < sample_size; iteration++) {
      std::cout << "(2) Benchmark: Iteration " << (iteration + 1) << " / " << sample_size
                << " on " << thread_count << " / " << max_thread_count << " threads\r"
                << std::flush;

      auto res = _Internal::_single_benchmark(core_function, reduce_function, thread_count);

      output_file << thread_count << "," 
                  << res.distinct_used_machine_threads << "," 
                  << res.runtime << ","
                  << res.parallel_energy + res.combine_energy << "\n"
                  << std::flush;
    }
  }
  std::cout << "\n";
  output_file.close();
}

template <typename PartialResultT, typename ResultT>
ResultT run(ThreadFunctionT<PartialResultT> thread_function,
            ReduceFunctionT<PartialResultT, ResultT> reduce_function,
            size_t thread_count) {
  std::vector<std::future<PartialResultT>> threads;
  threads.reserve(thread_count);

  ResultT res;
  for (size_t thread = 0; thread < thread_count; thread++) {
    threads.push_back(std::async(thread_function, thread, thread_count));
  }

  for (size_t i = 0; i < threads.size(); i++) {
    reduce_function(thread_count, threads[i].get(), res);
  }
  return res;
}

template <typename PartialResultT>
struct ThreadMeasurementWithResult : public TimeEnergyMeasurement {
  PartialResultT result;
};

template <typename PartialResultT>
ThreadMeasurementWithResult<PartialResultT> measure_and_calculate(
    ThreadFunctionT<PartialResultT> thread_function, 
    size_t thread_id,
    size_t thread_count) {
  ThreadMeasurementWithResult<PartialResultT> measurement;
  measurement.start(thread_id);
  measurement.result = thread_function(thread_id, thread_count);
  measurement.stop();
  return measurement;
}

template <typename PartialResultT, typename ResultT>
_Internal::BenchmarkResult _Internal::_single_benchmark(
    ThreadFunctionT<PartialResultT> thread_function,
    ReduceFunctionT<PartialResultT, ResultT> reduce_function,
    size_t thread_count) {
  std::vector<std::future<ThreadMeasurementWithResult<PartialResultT>>> threads;
  threads.reserve(thread_count);
  
  TimeMeasurement parent_time;
  parent_time.start();

  // 1. Launch threads
  for (size_t thread = 0; thread < thread_count; thread++) {
    threads.push_back(std::async(measure_and_calculate<PartialResultT>, 
                                 thread_function, thread, thread_count));
  }

  // 2. Wait for partial results
  std::vector<ThreadMeasurementWithResult<PartialResultT>> results(threads.size());
  std::vector<TimeEnergyMeasurement*> results_ptrs(threads.size());
  ResultT res;
  for (size_t i = 0; i < threads.size(); i++) {
    results[i] = threads[i].get();
    results_ptrs[i] = &results[i];
  }

  // 3. Reduce results
  TimeEnergyMeasurement combine_energy;
  combine_energy.start(0);
  for (size_t i = 0; i < results.size(); i++) {
    reduce_function(thread_count, results[i].result, res);
  }
  combine_energy.stop();
  parent_time.stop();

  BenchmarkResult benchmark_result;
  // Since the parent thread takes the longest time, we use its time as the
  // total runtime
  benchmark_result.runtime = parent_time.time();
  benchmark_result.combine_energy = combine_energy.energy();

  benchmark_result.parallel_energy = _calculate_energy_consumptions(
      results_ptrs, benchmark_result.distinct_used_machine_threads);

  return benchmark_result;
}

}  // namespace ParallelToolkit